#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_not_connected_layer;
static TextLayer *s_unread_sms_layer;


char *months[] =  {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

#define OP_NOP    0
#define OP_VIBRATE   1
#define OP_SHOW_NOTCONNECTED 2
#define OP_FIND_ME 3
#define OP_UNREAD_MESSAGE_COUNT 4

static char unread_sms_buffer[4];

static char bt_notification_delay = 0;

static const uint32_t const disconnect_segments[] = { 300, 300, 300, 300, 300 };
VibePattern disconnecte_pat = {
  .durations = disconnect_segments,
  .num_segments = ARRAY_LENGTH(disconnect_segments),
};

static void inbox_received_handler(DictionaryIterator *iterator, void *context) {
  
  // Get the first pair
  Tuple *t = dict_read_first(iterator);

  // Process all pairs present
  while(t != NULL) {
    // Process this pair's key
    APP_LOG(APP_LOG_LEVEL_INFO, "Got opcode: %d, value: %d", (int)t->key, (int)t->value->int32);
    switch(t->key) {
      case OP_VIBRATE:
        // Trigger vibration
        vibes_short_pulse();
        break;
      case OP_SHOW_NOTCONNECTED:
        layer_set_hidden((Layer *)s_not_connected_layer, false);
        break;
      case OP_FIND_ME:
        vibes_enqueue_custom_pattern(disconnecte_pat);    
        break;
      case OP_UNREAD_MESSAGE_COUNT:
        if ((int)t->value->int32 > 0) {
          snprintf(unread_sms_buffer, sizeof(unread_sms_buffer), "%d", (int)t->value->int32);
        } else {
          strncpy(unread_sms_buffer, "", sizeof(unread_sms_buffer));
        }
        
        text_layer_set_text(s_unread_sms_layer, unread_sms_buffer);
        APP_LOG(APP_LOG_LEVEL_INFO, "set sms msg: %s", unread_sms_buffer);

      break;          
      
      default:
        APP_LOG(APP_LOG_LEVEL_INFO, "Unknown key: %d", (int)t->key);
        break;
    }

    // Get next pair, if any
    t = dict_read_next(iterator);
  }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}


static char bt_connection_notifications = 0;

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char time_buffer[] = "00.00";
  static char date_buffer[] = "September 31 ";

  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(time_buffer, sizeof(time_buffer), "%H:%M", tick_time);
  } else {
    // Use 12 hour format
    strftime(time_buffer, sizeof(time_buffer), "%I:%M", tick_time);
  }
  
  snprintf(date_buffer, sizeof(date_buffer), "%s %d", months[tick_time->tm_mon], tick_time->tm_mday);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, time_buffer);
  text_layer_set_text(s_date_layer, date_buffer);
  
  // If bt_notification_delay is set to something else than zero we go over
  // a short delay by incrementing the bt_notification_delay counter until
  // we trigger the vibration. After the vibration is triggered we'll stop
  // at a final state so that no further vibers are triggered.
  //
  // The bt_handler can at any time reset the bt_notification_delay back to zero
  // if the connection is resumed.
  if (bt_notification_delay == 1) {
    bt_notification_delay = 2;
  } else if (bt_notification_delay == 2) {
    vibes_enqueue_custom_pattern(disconnecte_pat);    
    bt_notification_delay = 3;
  }
}

// Checks if local time is around the sleeping period to prevent waking user up.
bool isDaytime() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  if (tick_time->tm_hour > 6 && tick_time->tm_hour < 22) {
    return true;
  } else {
    return false;
  }
}

void bt_handler(bool connected) {
  if (connected == false) {
    layer_set_hidden((Layer *)s_not_connected_layer, false);      
    
    if (isDaytime()) {
      vibes_enqueue_custom_pattern(disconnecte_pat);
    } else {
      bt_notification_delay = 1;
    }
  } else {
      layer_set_hidden((Layer *)s_not_connected_layer, true);
      bt_notification_delay = 0; 
  }
}

static void main_window_load(Window *window) {
  // Create status TextLayer
  s_unread_sms_layer = text_layer_create(GRect(100, 0, 40, 30));
  text_layer_set_background_color(s_unread_sms_layer, GColorClear);
  text_layer_set_text_color(s_unread_sms_layer, GColorBlack);

  // Improve the layout to be more like a watchface
  text_layer_set_font(s_unread_sms_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_unread_sms_layer, GTextAlignmentRight);
  text_layer_set_text(s_unread_sms_layer, "");

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_unread_sms_layer));
  
  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(00, 95, 144, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);

  // Improve the layout to be more like a watchface
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
  
  // Create date TextLayer
  s_date_layer = text_layer_create(GRect(12, 75, 144, 30));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  
  // Improve the layout to be more like a watchface
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentLeft);
  
  text_layer_set_text(s_date_layer, "");
  
  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));
  
  // Create not connected TextLayer
  s_not_connected_layer = text_layer_create(GRect(6, 50, 144, 40));
  text_layer_set_background_color(s_not_connected_layer, GColorClear);
  text_layer_set_text_color(s_not_connected_layer, GColorBlack);

  // Improve the layout to be more like a watchface
  text_layer_set_font(s_not_connected_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  
  text_layer_set_text(s_not_connected_layer, "not connected");
  layer_set_hidden((Layer *)s_not_connected_layer, true);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_not_connected_layer));
  

}

static void main_window_unload(Window *window) {
    // Destroy TextLayer
    text_layer_destroy(s_time_layer);
    text_layer_destroy(s_date_layer);

}


static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void init() {
  bt_notification_delay = 0;
  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  
  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
    
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  update_time();

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  bluetooth_connection_service_subscribe(bt_handler);


}

static void deinit() {
  bluetooth_connection_service_unsubscribe();

  
  // Destroy Window
  window_destroy(s_main_window);
  
}


int main(void) {
  init();
  app_event_loop();
  deinit();
}