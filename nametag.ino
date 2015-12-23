#include <stdio.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include "TimerOne.h"
#include "k3ngdisplay_nametag.h"

// K3NG Hamfest Nametag
//
// Copyright 2015 Anthony Good, K3NG
// All trademarks referred to in source code and documentation are copyright their respective owners.

    /*

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    */

// Are you a radio artisan ?



#define CODE_VERSION "2015122001"

// #define OPTION_CHANGE_LCD_BACKLIGHT_WITH_CW
// #define DEBUG



#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

#define sidetone_line 0
#define key_line A0

#define initial_speed_wpm 25             // keyer speed setting
#define initial_sidetone_freq 1000        // sidetone frequency setting
#define char_send_buffer_size 50
#define element_send_buffer_size 20
#define default_length_letterspace 3
#define default_length_wordspace 7
#define repeat_cw_ms 10000


enum unit_mode_type {OFFLINE, MAINTENANCE, BEACON, TEST};
enum key_scheduler_type {IDLE, KEY_DOWN, KEY_UP};
enum sidetone_mode_type {SIDETONE_OFF, SIDETONE_ON};
enum char_send_mode_type {CW, HELL};
enum sending_tye {AUTOMATIC_SENDING, MANUAL_SENDING};
enum element_buffer_type {HALF_UNIT_KEY_UP, ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP, THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP,ONE_UNIT_KEYDOWN_3_UNITS_KEY_UP, THREE_UNIT_KEYDOWN_3_UNITS_KEY_UP,
  ONE_UNIT_KEYDOWN_7_UNITS_KEY_UP, THREE_UNIT_KEYDOWN_7_UNITS_KEY_UP, SEVEN_UNITS_KEY_UP, KEY_UP_LETTERSPACE_MINUS_1, KEY_UP_WORDSPACE_MINUS_4, KEY_UP_WORDSPACE};

#define SERIAL_SEND_BUFFER_WPM_CHANGE 200
#define SERIAL_SEND_BUFFER_PTT_ON 201
#define SERIAL_SEND_BUFFER_PTT_OFF 202
#define SERIAL_SEND_BUFFER_TIMED_KEY_DOWN 203
#define SERIAL_SEND_BUFFER_TIMED_WAIT 204
#define SERIAL_SEND_BUFFER_NULL 205
#define SERIAL_SEND_BUFFER_PROSIGN 206
#define SERIAL_SEND_BUFFER_HOLD_SEND 207
#define SERIAL_SEND_BUFFER_HOLD_SEND_RELEASE 208
#define SERIAL_SEND_BUFFER_MEMORY_NUMBER 210

#define SERIAL_SEND_BUFFER_NORMAL 0
#define SERIAL_SEND_BUFFER_TIMED_COMMAND 1
#define SERIAL_SEND_BUFFER_HOLD 2

#define NORMAL 0
#define OMIT_LETTERSPACE 1

#define LCD_ROWS 2
#define LCD_COLUMNS 16
#define LCD_UPDATE_TIME 2000

#define DISPLAY_NO_FORCED_UPDATE 0


volatile byte key_scheduler_state = IDLE;
volatile unsigned long next_key_scheduler_transition_time = 0;
unsigned int key_scheduler_keyup_ms;
unsigned int key_scheduler_keydown_ms;
unsigned int wpm = initial_speed_wpm;
byte key_state = 0;
byte sidetone_mode = SIDETONE_ON;
unsigned int hz_sidetone = initial_sidetone_freq;
byte char_send_mode = CW;
byte length_letterspace = default_length_letterspace;
byte length_wordspace = default_length_wordspace;
byte last_sending_type = MANUAL_SENDING;
byte pause_sending_buffer = 0;
byte char_send_buffer_array[char_send_buffer_size];
byte char_send_buffer_bytes = 0;
byte char_send_buffer_status = SERIAL_SEND_BUFFER_NORMAL;
byte element_send_buffer_array[element_send_buffer_size];
byte element_send_buffer_bytes = 0;
unsigned long last_cw_send_time = 0;
unsigned long last_backlight_change = 0;
byte backlight_color = 1;
byte display_sequence = 1;
byte currently_displayed_sequence = 0;
unsigned long next_display_sequence_time = 0;
byte displaybacklightred = 0;

