
/****************************************************
*****************************************************
  Simple acoustic variometer with LCD for Arduino
  Version: v01.2019
  Author: Petr Vilimek
  Email: spiderwolf@seznam.cz
  Feel free to use the code as you wish.
  
*****************************************************
  Notice 1:  
   * I reused some code (filter calculations & beeper function) from https://github.com/IvkoPivko/MiniVario-Arduino
   * It provides better responses and results than my own implementation based on using of simple Kalman filter			
  
*****************************************************
  Notice 2: 
   * Barometric pressure & temperature sensor (MS-5611) Arduino Library: https://github.com/jarzebski/Arduino-MS5611

*****************************************************
****************************************************/

#include <Wire.h>
#include <MS5611.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include <Adafruit_PCD8544.h>

// LCD PIN definition
#define RST 4
#define CE 5
#define DC 6
#define DIN 7
#define CLK 8

MS5611 ms5611; // Barometer variable

Adafruit_PCD8544 display = Adafruit_PCD8544(RST, CE, DC, DIN, CLK); // LCD variable

// Pushbutton pin definition
const unsigned short buttonMenuUp = 3;
const unsigned short buttonMenuDown = 12;
const unsigned short buttonUp = 10;
const unsigned short buttonDown = 11;
const unsigned short buttonContrast = 2;

// Pushbutton states
unsigned short buttonUpState = 0;
unsigned short buttonDownState = 0;
unsigned short buttonMenuUpState = 0;
unsigned short buttonMenuDownState = 0;
unsigned short buttonContrastState = 0;

unsigned short menuCounter = 1; // Menu counter

float seaLevelPressure = 101325; // Default sea level pressure [Pa]

// Min/Max Temperature
float maxTemp = 0;
float minTemp = 50;
float newTemp = 0;

// Max/Min Altitude
float lastAlt = 0;
float newAlt = 0;
float maxAlt = 0;

float min_climb = 0.20;               // Minimal climb (Standard value is + 0.4 m/s)
float max_sink = -3.50;               // Maximal sink (Standard value is - 1.1 m/s)

long readTime = 125;                    // Reading interval from Barometer, standard (min) is 150

long const_frq = 150;                  // Audio frequency at constant frequency setting
long max_frq = 2000;                   // Maximum audio frequency at variable frequency setting

const int a_pin1 = 9;                         // Speaker pin definition 

// Filter settings / Make chnages very very carefully !!!
float errorV = 3.000 * min_climb;    // Calculate weighting for Vario filters 0.1 > errorV < 1.0
float mean_n = 7;                     //  Number of values used for mean value computing
float kal[8];                           // kal[n] ==> n = mean_n +1

long Pressure;
float Vario, VarioR, Altitude, AvrgV, Temp;
int startCH = 0;
unsigned long  dTime, TimeE, TimeS, TimePip;

int lcdContrast = 42; // Default LCD contrast


// SETUP//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  // LCD Unit
  display.begin();
  display.clearDisplay();
    
  //Serial.begin(9600); // Uncomment for debug purposes
  
  // Initialize MS5611 sensor!
  // Ultra high resolution: MS5611_ULTRA_HIGH_RES
  // (default) High resolution: MS5611_HIGH_RES
  // Standard: MS5611_STANDARD
  // Low power: MS5611_LOW_POWER
  // Ultra low power: MS5611_ULTRA_LOW_POWER
  
  while (!ms5611.begin(MS5611_ULTRA_HIGH_RES))
  {
    delay(500);
  }

  
  // Startup tone sequence
  tone(a_pin1 , 100, 150);
  delay(200);
  tone(a_pin1 , 200, 150);
  delay(200);
  tone(a_pin1 , 400, 150);
  delay(200);
  tone(a_pin1 , 700, 150);
  delay(200);
  tone(a_pin1 , 1100, 150);
  delay(200);
  tone(a_pin1 , 1600, 150);
  delay(200);
 
  TimeS = micros();
}


// LOOP///////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {

// LCD contrast
if (buttonContrastState == HIGH) {
	lcdContrast++;
	delay(200);
	display.setContrast(lcdContrast); // LCD contrast tuning
}

if (lcdContrast == 51) {
	lcdContrast = 35;
}


dTime = (micros() - TimeS);
if (float(dTime) / 1000 >= float(readTime))
    
    Calculate();
    if (Vario >= min_climb || Vario <= max_sink) Beeper();
    else noTone(a_pin1);

// Button
buttonUpState = digitalRead(buttonUp);
buttonDownState = digitalRead(buttonDown);
buttonMenuUpState = digitalRead(buttonMenuUp);
buttonMenuDownState = digitalRead(buttonMenuDown);
buttonContrastState = digitalRead(buttonContrast);


if (buttonUpState == HIGH){
	seaLevelPressure = seaLevelPressure + 50;
	delay(50);
}

