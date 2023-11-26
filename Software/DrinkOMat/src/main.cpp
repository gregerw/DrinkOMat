#include <ArduinoBLE.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//IOs
#define PUMP_CH1 D3
#define PUMP_CH2 D4
#define VENT_COMP1 D6
#define VENT_COMP2 D7
#define VENT_COMP3 D8
#define VENT_COMP4 D9
#define VENT_COMP5 D10
#define VENT_COMP6 D11
#define VENT_BYPASS D12
#define VENT_AIR D5
#define SWITCH_DOSE D2

//cocktail definition
struct cocktail {
	char name[50];
	int dos1;
  int dos2;
  int dos3;
  int dos4;
  int dos5;
  int dos6;
  int co2;
};
int selectedDrink = 0;

cocktail cocktails[5]; //

//display
#define WIRE Wire
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &WIRE,-1);

//time
unsigned long myTime = millis();
unsigned long startTimeBankReleais = 0;
unsigned long startTimePump = 0;
unsigned long mixButtonPressTime = 0;
bool mixButtonPressed = false;
bool toggledDrink = false;

//Drink data
String _drinkName = "DrinkOMat";
int _dos1 = 0;
int _dos2 = 0;
int _dos3 = 0;
int _dos4 = 0;
int _dos5 = 0;
int _dos6 = 0;
int _co2 = 0;

//bools for ble transfer
bool receivedNewCocktailName = false;
bool receivedNewDos1 = false;
bool receivedNewDos2 = false;
bool receivedNewDos3 = false;
bool receivedNewDos4 = false;
bool receivedNewDos5 = false;
bool receivedNewDos6 = false;
bool receivedNewCo2 = false;


//UUIDs for BT communication
const char* deviceServiceUuid = "4D6592AB-1413-48D8-A64C-F7F0038C3F55";
const char* comp1DoseCharacteristicUuid = "2618983E-A834-4F02-960A-7037CB1F75A8";
const char* comp2DoseCharacteristicUuid = "273672CD-0690-4F01-AEFB-94F51C5E207D";
const char* comp3DoseCharacteristicUuid = "FA0C2D8A-1998-40FA-9861-BA0B62CD3FAC";
const char* comp4DoseCharacteristicUuid = "26C3C153-653A-4C1D-B3BD-0080EEE4B7C4";
const char* comp5DoseCharacteristicUuid = "5E482368-C9D2-4EF0-9704-218EA8423D9F";
const char* comp6DoseCharacteristicUuid = "BC97F54D-ADA3-4B04-9CAD-D5BA8B68621B";
const char* cocktailNameCharacteristUuid = "19274C1C-1B7C-46C9-981A-7FA1AC3C2189";
const char* co2CharacteristicUuid = "AC6A7575-B4B5-4477-B51B-BF0F82D15A91";

int gesture = -1;

BLEService drinkService(deviceServiceUuid); 
BLEIntCharacteristic comp1DoseCharacteristic(comp1DoseCharacteristicUuid, BLERead | BLEWrite);
BLEIntCharacteristic comp2DoseCharacteristic(comp2DoseCharacteristicUuid, BLERead | BLEWrite);
BLEIntCharacteristic comp3DoseCharacteristic(comp3DoseCharacteristicUuid, BLERead | BLEWrite);
BLEIntCharacteristic comp4DoseCharacteristic(comp4DoseCharacteristicUuid, BLERead | BLEWrite);
BLEIntCharacteristic comp5DoseCharacteristic(comp5DoseCharacteristicUuid, BLERead | BLEWrite);
BLEIntCharacteristic comp6DoseCharacteristic(comp6DoseCharacteristicUuid, BLERead | BLEWrite);
BLEIntCharacteristic co2Characteristic(co2CharacteristicUuid, BLERead | BLEWrite);
BLEStringCharacteristic cocktailNameCharacteristic(cocktailNameCharacteristUuid, BLERead | BLEWrite,20);

//declarations
void mixDrink();
void checkForNewCocktail();
bool getCO2FlagForComponent(int component, int co2value);

