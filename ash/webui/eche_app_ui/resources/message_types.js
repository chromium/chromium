// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Message definitions passed over the Eche privileged/unprivileged pipe.
 */

/**
 * Representation of an event passed in from the phone notification.
 * @typedef {{
 *    notificationId: string,
 *    packageName: string,
 *    timestamp: number,
 * }}
 */
/* #export */ let NotificationInfo;

/**
 * Representation of the system info passed in from SystemInfoProvider.
 * @typedef {{
 *    boardName: string,
 *    deviceName: string,
 *    tabletMode: boolean,
 *    wifiConnectionState: string,
 *    debugMode: boolean,
 *    MeasureLatency: boolean,
 *    sendStartSignaling: boolean,
 *    disable_stun_server: boolean,
 *    check_android_network_info: boolean,
 *    process_android_accessibility_tree: boolean
 * }}
 */
/* #export */ let SystemInfo;

/**
 * Representation of the uid from local device.
 * @typedef {{
 *    localUid: string,
 * }}
 */
/* #export */ let UidInfo;

/**
 * Representation of the metrics data for recording an elapsed time to the
 * given histogram name. The histogram should be defined in
 * google3/analysis/uma/configs/chrome/histograms.xml
 * @typedef {{
 *    histogram: string,
 *    value: number,
 * }}
 */
/* #export */ let TimeHistogram;

/**
 * Representation of the metrics data for recording in an enumeration
 * value to the given histogram name. The histogram and the enumeration should
 * be defined in
 * google3/analysis/uma/configs/chrome/histograms.xml
 * @typedef {{
 *    histogram: string,
 *    value: number,
 *    maxValue: number,
 * }}
 */
/* #export */ let EnumHistogram;

/**
 * A number that represents the action to control stream.
 * @typedef {number} StreamAction
 */
/* #export */ let StreamAction;

/**
 * Enum for message types.
 * @enum {string}
 */
/* #export */ const Message = {
  // Message for sending window close request to privileged section.
  CLOSE_WINDOW: 'close-window',
  // Message for sending signaling data in bi-directional pipes.
  SEND_SIGNAL: 'send-signal',
  // Message for sending tear down signal request to privileged section.
  TEAR_DOWN_SIGNAL: 'tear-down-signal',
  // Message for getting the result of getSystemInfo api from privileged
  // section.
  GET_SYSTEM_INFO: 'get-system-info',
  // Message for getting the result of getUid api from privileged section.
  GET_UID: 'get-uid',
  // Message for sending screen backlight state to unprivileged section.
  SCREEN_BACKLIGHT_STATE: 'screen-backlight-state',
  // Message for sending tablet mode state to unprivileged section.
  TABLET_MODE: 'tablet-mode',
  // Message for sending notification event to unprivileged section.
  NOTIFICATION_INFO: 'notification_info',
  // Message for sending notification data in bi-directional pipes.
  SHOW_NOTIFICATION: 'show_notification',
  // Message for sending toast data.
  SHOW_TOAST: 'show_toast',
  // Message for sending metrics data for recording time histogram.
  TIME_HISTOGRAM_MESSAGE: 'time_histagram_message',
  // Message for sending metrics data for recording enum histogram.
  ENUM_HISTOGRAM_MESSAGE: 'enum_histagram_message',
  // Message for starting the display video of Eche.
  START_STREAMING: 'start_streaming',
  // Message for stream action
  STREAM_ACTION: 'stream_action',
  // Message for virtual keyboard state
  IS_VIRTUAL_KEYBOARD_ENABLED: 'is_virtual_keyboard_enabled',
  // Message for accessibility state
  IS_ACCESSIBILITY_ENABLED: 'is_accessibility_enabled',
  // Message for Android network info
  ANDROID_NETWORK_INFO: 'android-network-info',
  // Message for changing app stream orientation
  CHANGE_ORIENTATION: 'change_orientation',
  // Message for notifying Chrome OS about a change in the status of the WebRTC
  // connection.
  CONNECTION_STATUS_CHANGED: 'connection_status_changed',
  // Enable or disable accessibility tree streaming.
  ACCESSIBILITY_SET_TREE_STREAMING_ENABLED:
      'accessibility_set_tree_streaming_enabled',
  // Enable or disable explore by touch.
  ACCESSIBILITY_SET_EXPLORE_BY_TOUCH_ENABLED:
      'accessibility_set_explore_by_touch_enabled',
  // Message for sending accessibility event data.
  ACCESSIBILITY_EVENT_DATA: 'accessibility_event_data',
  // Message for getting the location of text in android.
  ACCESSIBILITY_REFRESH_WITH_EXTRA_DATA:
      'accessibility_refresh_with_extra_data',
  // Message for sending actions and their parameters.
  ACCESSIBILITY_PERFORM_ACTION: 'accessibility_perform_action',
  // Message for requesting keyboard layout information.
  KEYBOARD_LAYOUT_REQUEST: 'keyboard_layout_request',
  // Message for sending keyboard layout information.
  KEYBOARD_LAYOUT_INFO: 'keyboard_layout_info',
  // Message for processing Android device accessibility tree
  PROCESS_ANDROID_ACCESSIBILITY_TREE: 'process_android_accessibility_tree',
};