if (buttonDownState == HIGH){
	seaLevelPressure = seaLevelPressure - 50;
	delay(50);
}

if (buttonMenuUpState == HIGH){
	menuCounter++;
	delay(150);
}

if (buttonMenuDownState == HIGH){
	menuCounter--;
	delay(150);
}

if (menuCounter == 7){
	menuCounter = 1;
}

if (menuCounter == 0){
	menuCounter = 6;
}	


// Menu
switch(menuCounter) {
	
	case 1:
	{
		// Altitude				
		display.drawLine(0, 0, display.width(), 0, BLACK);
		display.drawLine(0, 8, display.width(), 8, BLACK);
		display.setTextSize(1);
		display.setTextColor(WHITE,BLACK);
		display.setCursor(0,1);
		display.println(" ALT         1");
		display.setTextSize(2);
		display.setTextColor(BLACK);
		display.setCursor(15,17);
		display.print(Altitude, 0);
		display.setTextSize(2);
		display.setTextColor(BLACK);
		display.setCursor(70,15);
		display.println("m");
		display.setCursor(0,40);
		display.setTextSize(1);
		display.println("MAX:");
		display.setCursor(40,40);
		display.print(maxAlt, 0);
		display.setCursor(70,40);
		display.println("m");
		display.display();
		display.clearDisplay();
	}
	break;
	
	case 2:
	{
		// Vario
		display.drawLine(0, 0, display.width(), 0, BLACK);
		display.drawLine(0, 8, display.width(), 8, BLACK);
		display.setTextSize(1);
		display.setTextColor(WHITE,BLACK);
		display.setCursor(0,1);
		display.println(" VARIO       2");
		display.setTextSize(2);
		display.setTextColor(BLACK);
		display.setCursor(5,17);
		display.print(Vario,2);
		display.setTextSize(1);
		display.setTextColor(BLACK);
		display.setCursor(65,16);
		display.println("m/s");
		display.setCursor(0,40);
		display.println("ALT:");
		display.setCursor(40,40);
		display.println(Altitude,0);
		display.setCursor(70,40);
		display.println("m");
		display.display();
		display.clearDisplay();
	}
	break;
	
	case 3:
	{
		// Contrast
		display.drawLine(0, 0, display.width(), 0, BLACK);
		display.drawLine(0, 8, display.width(), 8, BLACK);
		display.setTextSize(1);
		display.setTextColor(WHITE,BLACK);
		display.setCursor(0,1);
		display.println(" CONTRAST    3");
		display.setTextSize(2);
		display.setTextColor(BLACK);
		display.setCursor(30,17);
		display.print(lcdContrast);
		display.setTextSize(1);
		display.setTextColor(BLACK);
		display.setCursor(0,40);
		display.println("DEFAULT: 42");
		display.display();
		display.clearDisplay();
	}
	break;
	
	case 4:
	{
		//Sea level adjustment
		display.drawLine(0, 0, display.width(), 0, BLACK);
		display.drawLine(0, 8, display.width(), 8, BLACK);
		display.setTextSize(1);
		display.setTextColor(WHITE,BLACK);
		display.setCursor(0,1);
		display.println(" SEA LEVEL   4");
		display.setTextSize(1);
		display.setTextColor(BLACK);
		display.setCursor(0,15);
		display.println("USE +- BUTTONS");
		display.setCursor(15,25);
		display.print(seaLevelPressure,0);
		display.setCursor(60,25);
		display.println("Pa");
		display.setCursor(0,40);
		display.println("ALT:");
		display.setCursor(40,40);
		display.println(Altitude,0);
		display.setCursor(70,40);
		display.println("m");
		display.display();
		display.clearDisplay();
		
	}
	break;

	case 5:
	{
		// Temperatures
		display.drawLine(0, 0, display.width(), 0, BLACK);
		display.drawLine(0, 8, display.width(), 8, BLACK);
		display.setTextSize(1);
		display.setTextColor(WHITE,BLACK);
		display.setCursor(0,1);
		display.println(" TEMP        5");
		display.setTextSize(2);
		display.setTextColor(BLACK);
		display.setCursor(0,12);
		display.print(Temp, 2);
		display.setTextSize(1);
		display.setTextColor(BLACK);
		display.setCursor(65,12);
		display.println("C");
		display.setCursor(0,30);
		display.println("MAX:");
		display.setCursor(25,30);
		display.print(maxTemp);
		display.setCursor(0,40);
		display.println("MIN:");
		display.setCursor(25,40);
		display.print(minTemp);
		display.display();
		display.clearDisplay();
	}
	break;
	
	case 6:
	{
		
		// Voltage
		float temp = (float)(readVcc());
		float voltage = temp /1000;
		
		if (voltage > 2.90) {
			display.drawLine(0, 0, display.width(), 0, BLACK);
			display.drawLine(0, 8, display.width(), 8, BLACK);
			display.setTextSize(1);
			display.setTextColor(WHITE,BLACK);
			display.setCursor(0,1);
			display.println(" VOLTAGE     6");
			display.setTextSize(2);
			display.setTextColor(BLACK);
			display.setCursor(0,17);
			display.print(voltage,2);
			display.setTextSize(1);
			display.setTextColor(BLACK);
			display.setCursor(55,17);
			display.println("V");
			display.setCursor(0,40);
			display.println("BATT: OK");
			display.display();
			display.clearDisplay();
		}
		else {
			display.drawLine(0, 0, display.width(), 0, BLACK);
			display.drawLine(0, 8, display.width(), 8, BLACK);
			display.setTextSize(1);
			display.setTextColor(WHITE,BLACK);
			display.setCursor(0,1);
			display.println(" VOLTAGE     8");
			display.setTextSize(2);
			display.setTextColor(BLACK);
			display.setCursor(0,17);
			display.print(voltage,2);
			display.setTextSize(1);
			display.setTextColor(BLACK);
			display.setCursor(55,17);
			display.println("V");
			display.setCursor(0,40);
			display.println("BATT:RECHARGE!");
			display.display();
			display.clearDisplay();
		}
	}
	break;
  }
}
  

