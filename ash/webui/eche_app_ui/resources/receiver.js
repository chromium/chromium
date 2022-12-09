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
parentMessagePipe.registerHandler(Message.SCREEN_BACKLIGHT_STATE,
 async (message) => {
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

  onStreamAction(callback) {
    console.log('echeapi receiver.js onStreamAction');
    streamActionCallback = callback;
  }

  onReceivedVirtualKeyboardChanged(callback) {
    console.log('echeapi receiver.js onReceivedVirtualKeyboardChanged');
    virtualKeyboardCallback = callback;
  }

  onAndroidDeviceNetworkInfoChanged(callback) {
    console.log('echeapi receiver.js onAndroidDeviceNetworkInfoChanged');
    androidNetworkInfoCallback = callback;
  }
})();

// Declare module echeapi and bind the implementation to echeapi.d.ts
console.log('echeapi receiver.js start bind the implementation of echeapi');
const echeapi = {};
echeapi.webrtc = {};
echeapi.webrtc.sendSignal =
    EcheApiBindingImpl.sendWebRtcSignal.bind(EcheApiBindingImpl);
echeapi.webrtc.tearDownSignal =
    EcheApiBindingImpl.tearDownSignal.bind(EcheApiBindingImpl);
echeapi.webrtc.registerSignalReceiver =
    EcheApiBindingImpl.onWebRtcSignalReceived.bind(EcheApiBindingImpl);
echeapi.webrtc.closeWindow =
    EcheApiBindingImpl.closeWindow.bind(EcheApiBindingImpl);
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
echeapi.system.registerAndroidNetworkInfoChangedReceiver =
    EcheApiBindingImpl.onAndroidDeviceNetworkInfoChanged.bind(
        EcheApiBindingImpl);
window['echeapi'] = echeapi;
console.log('echeapi receiver.js finish bind the implementation of echeapi');