K3NGdisplay k3ngdisplay(LCD_COLUMNS,LCD_ROWS,LCD_UPDATE_TIME);

void setup() {
  
  initialize_pins();   
  k3ngdisplay.initialize(); 
  k3ngdisplay.setBacklight(WHITE);
  k3ngdisplay.clear();
  k3ngdisplay.print_center_timed_message("K3NG Nametag",3000);
  sidetone_mode = SIDETONE_ON;
  TWBR = ((F_CPU / 800000L) - 16) / 2;  // speed up the I2C interface from 100 kHz to 800 kHz
  Timer1.initialize(1000);
  Timer1.attachInterrupt(service_key_scheduler);

}

void loop() {
 
  if ((keyer_is_idle()) || (!(keyer_is_idle()) && (next_key_scheduler_transition_time - millis()) > 50)){ // it takes about 50 mS to do I2C LCD updates
    k3ngdisplay.service(DISPLAY_NO_FORCED_UPDATE);
  }

  if (millis() > next_display_sequence_time){
    display_sequence++;
    if (display_sequence > 4){display_sequence = 1;}
  }

  if (display_sequence != currently_displayed_sequence){
    switch(display_sequence){
      case 1:
        k3ngdisplay.print_center_entire_row("K3NG",0,0);
        k3ngdisplay.print_center_entire_row("radioartisan.com",1,0);
        break;
      case 2:
        k3ngdisplay.print_center_entire_row("Anthony Good",0,0);
        k3ngdisplay.print_center_entire_row("K3NG",1,0);
        break;
      case 3:
        k3ngdisplay.print_center_entire_row("Anthony Good",0,0);
        k3ngdisplay.print_center_entire_row("\"Goody\" K3NG",1,0);
        break; 
      case 4:
        k3ngdisplay.print_center_entire_row("code available @",0,0);
        k3ngdisplay.print_center_entire_row("radioartisan.com",1,0);
        break;               
    }
    currently_displayed_sequence = display_sequence;
    next_display_sequence_time = millis() + 3000;
  }

   
  if (((millis() - last_cw_send_time ) > repeat_cw_ms) && (keyer_is_idle())){
    send_character_string("CQ DE K3NG K3NG");
    last_cw_send_time = millis();
  }


  #if !defined(OPTION_CHANGE_LCD_BACKLIGHT_WITH_CW)
    if (((millis() - last_backlight_change) > 1000) ){
      backlight_color++;
      if (backlight_color > 7){backlight_color = 1;}
      k3ngdisplay.setBacklight(backlight_color);
      last_backlight_change = millis();
    }
  #else // OPTION_CHANGE_LCD_BACKLIGHT_WITH_CW
    if (keyer_is_idle()){
      if (((millis() - last_backlight_change) > 1000) ){
        backlight_color++;
        if (backlight_color > 7){backlight_color = 1;}
        k3ngdisplay.setBacklight(backlight_color);
        last_backlight_change = millis();
      }
    } else {
      if ((key_state) && (!displaybacklightred)){
        k3ngdisplay.setBacklight(RED);
        displaybacklightred = 1;
      }
      if ((!key_state) && (displaybacklightred)){
        k3ngdisplay.setBacklight(GREEN);
        displaybacklightred = 0;
        last_backlight_change = millis();
      }    
    }
  #endif //OPTION_CHANGE_LCD_BACKLIGHT_WITH_CW

        
  //service_key_scheduler(); 
  service_element_send_buffer();
  service_char_send_buffer();
   
  
}



//-------------------------------------------------------------------------------------------------------
byte keyer_is_idle() {
  
  if ((!char_send_buffer_bytes) && (!element_send_buffer_bytes) && (key_scheduler_state == IDLE)) {
    return 1;
  } else {
    return 0;
  }
  
}