void BaroReading() {
  Temp = ms5611.readTemperature();
  Pressure = ms5611.readPressure(true);
  Altitude = ms5611.getAltitude(Pressure, seaLevelPressure);
}


void Calculate() {
      
  BaroReading();

// Max altitude
newAlt = Altitude;
if (newAlt > maxAlt)
maxAlt = newAlt;

// Max/Min temperature
newTemp = Temp;
if (newTemp > maxTemp)
maxTemp = newTemp;
if (newTemp < minTemp)
minTemp = newTemp;

int i;
if (startCH == 0) {
	kal[0] = Altitude;
	startCH = 1;}
    
  dTime = (micros() - TimeS);
  TimeS = micros();

  VarioR = ((Altitude - kal[0]) / (float(dTime) / 1000000));

  kal[1] = 0.55* VarioR + 0.45* kal[1]; 

  kal[0] = Altitude;

  AvrgV = 0;
  i = 1;
  for (i; i <= mean_n; i++) {
    AvrgV = AvrgV + kal[i];
  }
  
  AvrgV = AvrgV / mean_n;
  AvrgV = (AvrgV  + Vario) / 2;
  
  if (errorV > 1.000) errorV = 1.000;
  Vario = errorV * AvrgV + (1 - errorV) * Vario;

  i = mean_n;
  for (i; i > 1; i--) {
    kal[i] = kal[i - 1];
  }

  /**   // Comment out for debug purposes 
        Serial.print(pressure);
        Serial.print("; ");
      
        Serial.print(Altitude, 2);
        Serial.print("; ");
      
        Serial.print(VarioR, 2);
        Serial.print("; ");
      
        Serial.print(Vario, 2);
        Serial.println();  

		Serial.print(lcdContrast);
		Serial.println();
 **/	  
}

void Beeper() {
  
    float frequency = -0.33332*Vario*Vario*Vario*Vario + 9.54324*Vario*Vario*Vario - 102.64693*Vario*Vario + 512.227*Vario + 84.38465;

    float duration = 1.6478887*Vario*Vario -38.2889*Vario + 341.275253; // Variable Pause
  
    frequency = int(frequency);
    duration = long(duration);
  
    if ( Vario >= min_climb) {
        if ((millis() - TimePip) >= (unsigned long)(2 * duration)) {
          TimePip = millis();
          tone( a_pin1 , int(frequency), int(duration) );
        }
    }
    
	if ( Vario <= max_sink) {
      tone(a_pin1 , 300, 150);
      delay(125);
      tone(a_pin1 , 200, 150);
      delay(150);
      tone(a_pin1 , 100, 150);
      delay(175);
    }
}

// Voltage measure
long readVcc() {
	// Read 1.1V reference against AVcc
	// set the reference to Vcc and the measurement to the internal 1.1V reference
	#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
	ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
	#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
	ADMUX = _BV(MUX5) | _BV(MUX0);
	#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
	ADMUX = _BV(MUX3) | _BV(MUX2);
	#else
	ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
	#endif

	delay(2); // Wait for Vref to settle
	ADCSRA |= _BV(ADSC); // Start conversion
	while (bit_is_set(ADCSRA,ADSC)); // measuring

	uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
	uint8_t high = ADCH; // unlocks both

	long result = (high<<8) | low;
	
	//scale_constant = internal1.1Ref * 1023 * 1000
	
	//internal1.1Ref = 1.1 * Vcc1 (per voltmeter) / Vcc2 (per readVcc() function)

	long scale_constant = 1111619L;
	//result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
	result = scale_constant / result;
	return result; // Vcc in millivolts
}
