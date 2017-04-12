/////////////////////////////////////////////////
//
//  D-I-Y Polishing Station
//
//  Rey Medina 2017
//
//
//  ACKNOWLEDGEMENTS
//  This project will not possible without the prior work of the following persons:
//  
//  Angelo Fiorillo - Kitchen Timer code
//  Matthias Hertel - OneButton library and code
//  Nick Gammon - Rotary Encoder code 
//
/////////////////////////////////////////////////

#include "OneButton.h"
#include <Time.h>
#include <TimeLib.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <Stepper.h>

// Setup a new OneButton on pin A2.  
OneButton EncButton(A2, true);

// Button actions
#define btnClear 0
#define btnClicked 1
#define btnDblClicked 2
#define btnHeldStart 3
#define btnHeld 4
#define btnReleased 5

int btnLastAction = btnClear;

// Rotary encoder variables
volatile boolean blnRotated;

#define PinEncCLK 5
#define PinEncDATA 2
#define INTERRUPT 0  // that is, pin 2

int EncState;

#define RotateCW 1
#define RotateCCW 2
int iLastDir;

// Configure LCD
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

// Define icons
byte speaker[8] = {
        B00001,
        B00011,
        B00111,
        B10111,
        B10111,
        B00111,
        B00011,
        B00001
};

byte soundOn[8] = {
        B01000,
        B00100,
        B10010,
        B01010,
        B01010,
        B10010,
        B00100,
        B01000
};

byte soundOff[8] = {
        B00000,
        B00000,
        B10100,
        B01000,
        B10100,
        B00000,
        B00000,
        B00000
};

const int icon_speaker = 1;
const int icon_soundOn = 2;
const int icon_soundOff = 3;
const int icon_hourglass = 4;
const int icon_timer = 5;

bool blnSoundEnabled = true;

// Timer variables
int setupHours = 0;     // How many hours will count down when started
int setupMinutes = 0;   // How many minutes will count down when started
int setupSeconds = 0;   // How many seconds will count down when started
time_t setupTime = 0;

int currentHours = 0;
int currentMinutes = 0;
int currentSeconds = 0;
time_t currentTime = 0;

time_t startTime = 0;
time_t elapsedTime = 0;

// Modes
const int MODE_IDLE = 0;
const int MODE_SETUP = 1;
const int MODE_RUNNING = 2;
const int MODE_RINGING = 3;

int currentMode = MODE_IDLE;

int dataSelection = 0;

// Buzzer pin config
#define buzzerPin 10

unsigned long previousMillis = 0;
const long BeepInterval = 3500; 

// humidifier pin config
#define relayPin 12
bool HumidifierRunning = false;

// LED colors
#define PixelPin 13
const int intMaxLEDColors = 7;
const int intWipeWait = 0;
int intCurrLED = 0;

const int intPixels = 17;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(intPixels, PixelPin, NEO_GRB + NEO_KHZ800);

// initialize the stepper library on pins 8 through 11:
//const int stepsPerRevolution = 200;  // Nema 14

const int stepsPerRevolution = 1000;
Stepper myStepper(stepsPerRevolution, 6, 7, 8, 9);

int intPolishProgress = 0;
#define TICKVAL 7

//////////////////////////////////////////////////////////////////////////////////
//
//   setup
//
//////////////////////////////////////////////////////////////////////////////////
void setup() { 

  // link the Encoder Button functions.
  EncButton.attachClick(click1);
  EncButton.attachDoubleClick(doubleclick1);
  EncButton.attachLongPressStart(longPressStart1);
  EncButton.attachLongPressStop(longPressStop1);
  EncButton.attachDuringLongPress(longPress1);

  // setup rotation interrupt
  digitalWrite (PinEncCLK, HIGH);     // enable pull-ups
  digitalWrite (PinEncDATA, HIGH); 
  attachInterrupt (INTERRUPT, EncRotateISR, CHANGE);

  // configure buzzer pin
  pinMode(buzzerPin, OUTPUT);

  // Initialize LCD for 16 chars, 2 lines
  lcd.begin(16, 2);

  // create custom icons
  lcd.createChar(1,speaker);
  lcd.createChar(2,soundOn);
  lcd.createChar(3,soundOff);

  // set up humidifier pin
  pinMode(relayPin, OUTPUT);

  btnLastAction = btnClear;

  // Neo Pixel setup
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // set stepper speed
  myStepper.setSpeed(10);

  Serial.begin (9600);
  
} // setup

