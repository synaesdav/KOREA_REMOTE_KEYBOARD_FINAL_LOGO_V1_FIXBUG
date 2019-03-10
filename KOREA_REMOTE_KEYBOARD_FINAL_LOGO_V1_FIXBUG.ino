//weird fix for unexplained non compiling
//with SEOUL logo

//radio remote controller with oled display and trellis keypad
//advanced version has specific keystrokes sent depending on button pressed
//Korea tour version
//written by David Crittenden 7/2018
//BUTTON LAYOUT
/*
  row 1: All around you WITH COUNT IN / Always Away NO INTRO / Everything is Beautiful Under the Sun NO INTRO / Sing WITH COUNT IN
  row 2: Inferno NO INTRO / Beautiful NO INTRO / All around you NO INTRO / Sing NO INTO
  row 3: CRISTIANO DANCE MUSIC / HEART FRAME FADE / Circle Droplet / Multi - Color Shock Wave
  row 4: FUCK TRUMP / Rainbow Checkers / Red-White Heart / CLEAR ALL
*/

//single menue performance mode transmitter for Cristiano
//1 menu of 16 buttons
//higher transmition power

//sends numerical values of ascii characters
//modified from https://learn.adafruit.com/remote-effects-trigger/overview-1?view=all
//Ada_remoteFXTrigger_TX
//Remote Effects Trigger Box Transmitter
//by John Park
//for Adafruit Industries
//
// General purpose button box
// for triggering remote effects
// using packet radio Feather boards
//
//
//MIT License

#include <Keyboard.h>//library allows remote to act as a keyboard over usb
#include <SPI.h>
#include <RH_RF69.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_Trellis.h"
#include <Encoder.h>

/********* Encoder Setup ***************/
#define PIN_ENCODER_SWITCH 11
Encoder knob(10, 12);
uint8_t activeRow = 0;
long pos = -999;
long newpos;
int prevButtonState = HIGH;
bool needsRefresh = true;
bool advanced = false;
unsigned long startTime;


/********* Trellis Setup ***************/
#define MOMENTARY 0
#define LATCHING 1
#define MODE LATCHING //all Trellis buttons in latching mode
Adafruit_Trellis matrix0 = Adafruit_Trellis();
Adafruit_TrellisSet trellis =  Adafruit_TrellisSet(&matrix0);
#define NUMTRELLIS 1
#define numKeys (NUMTRELLIS * 16)
#define INTPIN A2

/************ OLED Setup ***************/
Adafruit_SSD1306 oled = Adafruit_SSD1306();
#if defined(ESP8266)
#define BUTTON_A 0
#define BUTTON_B 16
#define BUTTON_C 2
#define LED      0
#elif defined(ESP32)
#define BUTTON_A 15
#define BUTTON_B 32
#define BUTTON_C 14
#define LED      13
#elif defined(ARDUINO_STM32F2_FEATHER)
#define BUTTON_A PA15
#define BUTTON_B PC7
#define BUTTON_C PC5
#define LED PB5
#elif defined(TEENSYDUINO)
#define BUTTON_A 4
#define BUTTON_B 3
#define BUTTON_C 8
#define LED 13
#elif defined(ARDUINO_FEATHER52)
#define BUTTON_A 31
#define BUTTON_B 30
#define BUTTON_C 27
#define LED 17
#else // 32u4, M0, and 328p
#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5
#define LED      13
#endif


/************ Radio Setup ***************/
// Can be changed to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ 915.0

#if defined (__AVR_ATmega32U4__) // Feather 32u4 w/Radio
#define RFM69_CS      8
#define RFM69_INT     7
#define RFM69_RST     4
#endif

#if defined(ARDUINO_SAMD_FEATHER_M0) // Feather M0 w/Radio
#define RFM69_CS      8
#define RFM69_INT     3
#define RFM69_RST     4
#endif

#if defined (__AVR_ATmega328P__)  // Feather 328P w/wing
#define RFM69_INT     3  // 
#define RFM69_CS      4  //
#define RFM69_RST     2  // "A"
#endif

#if defined(ESP32)    // ESP32 feather w/wing
#define RFM69_RST     13   // same as LED
#define RFM69_CS      33   // "B"
#define RFM69_INT     27   // "A"
#endif

//NEW CODE (wasn't choosing from the feather MO if endif)
#define RFM69_CS      8
#define RFM69_INT     3
#define RFM69_RST     4

// Singleton instance of the radio driver
RH_RF69 rf69(RFM69_CS, RFM69_INT);

int lastButton = 17; //last button pressed for Trellis logic
char radiopacket[20];//stores information sent

//int menuList[8] = {1, 2, 3, 4, 5, 6, 7, 8}; //for rotary encoder choices
int m = 0; //variable to increment through menu list
int lastTB[8] = {16, 16, 16, 16, 16, 16, 16, 16}; //array to store per-menu Trellis button

