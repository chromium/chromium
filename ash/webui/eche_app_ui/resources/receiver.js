// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const parentMessagePipe = new MessagePipe('chrome://eche-app', window.parent);

let signalingCallback = null;
parentMessagePipe.registerHandler(Message.SEND_SIGNAL, async (message) => {
  if (!signalingCallback) {
    return;
  }
  signalingCallback(/** @type {Uint8Array} */ (message.signal));
});

let screenBacklightCallback = null;
parentMessagePipe.registerHandler(
    Message.SCREEN_BACKLIGHT_STATE, async (message) => {
      if (!screenBacklightCallback) {
        return;
      }
      screenBacklightCallback(/** @type {number} */ (message.state));
    });

let tabletModeCallback = null;
parentMessagePipe.registerHandler(Message.TABLET_MODE, async (message) => {
  if (!tabletModeCallback) {
    return;
  }
  tabletModeCallback(/** @type {boolean} */ (message.isTabletMode));
});

let notificationCallback = null;
parentMessagePipe.registerHandler(
    Message.NOTIFICATION_INFO, async (message) => {
      if (!notificationCallback) {
        return;
      }
      notificationCallback(/** @type {!NotificationInfo} */ (message));
    });

let streamActionCallback = null;
parentMessagePipe.registerHandler(Message.STREAM_ACTION, async (message) => {
  if (!streamActionCallback) {
    return;
  }
  streamActionCallback(/** @type {!StreamAction} */ (message.action));
});

let keyboardLayoutChangedCallback = null;
parentMessagePipe.registerHandler(
    Message.KEYBOARD_LAYOUT_INFO, async (message) => {
      if (!keyboardLayoutChangedCallback) {
        return;
      }
      keyboardLayoutChangedCallback(
          /** @type {string} */ (message.id),
          /** @type {string} */ (message.longName),
          /** @type {string} */ (message.shortName),
          /** @type {string} */ (message.layoutTag));
    });

let virtualKeyboardCallback = null;
parentMessagePipe.registerHandler(
    Message.IS_VIRTUAL_KEYBOARD_ENABLED, async (message) => {
      if (!virtualKeyboardCallback || !message.isVirtualKeyboardEnabled) {
        return;
      }

      virtualKeyboardCallback(
          /** @type {boolean} */ (message.isVirtualKeyboardEnabled));
    });

let androidNetworkInfoCallback = null;
parentMessagePipe.registerHandler(
    Message.ANDROID_NETWORK_INFO, async (message) => {
      if (!androidNetworkInfoCallback) {
        return;
      }

      androidNetworkInfoCallback(
          /** @type {boolean} */ (message.isDifferentNetwork),
          /** @type {boolean} */ (message.androidDeviceOnCellular));
    });

let setAccessibilityEnabledCallback = null;
parentMessagePipe.registerHandler(
    Message.ACCESSIBILITY_SET_TREE_STREAMING_ENABLED, (payload) => {
      if (setAccessibilityEnabledCallback) {
        setAccessibilityEnabledCallback(payload.enabled);
      }
    });

let setExploreByTouchEnabledCallback = null;
parentMessagePipe.registerHandler(
    Message.ACCESSIBILITY_SET_EXPLORE_BY_TOUCH_ENABLED, (payload) => {
      if (setExploreByTouchEnabledCallback) {
        setExploreByTouchEnabledCallback(payload.enabled);
      }
    });

// Handle accessibility perform action.
let performActionCallback = null;
parentMessagePipe.registerHandler(
    Message.ACCESSIBILITY_PERFORM_ACTION, async (action) => {
      if (!performActionCallback) {
        return Promise.resolve(false);
      }
      return performActionCallback(/** @type {Uint8Array} */ (action));
    });
let refreshWithExtraDataCallback = null;
parentMessagePipe.registerHandler(
    Message.ACCESSIBILITY_REFRESH_WITH_EXTRA_DATA, async (action) => {
      if (!refreshWithExtraDataCallback) {
        return Promise.resolve(null);
      }
      return refreshWithExtraDataCallback(/** @type {Uint8Array} */ (action));
    });