//////////////////////////////////////////////////////////////////////////////////
//
//   loop
//
//////////////////////////////////////////////////////////////////////////////////
void loop() { 

  // keep watching the push buttons:
  EncButton.tick();

  if ( blnRotated ) {
    
    switch(currentMode) {
    
      case MODE_IDLE:
        if(iLastDir == RotateCW) {
          // toggle beep on/off
          blnSoundEnabled = (!blnSoundEnabled);
          if (blnSoundEnabled) Beep();
        }
        else {
          // turn chamber light on/off
          if (intCurrLED == 0) intCurrLED = 1 ; else intCurrLED = 0;
          TurnLedON();
        }
        break;
        
      case MODE_SETUP:
        if(iLastDir == RotateCCW)
        {
          switch(dataSelection)
          {
            case 0: // hours
              setupHours--;
              if(setupHours == -1)
              {
                setupHours = 1;   // Max. hours. Original code set to 12 but 2hrs should be more than enough to polish
              }
              break;
            case 1: // minutes
              setupMinutes--;
              if(setupMinutes == -1)
              {
                setupMinutes = 59;
              }
              break;
            case 2: // seconds
              setupSeconds--;
              if(setupSeconds == -1)
              {
                setupSeconds = 59;
              }
              break;
          }
        }
        if(iLastDir == RotateCW)
        {
          switch(dataSelection)
          {
            case 0: // hours
              setupHours++;
//              if(setupHours == 13)
              if(setupHours == 2)
              {
                setupHours = 0;
              }
              break;
            case 1: // minutes
              setupMinutes++;
              if(setupMinutes == 60)
              {
                setupMinutes = 0;
              }
              break;
            case 2: // seconds
              setupSeconds++;
              if(setupSeconds == 60)
              {
                setupSeconds = 0;
              }
              break;
          }
        }
        break;

      case MODE_RUNNING:

        // Cycle through LED colors
        if(iLastDir == RotateCW)
        {
          intCurrLED++;
          if (intCurrLED > intMaxLEDColors) intCurrLED = 0;
        }
        
        if(iLastDir == RotateCCW)
        {
          intCurrLED--;
          if (intCurrLED < 0) intCurrLED = intMaxLEDColors;
        }

        TurnLedON();        
        
        Serial.println(intCurrLED);
        break;

      case MODE_RINGING:

        currentMode = MODE_IDLE;
        break;       
              
    }
      
    iLastDir = 0;
    blnRotated = false;
  
  }  // end if blnRotated

// Button management
  if ( btnLastAction > 0 ) {
    
    switch(currentMode) {
      
      case MODE_IDLE:
        
        if( btnLastAction == btnReleased )
        {
          if (blnSoundEnabled) Beep();
          currentMode = MODE_SETUP;
        }
           
        if( btnLastAction == btnClicked )
        {
          if (setupHours+setupMinutes+setupSeconds > 0 )
          {
            currentMode = currentMode == MODE_IDLE ? MODE_RUNNING : MODE_IDLE;
            if(currentMode == MODE_RUNNING)
            {
//              if (blnSoundEnabled) Beep();
              
              // STARTING TIMER!
              startTime = now();

              if (intCurrLED == 0) intCurrLED = 1;
              TurnLedON();
            }
          } else {
        
//            lcd.setCursor(0, 0);
//            lcd.print(formatLCDstring("Timer not set"));
//            delay(1500);
          }
        }
        break;
      
      case MODE_SETUP:
        if(btnLastAction == btnClicked)
        {
          // Select next data to adjust
          dataSelection++;
          if(dataSelection == 3)
          {
            dataSelection = 0;
          }
        }
        
        if( btnLastAction == btnReleased )
        {
          // Exit setup mode
          setupTime = setupSeconds + (60 * setupMinutes) + (3600 * setupHours);
          currentHours = setupHours;
          currentMinutes = setupMinutes;
          currentSeconds = setupSeconds;
          dataSelection = 0;
          currentMode = MODE_IDLE;
          if (blnSoundEnabled) Beep();
        }
        break;
      
    case MODE_RUNNING:
      if(btnLastAction == btnClicked)
      {
        Reset();
        TurnLedOFF();
        currentMode = MODE_IDLE;
      }

      break;

      case MODE_RINGING:

        if( btnLastAction == btnClicked )
        {
          currentMode = MODE_IDLE;
        }      
        break;       
    }

    btnLastAction = btnClear;
  
  }  // end if btnLastAction

  /*
   * Mode management
   */
  switch(currentMode)
  {
    case MODE_IDLE:

      digitalWrite(relayPin, HIGH);
      HumidifierRunning = false;
      intPolishProgress=0;

      break;
  }

  /*
   * Time management
   */
  switch(currentMode)
  {
    case MODE_IDLE:
    
      // turn off LEDs

      noTone(buzzerPin);
      myStepper.step(0);

    case MODE_SETUP:
      // NOP
      break;
      
    case MODE_RUNNING:
      currentTime = setupTime - (now() - startTime);

      myStepper.step(20);

      // Check if humidifier is currently running. If not, turn it on
      if (!HumidifierRunning) {
         digitalWrite(relayPin, LOW);
         HumidifierRunning = true;
      }
      
      if(currentTime <= 0)
      {
        currentMode = MODE_RINGING;
        TurnLedOFF();
      }
      break;
      
    case MODE_RINGING:

      unsigned long currentMillis = millis();

      if (HumidifierRunning) {
         digitalWrite(relayPin, HIGH);
         HumidifierRunning = false;
      }

      if (blnSoundEnabled)
      {
        if (currentMillis - previousMillis >= BeepInterval) {
            // save the last time you blinked the LED
            previousMillis = currentMillis;
            Beep();
            Beep();
            delay(150);
            Beep();
        }            
      }
      break;
  }


 /*
   * LCD management
   */
  //lcd.clear();
  lcd.setCursor(0, 0);
  switch(currentMode)
  {
    case MODE_IDLE:
      //lcd.backlight();
      lcd.print(formatLCDstring("READY"));
      lcd.setCursor(0, 1);
      lcd.print(formatTimeDigits(currentHours));
      lcd.print(":");
      lcd.print(formatTimeDigits(currentMinutes));
      lcd.print(":");
      lcd.print(formatTimeDigits(currentSeconds));
      lcd.print("    ");

      DisplaySoundConfig();
      break;
      
    case MODE_SETUP:
      lcd.print("Set timer: ");
      switch(dataSelection)
      {
        case 0:
          lcd.print("HRS ");
          break;
        case 1:
          lcd.print("MINS");
          break;
        case 2:
          lcd.print("SECS");
          break;
      }
      lcd.setCursor(0, 1);
      lcd.print(formatTimeDigits(setupHours));
      lcd.print(":");
      lcd.print(formatTimeDigits(setupMinutes));
      lcd.print(":");
      lcd.print(formatTimeDigits(setupSeconds));
      lcd.print("    ");
      break;
      
    case MODE_RUNNING:

      DisplayPolishProgress();
      
      lcd.setCursor(0, 1);
      if(hour(currentTime) < 10) lcd.print("0");
      lcd.print(hour(currentTime));
      lcd.print(":");
      if(minute(currentTime) < 10) lcd.print("0");
      lcd.print(minute(currentTime));
      lcd.print(":");
      if(second(currentTime) < 10) lcd.print("0");
      lcd.print(second(currentTime));
      break;
      
    case MODE_RINGING:
      lcd.print(formatLCDstring("DONE POLISHING"));
      lcd.setCursor(0, 1);
      lcd.print("        ");
      break;
  }
  
} // loop