//for battery level
#define VBATPIN A7

/*******************SETUP************/
void setup()
{
  delay(500);
  Serial.begin(115200);
  //while (!Serial) { delay(1); } // wait until serial console is open, remove if not tethered to computer

  Keyboard.begin();//for keyboard emulation

  // INT pin on Trellis requires a pullup
  pinMode(INTPIN, INPUT);
  digitalWrite(INTPIN, HIGH);
  trellis.begin(0x70);

  pinMode(PIN_ENCODER_SWITCH, INPUT_PULLUP);//set encoder push switch pin to input pullup

  digitalPinToInterrupt(10); //on M0, Encoder library doesn't auto set these as interrupts
  digitalPinToInterrupt(12);

  // Initialize OLED display
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  oled.setTextWrap(false);
  //oled.display();//Adafruit logo
  //delay(500);
  oled.clearDisplay();
  oled.display();
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);

  // manual reset
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);

  if (!rf69.init())
  {
    Serial.println("RFM69 radio init failed");
    while (1);
  }
  Serial.println("RFM69 radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
  // No encryption
  if (!rf69.setFrequency(RF69_FREQ))
  {
    Serial.println("setFrequency failed");
  }

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(20, true);//originally 14

  // The encryption key has to be the same as the one in the server
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
                  };
  rf69.setEncryptionKey(key);

  pinMode(LED, OUTPUT);

  Serial.print("RFM69 radio @");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");

  /*oled.setCursor(0, 0);
    oled.println("RFM69 @ ");
    oled.print((int)RF69_FREQ);
    oled.println(" MHz");
    oled.display();
    delay(1200); //pause to let message be read by a human*/

  // light up all the LEDs in order
  for (uint8_t i = 0; i < numKeys; i++)
  {
    trellis.setLED(i);
    trellis.writeDisplay();
    delay(25);
  }

  oled.clearDisplay();
  oled.setCursor(12, 0);
  oled.println("CRISTIANO");
  oled.setCursor(0, 16);
  oled.println("DANCE MUSIC");
  oled.display();
  delay(1200); //pause to let message be read by a human
  oled.clearDisplay();
  oled.setCursor(36, 0);
  oled.print("KOREA");
  oled.setCursor(0, 16);
  oled.print("PERFORMANCE");
  oled.display();
  delay (1200);

  // then turn them off
  for (uint8_t i = 0; i < numKeys; i++)
  {
    trellis.clrLED(i);
    trellis.writeDisplay();
    delay(25);
  }

  //check the battery level
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  measuredvbat *= 100;    // to move the decimal place
  int batteryPercent = map(measuredvbat, 330, 425, 0, 100);//operating voltage ranges from 3.3 - 4.25

  oled.clearDisplay();
  oled.setCursor(12, 0);
  oled.print("Batt ");
  oled.print(batteryPercent);
  oled.println("% ");
  oled.setCursor(0, 16);
  oled.print(measuredvbat / 100);
  oled.println(" volts  ");
  oled.display();
  delay(2000); //pause to let message be read by a human
  oled.clearDisplay();
  oled.setCursor(36, 0);
  oled.print("ENTER");
  oled.setCursor(12, 16);
  oled.print("SELECTION");
  oled.display();
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