//setup
void setup() {
  Serial.begin(9600);
  
  //display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.clearDisplay();
  display.display();
  delay(100);

  //IOs
  pinMode(PUMP_CH1, OUTPUT);
  pinMode(PUMP_CH2, OUTPUT);
  pinMode(VENT_COMP1, OUTPUT);
  pinMode(VENT_COMP2, OUTPUT);
  pinMode(VENT_COMP3, OUTPUT);
  pinMode(VENT_COMP4, OUTPUT);
  pinMode(VENT_COMP5, OUTPUT);
  pinMode(VENT_COMP6, OUTPUT);
  pinMode(VENT_BYPASS, OUTPUT);
  pinMode(VENT_AIR, OUTPUT);
  pinMode(SWITCH_DOSE, INPUT_PULLUP);

  //cocktail fifo
  cocktail dummyCocktail;
  strcpy(dummyCocktail.name, "no drink");
  dummyCocktail.dos1 = 0;
  dummyCocktail.dos2 = 0;
  dummyCocktail.dos3 = 0;
  dummyCocktail.dos4 = 0;
  dummyCocktail.dos5 = 0;
  dummyCocktail.dos6 = 0;
  dummyCocktail.co2 = 0;
  cocktails[0] = dummyCocktail;
  cocktails[1] = dummyCocktail;
  cocktails[2] = dummyCocktail;
  cocktails[3] = dummyCocktail;
  cocktails[4] = dummyCocktail;

  //BLE
  if (!BLE.begin()) {
    Serial.println("- Starting Bluetooth® Low Energy module failed!");
    while (1);
  }

  //BLE services
  BLE.setLocalName("Arduino Nano 33 BLE (Peripheral)");
  BLE.setAdvertisedService(drinkService);
  drinkService.addCharacteristic(comp1DoseCharacteristic);
  drinkService.addCharacteristic(comp2DoseCharacteristic);
  drinkService.addCharacteristic(comp3DoseCharacteristic);
  drinkService.addCharacteristic(comp4DoseCharacteristic);
  drinkService.addCharacteristic(comp5DoseCharacteristic);
  drinkService.addCharacteristic(comp6DoseCharacteristic);
  drinkService.addCharacteristic(co2Characteristic);
  drinkService.addCharacteristic(cocktailNameCharacteristic);
  BLE.addService(drinkService);
  BLE.advertise();

  Serial.println("DrinkOMat 33 BLE (Peripheral Device)");
}