//-------------------------------------------------------------------------------------------------------
void send_character_string(char* string_to_send) {
  
  for (int x = 0;x < 32;x++) {
    if (string_to_send[x] != 0) {
      add_to_char_send_buffer(string_to_send[x]);
    } else {
      x = 33;
    }
  }
}



//-------------------------------------------------------------------------------------------------------

void tx_and_sidetone_key (int state)
{

  if ((state) && (key_state == 0)) {
    if (sidetone_mode == SIDETONE_ON){
      tone(sidetone_line, hz_sidetone);
      digitalWrite(13,HIGH);
      digitalWrite(key_line,LOW);
      
    }
    key_state = 1;
  } else {
    if ((state == 0) && (key_state)) {
      if (sidetone_mode == SIDETONE_ON) {
        noTone(sidetone_line);
        digitalWrite(13,LOW);
        digitalWrite(key_line,HIGH);
        
      }
      key_state = 0;
    }          
  }
}  






//-------------------------------------------------------------------------------------------------------

void schedule_keydown_keyup (unsigned int keydown_ms, unsigned int keyup_ms)
{
  if (keydown_ms) {
    tx_and_sidetone_key(1);
    key_scheduler_state = KEY_DOWN;
    next_key_scheduler_transition_time = millis() + keydown_ms;
    key_scheduler_keyup_ms = keyup_ms;      
  } else {
    tx_and_sidetone_key(0);
    key_scheduler_state = KEY_UP;
    next_key_scheduler_transition_time = millis() + keyup_ms;
  }
  
  
}

//-------------------------------------------------------------------------------------------------------

void service_key_scheduler()
{
  
  switch (key_scheduler_state) {      
    case KEY_DOWN:
      if (millis() >= next_key_scheduler_transition_time) {
        tx_and_sidetone_key(0);
        key_scheduler_state = KEY_UP;
        if (key_scheduler_keyup_ms) {
          next_key_scheduler_transition_time = (millis() + key_scheduler_keyup_ms);
        } else {
          key_scheduler_state = IDLE;
        }
      }
      break;
    case KEY_UP:
      if (millis() >= next_key_scheduler_transition_time) {
        key_scheduler_state = IDLE;
      }
      break;    
  }
}

//-------------------------------------------------------------------------------------------------------

void service_char_send_buffer() {

  if ((char_send_buffer_bytes > 0) && (pause_sending_buffer == 0) && (element_send_buffer_bytes == 0)) {
    send_char(char_send_buffer_array[0],NORMAL);
    remove_from_char_send_buffer();    
  }
  
}

//-------------------------------------------------------------------------------------------------------

void remove_from_char_send_buffer()
{
  if (char_send_buffer_bytes > 0) {
    char_send_buffer_bytes--;
  }
  if (char_send_buffer_bytes > 0) {
    for (int x = 0;x < char_send_buffer_bytes;x++) {
      char_send_buffer_array[x] = char_send_buffer_array[x+1];
    }
  }
}

//-------------------------------------------------------------------------------------------------------

void add_to_char_send_buffer(byte incoming_serial_byte) {

    if (char_send_buffer_bytes < char_send_buffer_size) {
      if (incoming_serial_byte != 127) {
        char_send_buffer_bytes++;
        char_send_buffer_array[char_send_buffer_bytes - 1] = incoming_serial_byte;
      } else {  // we got a backspace
        char_send_buffer_bytes--;
      }
    } 

}


//-------------------------------------------------------------------------------------------------------

