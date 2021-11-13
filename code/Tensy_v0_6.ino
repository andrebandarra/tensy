/* 
Tensy V0.6
Uses the Tensy Mc-Tense Face V2 Hardware https://easyeda.com/editor#id=8d637346f792449f84bd48fbd7bb56de
Used on the first flight test with Swift 5

Changes from V0.5
- added timer on bot left corner (right button resetable)

number of active loadcells selectable on activeScales

Setup
- Sets up LCD
- Sets up SD card, file and header
- Sets up and calibrates scales

Loop
- Keeps track of time
- Performs functions at predefined rates
  - checks buttons state
  - writes hx711 readings to readings array
  - readings array
    - print on Serial
    - write on SD
    - print on LCD
*/

// =================== Timekeeping/Scheduling ==================
unsigned long currentMillis;          //current millis
unsigned long hx711Millis = 0;        //previous millis since hx711 readings
unsigned long serialMillis = 0;       //previous millis since serial
unsigned long sdMillis = 0;           //previous millis since sd write
unsigned long lcdMillis = 0;          //previous millis since lcd update
unsigned long buttonsMillis = 0;      //previous millis since buttons check

unsigned long allSeconds;             //current millis in seconds
char timeStart[5];                    //time allSeconds in 4 characters (5-1)

unsigned long buttonSeconds;          //button seconds, allSeconds - pressSeconds
char timeButton[3];                   //time buttonSeconds in 2 characters (3-1)
unsigned long pressSeconds;           //press millis in seconds

int hx711Rate = 100;                //millis interval for taking hx711 reading
int serialRate = 500;               //millis interval for serial update
int sdRate = 100;                   //millis interval for sd card write update
int lcdRate = 500;                  //millis interval for lcd update
int buttonsRate = 200;              //millis interval for buttons update

// =================== Buttons ==================
int Lbuttonpin = 4;               //pin for Left button
int Rbuttonpin = 3;               //pin for Right button
int LBval;                        //value of Left button
int RBval;                        //value of Right button

// =================== HX711 Scales ==================
#include "HX711.h"  //HX711 library

HX711 scale[13];       //sets up X scales
byte scalePins[13][2] = {{22,23},{24,25},{26,27}, {28,29}, {30,31},{32,33},{34,35},{36,37},{38,39},{40,41},{42,43},{44,45},{46,47}};     //defines the pairs of pins for each scale
const float calib[13] = {10200, 9564, 10249, 10204, 9753, 9850, 9000, 9800, 9450, 9575, 9700, 9800, 9800};                                  //defines the calibration factor for each scale
const int activeScales = 11;
const int init_sample = 50; //initial sample size
const int mini_sample = 25;  //mini sample size
const int sec_sample = 100;  //secondary sample size
float readings[13];               //array where hx711 readings are stored
const int readingSamples = 1;     //number of samples averaged to store on readings array 

// =================== SD CARD ==================
#include <SPI.h>                  //SPI communication library
#include <SD.h>                   //SD Card library
const int sdpin = 6;              //sd Chip Select pin
File dataFile;                    //Creates logging file
char fileName[] = "LOGGER00.TXT"; //Base filename for logging
const int sdWriteSamples = 1;     //number of averaged samples to write on SD card 
bool sdIcon = false;              //icon for LCD when SD is writting 

// =================== Nokia 5110 LCD ==================
#include <LCD5110_Graph.h>        //lcd library
LCD5110 myGLCD(12,11,10,8,9);     //pins: SCK, DIN, DC, RST, CS, (ground light for on)
extern uint8_t SmallFont[];       //defines usable font 
extern uint8_t TinyFont[];        //defines usable font 
//extern uint8_t MediumNumbers[];
byte xyScales1[13][2] = {{10,10},{30,10},{50,10},{10,20},{30,20},{50,20},{70,20},{10,30},{30,30},{50,30},{10,40},{30,40},{50,40}};     //defines the pairs of X,Y coords for each of the scales print (13 SMALL)
byte xyScales2[13][2] = {{1,1},{30,1},{59,1},{1,11},{30,11},{59,11},{1,21},{30,21},{59,21},{1,31},{30,31},{59,31},{84,48}};     //defines the pairs of X,Y coords for each of the scales print (12 BIG)