// The implementation of echeapi.d.ts
const EcheApiBindingImpl = new (class {
  closeWindow() {
    console.log('echeapi receiver.js closeWindow');
    parentMessagePipe.sendMessage(Message.CLOSE_WINDOW);
  }

  onWebRtcSignalReceived(callback) {
    console.log('echeapi receiver.js onWebRtcSignalReceived');
    signalingCallback = callback;
  }

  sendWebRtcSignal(signaling) {
    console.log('echeapi receiver.js sendWebRtcSignal');
    parentMessagePipe.sendMessage(Message.SEND_SIGNAL, signaling);
  }

  tearDownSignal() {
    console.log('echeapi receiver.js tearDownSignal');
    parentMessagePipe.sendMessage(Message.TEAR_DOWN_SIGNAL);
  }

  getSystemInfo() {
    console.log('echeapi receiver.js getSystemInfo');
    return /** @type {!SystemInfo} */ (
      parentMessagePipe.sendMessage(Message.GET_SYSTEM_INFO));
  }

  getLocalUid() {
    console.log('echeapi receiver.js getLocalUid');
    return /** @type {!UidInfo} */ (
      parentMessagePipe.sendMessage(Message.GET_UID));
  }

  isAccessibilityEnabled() {
    console.log('echeapi receiver.js isAccessibilityEnabled');
    return new Promise((resolve, reject) => {
      parentMessagePipe.sendMessage(Message.IS_ACCESSIBILITY_ENABLED)
          .then(payload => {
            resolve(payload.result);
          }, reject);
    });
  }

  onScreenBacklightStateChanged(callback) {
    console.log('echeapi receiver.js onScreenBacklightStateChanged');
    screenBacklightCallback = callback;
  }

  onReceivedTabletModeChanged(callback) {
    console.log('echeapi receiver.js onReceivedTabletModeChanged');
    tabletModeCallback = callback;
  }

  onReceivedNotification(callback) {
    console.log('echeapi receiver.js onReceivedNotification');
    notificationCallback = callback;
  }

  showNotification(title, message, notificationType) {
    console.log('echeapi receiver.js showNotification');
    parentMessagePipe.sendMessage(
        Message.SHOW_NOTIFICATION, {title, message, notificationType});
  }

  showToast(text) {
    console.log('echeapi receiver.js showToast');
    parentMessagePipe.sendMessage(Message.SHOW_TOAST, {text});
  }

  startStreaming() {
    console.log('echeapi receiver.js startStreaming');
    parentMessagePipe.sendMessage(Message.START_STREAMING);
  }

  sendTimeHistogram(histogram, value) {
    console.log('echeapi receiver.js sendTimeHistogram');
    parentMessagePipe.sendMessage(
        Message.TIME_HISTOGRAM_MESSAGE, {histogram, value});
  }

  sendEnumHistogram(histogram, value, maxValue) {
    console.log('echeapi receiver.js sendEnumHistogram');
    parentMessagePipe.sendMessage(
        Message.ENUM_HISTOGRAM_MESSAGE, {histogram, value, maxValue});
  }

  sendAccessibilityEventData(event) {
    console.log('echeapi receiver.js sendAccessibilityEventData');
    parentMessagePipe.sendMessage(Message.ACCESSIBILITY_EVENT_DATA, event);
  }

  onStreamAction(callback) {
    console.log('echeapi receiver.js onStreamAction');
    streamActionCallback = callback;
  }

  onStreamOrientationChanged(isLandscape) {
    console.log(
        'echeapi receiver.js onStreamOrientationChanged, landscape=' +
        isLandscape);
    parentMessagePipe.sendMessage(Message.CHANGE_ORIENTATION, {isLandscape});
  }

  onConnectionStatusChanged(connectionStatus) {
    console.log(
        `echeapi receiver.js onConnectionStatusChanged, connectionStatus=` +
        connectionStatus);
    parentMessagePipe.sendMessage(
        Message.CONNECTION_STATUS_CHANGED, {connectionStatus});
  }

  requestCurrentKeyboardLayout() {
    console.log('echeapi receiver.js requestCurrentKeyboardLayout');
    parentMessagePipe.sendMessage(Message.KEYBOARD_LAYOUT_REQUEST);
  }

  onReceivedVirtualKeyboardChanged(callback) {
    console.log('echeapi receiver.js onReceivedVirtualKeyboardChanged');
    virtualKeyboardCallback = callback;
  }

  onAndroidDeviceNetworkInfoChanged(callback) {
    console.log('echeapi receiver.js onAndroidDeviceNetworkInfoChanged');
    androidNetworkInfoCallback = callback;
  }

  onKeyboardLayoutChanged(callback) {
    console.log('echeapi receiver.js onKeyboardLayoutChanged');
    keyboardLayoutChangedCallback = callback;
  }

  // TODO: rename this and similar methods to set'Xxx'Callback
  onAccessibilityEnabledStateChanged(callback) {
    console.log('echeapi receiver.js onAccessibilityEnabledStateChanged');
    setAccessibilityEnabledCallback = callback;
  }

  // TODO: rename this and similar methods to set'Xxx'Callback
  onPerformAction(callback) {
    console.log('echeapi receiver.js onPerformAction');
    performActionCallback = callback;
  }

  registerRefreshWithExtraDataCallback(callback) {
    console.log('echeapi receiver.js registerRefreshWithExtraDataCallback');
    refreshWithExtraDataCallback = callback;
  }

  registerSetExploreByTouchEnabledCallback(callback) {
    console.log('echeapi receiver.js registerSetExploreByTouchEnabledCallback');
    setExploreByTouchEnabledCallback = callback;
  }
})();