///////////////////////////////////// 
//
//   Encoder rotation callback functions
//
/////////////////////////////////////

void EncRotateISR ()
{
  EncState = 0;
  EncState = EncState + digitalRead(PinEncCLK);
  if (EncState==0) return;
  EncState <<= 1;
  EncState = EncState + digitalRead(PinEncDATA);

  if (digitalRead(PinEncDATA) == digitalRead(PinEncCLK)) iLastDir = RotateCW; else iLastDir = RotateCCW;
  blnRotated = true;

}  // EncRotateISR


///////////////////////////////////// 
//
//   Encoder Button callback functions
//
/////////////////////////////////////

// This function will be called when the EncButton was pressed 1 time (and no 2. button press followed).
void click1() {
//  Serial.println("Encoder Button click.");
  btnLastAction = btnClicked;
} // click1

// This function will be called when the EncButton was pressed 2 times in a short timeframe.
void doubleclick1() {
//  Serial.println("Encoder Button doubleclick.");
  btnLastAction = btnDblClicked;
} // doubleclick1

// This function will be called once, when the EncButton is pressed for a long time.
void longPressStart1() {
//  Serial.println("Encoder Button longPress start");
  btnLastAction = btnHeldStart;
} // longPressStart1

// This function will be called often, while the EncButton is pressed for a long time.
void longPress1() {
//  Serial.println("Encoder Button longPress...");
  btnLastAction = btnHeld;
} // longPress1

