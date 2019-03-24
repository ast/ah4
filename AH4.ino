/* Arduino Nano 
 * ATmega328P  (Old Bootloader)
 * 
 */

const unsigned char START_OUT = 2;
const unsigned char KEY_IN = 4;
const unsigned char KEY_OUT = 12;
const unsigned char TUNE_BUTTON_PIN = 7;
const unsigned char BUTTON_GND = 9; // cheat ground for button
const unsigned char LED_PIN = 13;

enum state_t {
    ST_BYPASS,
    ST_START,
    ST_TUNING,
    ST_CONFIRM,
    ST_TUNED,
    ST_FAILED,
    NUM_STATES,
};

enum event_t {
    EV_NONE,
    EV_TUNE_PUSH,
    EV_TUNE_RELEASE,
    EV_TIMEOUT,
    EV_KEY_DOWN,
    EV_KEY_UP,
    EV_RESET,
    NUM_EVENTS,
};

/*
bool serialEvent() {
  if(Serial.available()) {
    char c = Serial.read();
    if(c == 'c') {
      Serial.println(c);
      return true;
    }
  }

  return false;
}*/

// Timers
unsigned long timeoutSet = 0;
unsigned long timeoutDelay = 0;

void setTimeout(unsigned long ms) {
  timeoutSet = millis();
  timeoutDelay = ms;
}

event_t timeoutEvent() {
  if(!timeoutDelay) {
    // No timer set
    return EV_NONE;
  }

  // Check timer
  if((millis() - timeoutSet) >= timeoutDelay) {
    // Expired, reset and return event
    timeoutSet = 0;
    return EV_TIMEOUT;
  }
  
  return EV_NONE;
}

event_t keyInEvent() {
  // Don't think I need to debounce this

  static uint8_t lastRead = digitalRead(KEY_IN);
  uint8_t currentRead = digitalRead(KEY_IN);
  event_t ev = EV_NONE;

  if(currentRead != lastRead) {
    switch(currentRead) {
      case 0: ev = EV_KEY_DOWN; break;
      case 1: ev = EV_KEY_UP; break;
    }
  }
  lastRead = currentRead;
  
  return ev;
}

// Tune button click with debounce
uint8_t tuneButtonEvent() {
  // Static variables keep state
  static unsigned long lastDebounceTime = millis();
  static uint8_t lastRead = digitalRead(TUNE_BUTTON_PIN);
  static uint8_t buttonState = lastRead;
 
  const unsigned long debounceDelay = 25;
  uint8_t currentRead = digitalRead(TUNE_BUTTON_PIN);

  event_t ev = EV_NONE;

  if(currentRead != lastRead) {
    lastDebounceTime = millis();
  }
  lastRead = currentRead;

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if(buttonState != currentRead) {
      buttonState = currentRead;
      switch(buttonState) {
        case 0: ev = EV_TUNE_PUSH; break;
        case 1: ev = EV_TUNE_RELEASE; break;
      }
    }
  }
  
  return ev;
}

state_t tuned_failed_bypass_push_ev_handler() {
  Serial.println("bypass_click_ev_handler");  
  digitalWrite(START_OUT, HIGH);
  setTimeout(500);
  return ST_START;
}

state_t start_key_down_ev_handler() {
  Serial.println("start_key_down_ev_handler");
  digitalWrite(KEY_OUT, HIGH);
  digitalWrite(START_OUT, LOW);
  return ST_TUNING;
}

state_t start_timeout_ev_handler() {
  Serial.println("start_timeout_ev_handler");
  return ST_BYPASS;
}

state_t tuning_key_up_ev_handler() {
  Serial.println("tuning_key_up_ev_handler");
  digitalWrite(KEY_OUT, LOW);
  setTimeout(40);
  return ST_CONFIRM;
}

state_t confirm_key_down_ev_handler() {
  Serial.println("confirm_key_down_ev_handler");
  return ST_FAILED;
}

state_t confirm_timeout_ev_handler() {
  Serial.println("confirm_timeout_ev_handler");
  return ST_TUNED;
}


/* State function type and state event table */
typedef state_t (*state_function_t)(void);
state_function_t state_table[NUM_STATES][NUM_EVENTS] = {
                   /* EV_NONE, EV_CLICK, EV_LONG_PRESS, EV_TIMEOUT, EV_KEY_DOWN, EV_KEY_UP */
/* ST_BYPASS */   {NULL, tuned_failed_bypass_push_ev_handler, NULL, NULL, NULL, NULL, NULL},
/* ST_START */    {NULL, NULL, NULL, start_timeout_ev_handler, start_key_down_ev_handler, NULL, NULL},
/* ST_TUNING */   {NULL, NULL, NULL, NULL, NULL, tuning_key_up_ev_handler, NULL},
/* ST_CONFIRM */  {NULL, NULL, NULL, confirm_timeout_ev_handler, confirm_key_down_ev_handler, NULL, NULL},
/* ST_TUNED */    {NULL, tuned_failed_bypass_push_ev_handler, NULL, NULL, NULL, NULL, NULL},
/* ST_FAILED */   {NULL, tuned_failed_bypass_push_ev_handler, NULL, NULL, NULL, NULL, NULL},
};

// the setup routine runs once when you press reset:
void setup() {
  Serial.begin(57600);
  Serial.setTimeout(0);

  pinMode(BUTTON_GND, OUTPUT); // just using a digital output as temporary button gnd
  digitalWrite(BUTTON_GND, LOW); // gnd is low

  // Tune button
  pinMode(TUNE_BUTTON_PIN, INPUT_PULLUP);

  // To tuner
  pinMode(START_OUT, OUTPUT);     // start signal to tuner
  digitalWrite(START_OUT, LOW);   // high starts tuning because of transistor buffer

  // From tuner
  pinMode(KEY_IN, INPUT_PULLUP); // key input from tuner TODO: put a stronger pull up on schematic
  
  // To radio
  pinMode(KEY_OUT, OUTPUT);
  digitalWrite(KEY_OUT, LOW);

  // To LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

event_t nextEvent() {
  event_t ev;
  if(ev = tuneButtonEvent())
    return ev;
  if(ev = keyInEvent())
    return ev;
  if(ev = timeoutEvent())
    return ev;
  return EV_NONE;
}

// the loop routine runs over and over again forever:
void loop() {
  static state_t state = ST_BYPASS;
  state_function_t state_function;

  // Run state machine
  state_function = state_table[state][nextEvent()];
  if(state_function != NULL) {
    state = state_function();
  }
}