// Declare module echeapi and bind the implementation to echeapi.d.ts
console.log('echeapi receiver.js start bind the implementation of echeapi');
const echeapi = {};
// webrtc
echeapi.webrtc = {};
echeapi.webrtc.sendSignal =
  EcheApiBindingImpl.sendWebRtcSignal.bind(EcheApiBindingImpl);
echeapi.webrtc.tearDownSignal =
    EcheApiBindingImpl.tearDownSignal.bind(EcheApiBindingImpl);
echeapi.webrtc.registerSignalReceiver =
    EcheApiBindingImpl.onWebRtcSignalReceived.bind(EcheApiBindingImpl);
echeapi.webrtc.closeWindow =
    EcheApiBindingImpl.closeWindow.bind(EcheApiBindingImpl);
// accessibility
echeapi.accessibility = {};
echeapi.accessibility.sendAccessibilityEventData =
  EcheApiBindingImpl.sendAccessibilityEventData.bind(EcheApiBindingImpl);
echeapi.accessibility.isAccessibilityEnabled =
    EcheApiBindingImpl.isAccessibilityEnabled.bind(EcheApiBindingImpl);
echeapi.accessibility.registerAccessibilityEnabledStateChangedReceiver =
    EcheApiBindingImpl.onAccessibilityEnabledStateChanged.bind(
        EcheApiBindingImpl);
echeapi.accessibility.registerExploreByTouchEnabledStateChangedReceiver =
    EcheApiBindingImpl.registerSetExploreByTouchEnabledCallback.bind(
        EcheApiBindingImpl);
echeapi.accessibility.registerPerformActionReceiver =
  EcheApiBindingImpl.onPerformAction.bind(EcheApiBindingImpl);
echeapi.accessibility.registerRefreshWithExtraDataReceiver =
    EcheApiBindingImpl.registerRefreshWithExtraDataCallback.bind(
        EcheApiBindingImpl);
// system
echeapi.system = {};
echeapi.system.getLocalUid =
    EcheApiBindingImpl.getLocalUid.bind(EcheApiBindingImpl);
echeapi.system.getSystemInfo =
    EcheApiBindingImpl.getSystemInfo.bind(EcheApiBindingImpl);
echeapi.system.registerScreenBacklightState =
    EcheApiBindingImpl.onScreenBacklightStateChanged.bind(EcheApiBindingImpl);
echeapi.system.registerTabletModeChangedReceiver =
    EcheApiBindingImpl.onReceivedTabletModeChanged.bind(EcheApiBindingImpl);
echeapi.system.registerNotificationReceiver =
    EcheApiBindingImpl.onReceivedNotification.bind(EcheApiBindingImpl);
echeapi.system.showCrOSNotification =
    EcheApiBindingImpl.showNotification.bind(EcheApiBindingImpl);
echeapi.system.showToast =
    EcheApiBindingImpl.showToast.bind(EcheApiBindingImpl);
echeapi.system.startStreaming =
    EcheApiBindingImpl.startStreaming.bind(EcheApiBindingImpl);
echeapi.system.sendTimeHistogram =
    EcheApiBindingImpl.sendTimeHistogram.bind(EcheApiBindingImpl);
echeapi.system.sendEnumHistogram =
    EcheApiBindingImpl.sendEnumHistogram.bind(EcheApiBindingImpl);
echeapi.system.registerStreamActionReceiver =
    EcheApiBindingImpl.onStreamAction.bind(EcheApiBindingImpl);
echeapi.system.registerVirtualKeyboardChangedReceiver =
    EcheApiBindingImpl.onReceivedVirtualKeyboardChanged.bind(
        EcheApiBindingImpl);
echeapi.system.registerKeyboardLayoutChangedReceiver =
    EcheApiBindingImpl.onKeyboardLayoutChanged.bind(EcheApiBindingImpl);
echeapi.system.registerAndroidNetworkInfoChangedReceiver =
    EcheApiBindingImpl.onAndroidDeviceNetworkInfoChanged.bind(
        EcheApiBindingImpl);
echeapi.system.onStreamOrientationChanged =
    EcheApiBindingImpl.onStreamOrientationChanged.bind(EcheApiBindingImpl);
echeapi.system.onConnectionStatusChanged =
    EcheApiBindingImpl.onConnectionStatusChanged.bind(EcheApiBindingImpl);
echeapi.system.requestCurrentKeyboardLayout =
    EcheApiBindingImpl.requestCurrentKeyboardLayout.bind(EcheApiBindingImpl);
window['echeapi'] = echeapi;
console.log('echeapi receiver.js finish bind the implementation of echeapi');