void loop() {
  BLEDevice central = BLE.central();
  delay(100);

  //vent releais on/off to prevent power bank going off
  unsigned long bankReleaisOnTime = 500;
  unsigned long bankReleaisOffTime = 15000;
  
  if((millis() - startTimeBankReleais) > (bankReleaisOnTime + bankReleaisOffTime)) {
    //new cycle reached, turn releais on
    digitalWrite(VENT_BYPASS,1);
    startTimeBankReleais = millis();
  }
  else {
    if((millis() - startTimeBankReleais) > bankReleaisOnTime) {
      //turn releais off.
      digitalWrite(VENT_BYPASS,0);
    }
    else {
      //turn releais on.
      digitalWrite(VENT_BYPASS,1);
    }
  }
  

  //check drink switch
  unsigned long timeForDrinkSwitch = 2000;
  bool mixButtonStatus = !digitalRead(SWITCH_DOSE); //true when button is pressed
  if(!mixButtonPressed && mixButtonStatus) {
    //button was toggled to on, start timer
    mixButtonPressTime = millis();
    mixButtonPressed = true;
  }
  else if(mixButtonPressed && mixButtonStatus) {
    //button is still pressed, check for time
    if((millis() - mixButtonPressTime) > timeForDrinkSwitch) {
      //switch to next drink and reset timer
      toggledDrink = true;
      mixButtonPressed = false;
      selectedDrink++;
      if(selectedDrink > 4) selectedDrink = 0;
    }
  }
  else if(mixButtonPressed && !mixButtonStatus) {
    if(toggledDrink) {
      //just toggled the drink, nothing to do
      toggledDrink = false;
      mixButtonPressed = false;
      delay(100);
    }
    else {
    //switch toggled off. mix the drink
    Serial.println("mix the drink");
    mixButtonPressed = false;
    mixDrink();
    }

  }

  //BLE central
  if (central) {
    //Serial.println("* Connected to central device!");
    //Serial.print("* Device MAC address: ");
    //Serial.println(central.address());
    //Serial.println(" ");

    if (central.connected()) {
      if (comp1DoseCharacteristic.written()) {
        Serial.print("dos 1 updated:");
        Serial.println(comp1DoseCharacteristic.value());
        _dos1 = comp1DoseCharacteristic.value();
        receivedNewDos1 = true;
        checkForNewCocktail();
       }
      if (comp2DoseCharacteristic.written()) {
        Serial.print("dos 2 updated:");
        Serial.println(comp2DoseCharacteristic.value());
        _dos2 = comp2DoseCharacteristic.value();
        receivedNewDos2 = true;
        checkForNewCocktail();
       }
      if (comp3DoseCharacteristic.written()) {
        Serial.print("dos 3 updated:");
        Serial.println(comp3DoseCharacteristic.value());
        _dos3 = comp3DoseCharacteristic.value();
        receivedNewDos3 = true;
        checkForNewCocktail();
       }
      if (comp4DoseCharacteristic.written()) {
        Serial.print("dos 4 updated:");
        Serial.println(comp4DoseCharacteristic.value());
        _dos4 = comp4DoseCharacteristic.value();
        receivedNewDos4 = true;
        checkForNewCocktail();
       }
      if (comp5DoseCharacteristic.written()) {
        Serial.print("dos 5 updated:");
        Serial.println(comp5DoseCharacteristic.value());
        _dos5 = comp5DoseCharacteristic.value();
        receivedNewDos5 = true;
        checkForNewCocktail();
       }
      if (comp6DoseCharacteristic.written()) {
        Serial.print("dos 6 updated:");
        Serial.println(comp6DoseCharacteristic.value());
        _dos6 = comp6DoseCharacteristic.value();
        receivedNewDos6 = true;
        checkForNewCocktail();
       }
      if (co2Characteristic.written()) {
        Serial.print("co2 updated:");
        Serial.println(co2Characteristic.value());
        _co2 = co2Characteristic.value();
        receivedNewCo2 = true;
        checkForNewCocktail();
       }
      if (cocktailNameCharacteristic.written()) {
        Serial.print("name updated:");
        Serial.println(cocktailNameCharacteristic.value());
        _drinkName = String(cocktailNameCharacteristic.value());
        receivedNewCocktailName = true;
        checkForNewCocktail();
       }
    }    
    //Serial.println("* Disconnected to central device!");
  }

  //show current drink name
  display.clearDisplay();
  display.setTextSize(1,2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(cocktails[selectedDrink].name);

  
  //dose
  String stringDoses = String(cocktails[selectedDrink].dos1);
  stringDoses += " / ";
  stringDoses +=  String(cocktails[selectedDrink].dos2);
  stringDoses += " / ";
  stringDoses += String(cocktails[selectedDrink].dos3);
  stringDoses += " / ";
  stringDoses += String(cocktails[selectedDrink].dos4);
  stringDoses += " / ";
  stringDoses += String(cocktails[selectedDrink].dos5);
  stringDoses += " / ";
  stringDoses += String(cocktails[selectedDrink].dos6);
  display.setTextSize(1);
  display.println(stringDoses);
  display.display();
  delay(100);
}

void checkForNewCocktail() {
  //check if new cocktail is complete
  if(receivedNewCocktailName && receivedNewDos1 && receivedNewDos2 && receivedNewDos3 && receivedNewDos4 && receivedNewDos5 && receivedNewDos6 && receivedNewCo2) {
    //reset bools
    Serial.println("put cocktail in fifo");
    receivedNewCocktailName = false;
    receivedNewDos1 = false;
    receivedNewDos2 = false;
    receivedNewDos3 = false;
    receivedNewDos4 = false;
    receivedNewDos5 = false;
    receivedNewDos6 = false;
    receivedNewCo2 = false;

    //put in fifo on first position
    for(int i = 4; i>0;i--) {
      cocktails[i] = cocktails[i-1];
    }
    cocktail newCocktail;
    const char* drinkNameChar = _drinkName.c_str();
    strcpy(newCocktail.name, drinkNameChar);
    newCocktail.dos1 = _dos1;
    newCocktail.dos2 = _dos2;
    newCocktail.dos3 = _dos3;
    newCocktail.dos4 = _dos4;
    newCocktail.dos5 = _dos5;
    newCocktail.dos6 = _dos6;
    newCocktail.co2 = _co2;
    cocktails[0] = newCocktail;

    Serial.print("new cocktail in fifo:");
    for(int i=0;i<5;i++) {
      Serial.print(cocktails[i].name);
      Serial.print(";");
    }
    Serial.println();
  }
}

void doseComponent(int compNumber, int doseVolume) {
  unsigned long doseConst = 172; //millis for 1ml

  //get co2 attribute of component
  int co2Value = cocktails[0].co2;
  bool co2Active = getCO2FlagForComponent(compNumber,co2Value);
  Serial.print("CO2 flag");
  Serial.println(co2Active);

  //close bypass vent
  if(co2Active) {
    //co2 component, so close bypass
    digitalWrite(VENT_BYPASS,1);
    //overwrite dose constant
    doseConst = 50;
  }
  else {
    digitalWrite(VENT_BYPASS,0);
  }

  //turn magnet ventile on
  if(compNumber == 1) {
    digitalWrite(VENT_COMP1,1);
  }
  else if(compNumber == 2) {
    digitalWrite(VENT_COMP2,1);
  }
  else if(compNumber == 3) {
    digitalWrite(VENT_COMP3,1);
  }
  else if(compNumber == 4) {
    digitalWrite(VENT_COMP4,1);
  }
  else if(compNumber == 5) {
    digitalWrite(VENT_COMP5,1);
  }
  else if(compNumber == 6) {
    digitalWrite(VENT_COMP6,1);
  }
  //show mix status on display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("MIXING...");
  display.setTextSize(1);
  display.println(cocktails[selectedDrink].name);
  String compName = "Component ";
  compName += String(compNumber);
  compName += ": ";
  compName += String(doseVolume);
  compName += " ml";
  display.println(compName);
  display.display(); 
  delay(200);

  //start dosing
  startTimePump = millis();
  Serial.println("Pump on");

  if(co2Active) {
    //air pump
    digitalWrite(PUMP_CH1,0);
    digitalWrite(PUMP_CH2,1);
  }
  else {
    //peristaltic pump
    digitalWrite(PUMP_CH1,1);
    digitalWrite(PUMP_CH2,0);
  }
  
  while((millis()-startTimePump) < (doseVolume*doseConst)) {
    delay(100);
  }
  //stop dosing
  Serial.println("Pump off");
  digitalWrite(PUMP_CH1,0);
  digitalWrite(PUMP_CH2,0);
  digitalWrite(VENT_COMP1,0);
  digitalWrite(VENT_COMP2,0);
  digitalWrite(VENT_COMP3,0);
  digitalWrite(VENT_COMP4,0);
  digitalWrite(VENT_COMP5,0);
  digitalWrite(VENT_COMP6,0);
  delay(500);
}

void mixDrink() {
  for(int i = 1; i<=6;i++) {
    if((i == 1) && cocktails[selectedDrink].dos1 > 0) {
      //comp 1
      doseComponent(1,cocktails[selectedDrink].dos1);
    }
    else if((i == 2) && cocktails[selectedDrink].dos2 > 0) {
      //comp 2
      doseComponent(2,cocktails[selectedDrink].dos2);
    }
    else if((i == 3) && cocktails[selectedDrink].dos3 > 0) {
      //comp 3
      doseComponent(3,cocktails[selectedDrink].dos3);
    }
    else if((i == 4) && cocktails[selectedDrink].dos4 > 0) {
      //comp 4
      doseComponent(4,cocktails[selectedDrink].dos4);
    }
    else if((i == 5) && cocktails[selectedDrink].dos5 > 0) {
      //comp 5
      doseComponent(5,cocktails[selectedDrink].dos5);
    }
    else if((i == 6) && cocktails[selectedDrink].dos6 > 0) {
      //comp 5
      doseComponent(6,cocktails[selectedDrink].dos6);
    }
  }

}

bool getCO2FlagForComponent(int component, int co2value) {
  //comp 6 
  int modCO2 = co2value % 32;
  if(modCO2 != co2value) {
    co2value = co2value - 32;
    if(component == 6) return true;
  }
  //comp 5
  modCO2 = co2value % 16;
  if(modCO2 != co2value) {
    co2value = co2value - 16;
    if(component == 5) return true;
  }
  //comp 4
  modCO2 = co2value % 8;
  if(modCO2 != co2value) {
    co2value = co2value - 8;
    if(component == 4) return true;
  }
  //comp 3
  modCO2 = co2value % 4;
  if(modCO2 != co2value) {
    co2value = co2value - 4;
    if(component == 3) return true;
  }
  //comp 2
  modCO2 = co2value % 2;
  if(modCO2 != co2value) {
    co2value = co2value - 2;
    if(component == 2) return true;
  }
  //comp 1
  if(co2value > 0) {
    if(component == 1) return true;
  }
  return false;
}


 