// This function will be called once, when the EncButton is released after beeing pressed for a long time.
void longPressStop1() {
//  Serial.println("Encoder Button longPress stop");
  btnLastAction = btnReleased;
} // longPressStop1

///////////////////////////////////// 
//
//   Other routines
//
/////////////////////////////////////

char * formatTimeDigits (int num)
{
  static char strOut[3];

  if (num >= 0 && num < 100) {
    sprintf(strOut, "%02d", num);
  } else {
    strcpy(strOut, "XX");
  }

  return strOut;
}

char * formatLCDstring (char * strIn)
{
  static char strOut[16];
  sprintf(strOut, "%-16s", strIn); 
  return strOut;
}

void DisplaySoundConfig()
{
  lcd.setCursor(14, 1);
  lcd.write(icon_speaker);  
  
  lcd.setCursor(15, 1);
  if (blnSoundEnabled)
  {
    lcd.write(icon_soundOn); 
  }
  else 
  {
    lcd.write(icon_soundOff);
  }
} // DisplaySoundConfig()

void Reset()
{
  currentMode = MODE_IDLE;
  currentHours = setupHours;
  currentMinutes = setupMinutes;
  currentSeconds = setupSeconds;
}

///////////////////////////////
//
//   Neo Pixel code
//
///////////////////////////////

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void TurnLedON() {
        switch (intCurrLED) {
          case 0: 
            // turn off
            TurnLedOFF();
            break;

          case 1:
            colorWipe(strip.Color(100, 100, 100), intWipeWait); // White
            break;        
            
          case 2:
            colorWipe(strip.Color(150, 0, 0), intWipeWait); // Red
            break;
            
          case 3:
            colorWipe(strip.Color(0, 150, 0), intWipeWait); // Green
            break;
            
          case 4:
            colorWipe(strip.Color(0, 0, 150), intWipeWait); // Blue
            break;
  
          case 5:
            colorWipe(strip.Color(150, 50, 0), intWipeWait); // Yellow
            break;        
  
          case 6:
            colorWipe(strip.Color(150, 0, 100), intWipeWait); // Purple
            break;        
            
          case 7:
            colorWipe(strip.Color(0, 150, 150), intWipeWait); // Cyan
            break;        
        }
  
}

void TurnLedOFF() {
  colorWipe(strip.Color(0, 0, 0), 0); 
}

void DisplayPolishProgress() {
  intPolishProgress++;
//      Serial.println(intPolishProgress);
  if (intPolishProgress > TICKVAL*4) intPolishProgress=0;
  
  if (intPolishProgress<=TICKVAL)                                  lcd.print("Polishing       ");
  if (intPolishProgress>TICKVAL   && intPolishProgress<=TICKVAL*2) lcd.print("Polishing.      ");
  if (intPolishProgress>TICKVAL*2 && intPolishProgress<=TICKVAL*3) lcd.print("Polishing..     ");
  if (intPolishProgress>TICKVAL*3 && intPolishProgress<=TICKVAL*4) lcd.print("Polishing...    ");
  if (intPolishProgress>TICKVAL*4 && intPolishProgress<=TICKVAL*5) lcd.print("Polishing....   ");
  if (intPolishProgress>TICKVAL*5 && intPolishProgress<=TICKVAL*6) lcd.print("Polishing.....  ");
  if (intPolishProgress>TICKVAL*6 && intPolishProgress<=TICKVAL*7) lcd.print("Polishing...... ");
  if (intPolishProgress>TICKVAL*7)                                 lcd.print("Polishing.......");  
}

void Beep() {
  tone(buzzerPin, 2100);
  delay(40);
  noTone(buzzerPin);
//  delay(80);
}