void setup() {
  pinMode(Lbuttonpin, INPUT_PULLUP);     //configure pin as an input and enable the internal pull-up resistor
  pinMode(Rbuttonpin, INPUT_PULLUP);     //configure pin as an input and enable the internal pull-up resistor

  Serial.begin(57600);                      //initiate serial port
  Serial.println("Booting");
  Serial.print("Initializing LCD ");
  myGLCD.InitLCD();                         //initiate LCD
  myGLCD.setFont(SmallFont);
  myGLCD.clrScr();
  myGLCD.print("==Tensy V2==", CENTER, 2);
  myGLCD.update(); 
  Serial.println("DONE");

  delay(500);
  Serial.print("Initializing SD card  ");
  myGLCD.print("Init SD", LEFT, 11);
  myGLCD.update(); 
  if (!SD.begin(sdpin)){
    Serial.println("FAILED!");
    myGLCD.print("FAIL!", RIGHT, 11);
    myGLCD.update(); 
  }
  else{
    delay(500);
    Serial.println("DONE");
    myGLCD.print("DONE", RIGHT, 11);
    myGLCD.update(); 
  }
  for (byte i = 1; i <= 99; i++){         // Construct the filename to be the next incrementally indexed filename in the set [00-99].  
    if (SD.exists(fileName)){             // check before modifying target filename.     
      fileName[6] = i/10 + '0';           // the filename exists so increment the 2 digit filename index.
      fileName[7] = i%10 + '0';
    } 
   }
  dataFile = SD.open(fileName, FILE_WRITE);       // Open up the file we're going to log to!
  if (! dataFile){
    Serial.println("Error opening LOGGERXX.txt");
    myGLCD.print("file ERROR", CENTER, 21);
    myGLCD.update();
  }
  dataFile.println("#Tensy McTense V2 - Firmware V0.6");  //write Logger file header
  dataFile.print("#Cells calib values");                  
  for (byte j = 0; j <= activeScales; j++){              
    dataFile.print("\t");
    dataFile.print(calib[j]);
  }
  dataFile.println();
  
  dataFile.print("time");
  for (byte j = 1; j <= activeScales; j++){              
    dataFile.print(",");
    dataFile.print(j);
  }
  dataFile.println();
  
   dataFile.print("s");
  for (byte j = 1; j <= activeScales; j++){              
    dataFile.print(",");
    dataFile.print("kg");
  }
  dataFile.println();

    myGLCD.print("Calib Scales", CENTER, 40);
    myGLCD.update();
   
  for (int ida = 0; ida < activeScales; ida++){
    scale[ida].begin(scalePins[ida][0], scalePins[ida][1]);   //initializes each of the scales using the pin pairs
    scale[ida].set_gain(64);                                  //sets the gain for each of the scales
    Serial.print("Calibrating Scale ");
    Serial.print(ida+1);
    myGLCD.invertText(false);                   //gets text back to normal
    myGLCD.printNumI(ida+1,ida*6+1, 31);        //prints scale number on LCD and increments X spacing
    myGLCD.update();
    scale[ida].read();                          //raw reading from the ADC
    scale[ida].read_average(init_sample);       //average of X readings from the ADC
    scale[ida].get_value(mini_sample);          //average of Y readings from the ADC minus the tare weight (not set yet)
    scale[ida].get_units(mini_sample);          //average of Y readings from the ADC minus tare weight (not set) divided by the SCALE parameter (not set yet)
    scale[ida].set_scale(calib[ida]);           //set scale calibration factor
    scale[ida].tare();                          //reset the scale to 0
    scale[ida].read();                          //raw reading from the ADC 
    scale[ida].read_average(sec_sample);        //average of Z readings from the ADC
    scale[ida].get_value(mini_sample);          //average of Y readings from the ADC minus the tare weight, set with tare()
    scale[ida].get_units(mini_sample);          //average of Y readings from the ADC minus tare weight, divided by the SCALE parameter set with set_scale
    scale[ida].tare();                          //reset the scale to 0
    Serial.println(" DONE");
    myGLCD.invertText(true);                    //inverts text as confirmation
    myGLCD.printNumI(ida+1,ida*6+1, 31);        //prints scale number on LCD and increments X spacing
    myGLCD.invertText(false);                   //gets text back to normal
    myGLCD.update();
  }
}