void send_char(char cw_char, byte omit_letterspace)
{
  #ifdef DEBUG
  Serial.write("\nsend_char: called with cw_char:");
  Serial.print(cw_char);
  if (omit_letterspace) {
    Serial.print (" OMIT_LETTERSPACE");
  }
  Serial.write("\n\r");
  #endif
  
  if ((cw_char == 10) || (cw_char == 13)) { return; }  // don't attempt to send carriage return or line feed
  
  if (char_send_mode == CW) {
    switch (cw_char) {
      case 'A': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
      case 'B': send_dah(AUTOMATIC_SENDING); send_dits(3); break;
      case 'C': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
      case 'D': send_dah(AUTOMATIC_SENDING); send_dits(2); break;
      case 'E': send_dit(AUTOMATIC_SENDING); break;
      case 'F': send_dits(2); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
      case 'G': send_dahs(2); send_dit(AUTOMATIC_SENDING); break;
      case 'H': send_dits(4); break;
      case 'I': send_dits(2); break;
      case 'J': send_dit(AUTOMATIC_SENDING); send_dahs(3); break;
      case 'K': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
      case 'L': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dits(2); break;
      case 'M': send_dahs(2); break;
      case 'N': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
      case 'O': send_dahs(3); break;
      case 'P': send_dit(AUTOMATIC_SENDING); send_dahs(2); send_dit(AUTOMATIC_SENDING); break;
      case 'Q': send_dahs(2); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
      case 'R': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
      case 'S': send_dits(3); break;
      case 'T': send_dah(AUTOMATIC_SENDING); break;
      case 'U': send_dits(2); send_dah(AUTOMATIC_SENDING); break;    
      case 'V': send_dits(3); send_dah(AUTOMATIC_SENDING); break;
      case 'W': send_dit(AUTOMATIC_SENDING); send_dahs(2); break;
      case 'X': send_dah(AUTOMATIC_SENDING); send_dits(2); send_dah(AUTOMATIC_SENDING); break;
      case 'Y': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dahs(2); break;
      case 'Z': send_dahs(2); send_dits(2); break;
          
      case '0': send_dahs(5); break;
      case '1': send_dit(AUTOMATIC_SENDING); send_dahs(4); break;
      case '2': send_dits(2); send_dahs(3); break;
      case '3': send_dits(3); send_dahs(2); break;
      case '4': send_dits(4); send_dah(AUTOMATIC_SENDING); break;
      case '5': send_dits(5); break;
      case '6': send_dah(AUTOMATIC_SENDING); send_dits(4); break;
      case '7': send_dahs(2); send_dits(3); break;
      case '8': send_dahs(3); send_dits(2); break;
      case '9': send_dahs(4); send_dit(AUTOMATIC_SENDING); break;
      
      case '=': send_dah(AUTOMATIC_SENDING); send_dits(3); send_dah(AUTOMATIC_SENDING); break;
      case '/': send_dah(AUTOMATIC_SENDING); send_dits(2); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
      case ' ': add_to_element_send_buffer(KEY_UP_WORDSPACE_MINUS_4); break;
      case '*': send_dah(AUTOMATIC_SENDING); send_dits(3); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;    // using asterisk for BK
      case '.': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
      case ',': send_dahs(2); send_dits(2); send_dahs(2); break;
      case '\'': send_dit(AUTOMATIC_SENDING); send_dahs(4); send_dit(AUTOMATIC_SENDING); break;                   // apostrophe
      case '!': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dahs(2); break;
      case '(': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dahs(2); send_dit(AUTOMATIC_SENDING); break;
      case ')': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dahs(2); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
      case '&': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dits(3); break;
      case ':': send_dahs(3); send_dits(3); break;
      case ';': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
      case '+': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
      case '-': send_dah(AUTOMATIC_SENDING); send_dits(4); send_dah(AUTOMATIC_SENDING); break;
      case '_': send_dits(2); send_dahs(2); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
      case '"': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dits(2); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
      case '$': send_dits(3); send_dah(AUTOMATIC_SENDING); send_dits(2); send_dah(AUTOMATIC_SENDING); break;
      case '@': send_dit(AUTOMATIC_SENDING); send_dahs(2); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
      case '<': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;     // AR
      case '>': send_dits(3); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;               // SK
      case '\n': break;
      case '\r': break;
      case '|': add_to_element_send_buffer(HALF_UNIT_KEY_UP); return; break;  
      default: send_dits(2); send_dahs(2); send_dits(2); break;
    }  
    if (omit_letterspace != OMIT_LETTERSPACE) {
      add_to_element_send_buffer(KEY_UP_LETTERSPACE_MINUS_1); //this is minus one because send_dit and send_dah have a trailing element space
    }
  } else {
    #ifdef FEATURE_HELL
      transmit_hell_char(cw_char);
    #endif 
  }
  
}