void loop()
{
  delay(30); // 30ms delay is required, dont remove me! (Trellis)

  /*************Rotary Encoder Menu***********/
  //not using the rotary knob in performance mode untill I get a proper debounce

  // remember that the switch is active low
  int buttonState = digitalRead(PIN_ENCODER_SWITCH);
  if (buttonState == LOW)//button has been pressed
  {
    unsigned long now = millis();
    if (prevButtonState == HIGH)
    {
      prevButtonState = buttonState;
      startTime = now;
      Serial.println("button pressed");

      //send trellis selection
      Serial.print("Sending ");
      Serial.println(radiopacket[0]);
      rf69.send((uint8_t *)radiopacket, strlen(radiopacket));
      rf69.waitPacketSent();

      //send a keyboard command to the computer depending on which button was pressed
      switch (radiopacket[0])
      {
        case 1://ALL AROUND YOU - WITH 8 BEAT COUNT IN
          Keyboard.print("1");//send message to computer
          delay(20);
          Keyboard.print(" ");//send message to computer
          break;
        case 2://ALWAYS AWAY - NO INTRO
          Keyboard.print("2");//send message to computer
          delay(20);
          Keyboard.print(" ");//send message to 
          break;
        case 3://EVERYTHING IS BEAUTIFUL - NO INTRO
          Keyboard.print("3");//send message to computer
          delay(20);
          Keyboard.print(" ");//send message to 
          break;
        case 4://SING - WITH 8 BEAT COUNT IN
          Keyboard.print("4");//send message to computer
          delay(20);
          Keyboard.print(" ");//send message to 
          break;
        case 5://INFERNO - NO INTRO
          Keyboard.print("5");//send message to computer
          delay(20);
          Keyboard.print(" ");//send message to 
          break;
        case 6://BEAUTIFUL - NO INTRO
          Keyboard.print("6");//send message to computer
          delay(20);
          Keyboard.print(" ");//send message to 
          break;
        case 7://ALL AROUND YOU - NO INTRO
          Keyboard.print("1");//send message to computer
          delay(20);
          Keyboard.print(" ");//send message to 
          break;
        case 8://SING - NO INTRO
          Keyboard.print("4");//send message to computer
          delay(20);
          Keyboard.print(" ");//send message to 
          break;        
        case 16:
          Keyboard.print(" ");//send message to 
          break;
        default:
          break;
      }
    }
  }
  else if (buttonState == HIGH && prevButtonState == LOW)
  {
    Serial.println("button released!");
    prevButtonState = buttonState;
  }

  /*************Trellis Button Presses***********/
  if (MODE == LATCHING)
  {
    if (trellis.readSwitches()) { // If a button was just pressed or released...
      for (uint8_t i = 0; i < numKeys; i++) { // go through every button
        if (trellis.justPressed(i)) { // if it was pressed...
          //Serial.print("v"); Serial.println(i);

          if (i != lastTB[m]) {
            if (trellis.isLED(i)) {
              trellis.clrLED(i);
              lastTB[m] = i; //set the stored value for menu changes
            }
            else {
              trellis.setLED(i);
              //trellis.clrLED(lastButton);//turn off last one
              trellis.clrLED(lastTB[m]);
              lastTB[m] = i; //set the stored value for menu changes
            }
            trellis.writeDisplay();
          }

          //check the rotary encoder menu choice
          if (m == 0)//first menu item: PERFORMANCE MODE
          {
            if (i == 0)
            {
              radiopacket[0] = 1;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("8 COUNT IN");
              oled.setCursor(0, 16);
              oled.print("All Around You");
              oled.display();
            }
            if (i == 1)
            {
              radiopacket[0] = 2;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("NO INTRO");
              oled.setCursor(0, 16);
              oled.print("Always Away");
              oled.display();
            }
            if (i == 2)
            {
              radiopacket[0] = 3;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("NO INTRO");
              oled.setCursor(0, 16);
              oled.print("Everything is Beautiful");
              oled.display();
            }

            if (i == 3)
            {
              radiopacket[0] = 4;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("8 COUNT IN");
              oled.setCursor(0, 16);
              oled.print("Sing");
              oled.display();
            }

            if (i == 4)
            {
              radiopacket[0] = 5;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("NO INTRO");
              oled.setCursor(0, 16);
              oled.print("Inferno");
              oled.display();
            }

            if (i == 5)
            {
              radiopacket[0] = 6;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("NO INTRO");
              oled.setCursor(0, 16);
              oled.print("Beautiful");
              oled.display();
            }
            if (i == 6)
            {
              radiopacket[0] = 7;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("NO INTRO");
              oled.setCursor(0, 16);
              oled.print("All Around You");
              oled.display();
            }
            if (i == 7)
            {
              radiopacket[0] = 8;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("NO INTRO");
              oled.setCursor(0, 16);
              oled.print("Sing");
              oled.display();
            }

            if (i == 8)
            {
              radiopacket[0] = 9;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("TEXT SCROLL");
              oled.setCursor(0, 16);
              oled.print("Seoul Logo");
              oled.display();
            }

            if (i == 9)
            {
              radiopacket[0] = 10;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("Heart");
              oled.setCursor(0, 16);
              oled.print("Fade");
              oled.display();
            }

            if (i == 10)
            {
              radiopacket[0] = 11;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("Circle");
              oled.setCursor(0, 16);
              oled.print("Droplet");
              oled.display();
            }

            if (i == 11)
            {
              radiopacket[0] = 12;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("Multi-Color");
              oled.setCursor(0, 16);
              oled.print("Shock Wave");
              oled.display();
            }

            if (i == 12)
            {
              radiopacket[0] = 13;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("Text Scroll");
              oled.setCursor(0, 16);
              oled.print("FUCK TRUMP");
              oled.display();
            }

            if (i == 13)
            {
              radiopacket[0] = 14;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("Rainbow");
              oled.setCursor(0, 16);
              oled.print("Checkers");
              oled.display();
            }

            if (i == 14)
            {
              radiopacket[0] = 15;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("Red-White");
              oled.setCursor(0, 16);
              oled.print("Heart");
              oled.display();
            }

            if (i == 15)
            {
              radiopacket[0] = 16;
              oled.clearDisplay();
              oled.setCursor(0, 0);
              oled.print("CLEAR");
              oled.setCursor(0, 16);
              oled.print("ALL");
              oled.display();
            }
          }
        }
      }
    }
  }
}