void buttons_check(){
  if (currentMillis - buttonsMillis > buttonsRate){       //checks if enough time has gone past to update
  
  LBval = digitalRead(Lbuttonpin);                        //read the pushbutton value into the variable
  RBval = digitalRead(Rbuttonpin);                        //read the pushbutton value into the variable

    if (RBval == LOW){
      pressSeconds = millis()/1000;      
      }

   
   buttonsMillis = currentMillis;                          //records when was the last time it was updated
  }
}

void hx711_Update(){
  if (currentMillis - hx711Millis > hx711Rate){                    //checks if enough time has gone past to update
    for (int idb = 0; idb < activeScales; idb++){
      readings[idb] = (scale[idb].get_units(readingSamples));      //prints each scale to serial, with X decimal places
    } 
   hx711Millis = currentMillis;                                    //records when was the last time readings were updated
  }
}

void tension_printSerial() {
  if (currentMillis - serialMillis > serialRate){     //checks if enough time has gone past to update
   Serial.print(millis()/1000.0, 1);                       //prints seconds to serial
    for (int idc = 0; idc < activeScales; idc++){
      Serial.print("\t");
      Serial.print(readings[idc], 3);                 //prints value of the readings array to serial, with X decimal places
    }
   Serial.println();
   serialMillis = currentMillis;                       //records when was the last time readings were updated
  }
}

void tension_writeSD() {
  if (currentMillis - sdMillis > sdRate){             //checks if enough time has gone past to update
   dataFile.print(millis()/1000.0, 1);                     //prints seconds to sd, with 1 decimal place
    for (int idd = 0; idd < activeScales; idd++){
      dataFile.print(",");
      dataFile.print(readings[idd], 3);           //prints each scale to sd, with X decimal places
    }
   dataFile.println();
   dataFile.flush();                              //save after every line of data. Safer but slower
   sdMillis = currentMillis;                      //records when was the last time readings were updated
   if (SD.exists(fileName)){
    sdIcon = true;
   }
  }
}

void print_LCD() {
  if (currentMillis - lcdMillis > lcdRate){                       //checks if enough time has gone past to update
    myGLCD.clrScr();

    for (int idf = 0; idf < activeScales; idf++){
      myGLCD.printNumF(readings[idf],1,xyScales2[idf][0],xyScales2[idf][1]);      //prints each scale to LCD, with W decimal placesplaces, in position X and Y
    }

    allSeconds = millis() / 1000;                       //updates timer
    sprintf(timeStart, "%04d", allSeconds);
    myGLCD.print(timeStart, 0, 41);                      //prints total time to bot left
    buttonSeconds = allSeconds - pressSeconds;
    sprintf(timeButton, "%02d", buttonSeconds);
    myGLCD.print(timeButton, 30, 41);                    //prints button time to bot centre
    myGLCD.update();
    
     if (sdIcon == true){                                //if tension_writeSD is running and card and file are initialized this is true
      myGLCD.invertText(true);
      myGLCD.print("SD ON", 54,41);
      myGLCD.update();
      myGLCD.invertText(false);
      sdIcon = false; 
    }
    else{
      myGLCD.print("SD OFF", 49,41); 
      myGLCD.update();      
    }
 
   lcdMillis = currentMillis;                       //records when was the last time readings were updated
  } 
}

void loop() {
currentMillis = millis();                           //updates timekeeping
buttons_check();                                    //checks buttons statuses
hx711_Update();                                     //reads and stores hx711 readings
tension_printSerial();                              //prints readings to serial
tension_writeSD();                                  //writes tensions to SD
print_LCD();                                        //prints to LCD version two (12 BIG)
}