//-------------------------------------------------------------------------------------------------------
void send_dit(byte sending_type) {
  add_to_element_send_buffer(ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP);
}

//-------------------------------------------------------------------------------------------------------
void send_dah(byte sending_type) {
  add_to_element_send_buffer(THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP);
}

//-------------------------------------------------------------------------------------------------------

void send_dits(int dits)
{
  for (;dits > 0;dits--) {
    send_dit(AUTOMATIC_SENDING);
  } 
  
}

//-------------------------------------------------------------------------------------------------------

void send_dahs(int dahs)
{
  for (;dahs > 0;dahs--) {
    send_dah(AUTOMATIC_SENDING);
  } 
  
}

//-------------------------------------------------------------------------------------------------------

void add_to_element_send_buffer(byte element_byte)
{
  if (element_send_buffer_bytes < element_send_buffer_size) {
    element_send_buffer_array[element_send_buffer_bytes] = element_byte;
    element_send_buffer_bytes++;
  } 

}

//-------------------------------------------------------------------------------------------------------

void remove_from_element_send_buffer()
{
  if (element_send_buffer_bytes > 0) {
    element_send_buffer_bytes--;
  }
  if (element_send_buffer_bytes > 0) {
    for (int x = 0;x < element_send_buffer_bytes;x++) {
      element_send_buffer_array[x] = element_send_buffer_array[x+1];
    }
  }
}

//-------------------------------------------------------------------------------------------------------


void service_element_send_buffer(){
  
  /*
  enum element_buffer_type {HALF_UNIT_KEY_UP, ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP, THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP,ONE_UNIT_KEYDOWN_3_UNITS_KEY_UP, THREE_UNIT_KEYDOWN_3_UNITS_KEY_UP,
  ONE_UNIT_KEYDOWN_7_UNITS_KEY_UP, THREE_UNIT_KEYDOWN_7_UNITS_KEY_UP, SEVEN_UNITS_KEY_UP, KEY_UP_LETTERSPACE_MINUS_1, KEY_UP_WORDSPACE_MINUS_1};
  */
  
  if ((key_scheduler_state == IDLE) && (element_send_buffer_bytes > 0)) {
    switch(element_send_buffer_array[0]) {
 
       case HALF_UNIT_KEY_UP:
         schedule_keydown_keyup(0,0.5*(1200/wpm));
         remove_from_element_send_buffer();
         break;
      
       case ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP:
         schedule_keydown_keyup(1200/wpm,1200/wpm);
         remove_from_element_send_buffer();
         break;
       
       case THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP:
         schedule_keydown_keyup(3*(1200/wpm),1200/wpm);
         remove_from_element_send_buffer();
         break;
         
       case KEY_UP_LETTERSPACE_MINUS_1:
         schedule_keydown_keyup(0,(length_letterspace-1)*(1200/wpm));
         remove_from_element_send_buffer();
         break;         
         
       case KEY_UP_WORDSPACE_MINUS_4:
         schedule_keydown_keyup(0,(length_wordspace-4)*(1200/wpm));
         remove_from_element_send_buffer();
         break;  
 
       case KEY_UP_WORDSPACE:
         schedule_keydown_keyup(0,length_wordspace*(1200/wpm));
         remove_from_element_send_buffer();
         break;         
      
    }
  }
  
}


//-------------------------------------------------------------------------------------------------------

int uppercase (int charbytein)
{
  if ((charbytein > 96) && (charbytein < 123)) {
    charbytein = charbytein - 32;
  }
  return charbytein;
}

//-------------------------------------------------------------------------------------------------------

void initialize_pins(){

  pinMode (sidetone_line, OUTPUT);
  digitalWrite (sidetone_line, LOW);  
  pinMode (13, OUTPUT);
  digitalWrite (13, LOW);
  pinMode(key_line, OUTPUT);
  digitalWrite(key_line,HIGH);

}  

//-------------------------------------------------------------------------------------------------------


