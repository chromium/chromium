// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pass the query string params from the trusted URL.
const mainUrl = 'chrome-untrusted://eche-app/untrusted_index.html';
// The window.location.hash starts with # and window.location.search starts
// with ?, uses substring(1) to remove them.
let urlParams = window.location.hash ?
    new URLSearchParams(window.location.hash.substring(1)) :
    new URLSearchParams(window.location.search.substring(1));
urlParams = urlParams.toString();

let iframeUrl = mainUrl;
if (urlParams) {
  iframeUrl = mainUrl + '?' + urlParams;
}
document.getElementsByTagName('iframe')[0].src = iframeUrl;

// Returns a remote for SignalingMessageExchanger interface which sends messages
// to the browser.
const signalMessageExchanger =
    ash.echeApp.mojom.SignalingMessageExchanger.getRemote();
// An object which receives request messages for the SignalingMessageObserver
// mojom interface and dispatches them as callbacks.
const signalingMessageObserverRouter =
    new ash.echeApp.mojom.SignalingMessageObserverCallbackRouter();
// Set up a message pipe to talk to the browser process.
signalMessageExchanger.setSignalingMessageObserver(
    signalingMessageObserverRouter.$.bindNewPipeAndPassRemote());
// Returns a remote for SystemInfoProvider interface which gets system info
// from the browser.
const systemInfo = ash.echeApp.mojom.SystemInfoProvider.getRemote();
// Returns a remote for UidGenerator interface which gets an uid from the
// browser.
const uidGenerator = ash.echeApp.mojom.UidGenerator.getRemote();
// An object which receives request messages for the SystemInfoObserver
// mojom interface and dispatches them as callbacks.
const systemInfoObserverRouter =
    new ash.echeApp.mojom.SystemInfoObserverCallbackRouter();
// Set up a message pipe to the browser process to monitor screen state.
systemInfo.setSystemInfoObserver(
    systemInfoObserverRouter.$.bindNewPipeAndPassRemote());
// Returns the remote for AccessibilityProvider.
const accessibility = ash.echeApp.mojom.AccessibilityProvider.getRemote();
// An object that receives requests for the AccessibilityObserver mojom
// interface and dispatches them as callbacks. Setup the message
const accessibilityObserverRouter =
    new ash.echeApp.mojom.AccessibilityObserverCallbackRouter();
// Set up a message pipe to the browser process to accessibility actions.
accessibility.setAccessibilityObserver(
    accessibilityObserverRouter.$.bindNewPipeAndPassRemote());

const notificationGenerator =
    ash.echeApp.mojom.NotificationGenerator.getRemote();

const displayStreamHandler = ash.echeApp.mojom.DisplayStreamHandler.getRemote();

const streamOrientationObserver =
    ash.echeApp.mojom.StreamOrientationObserver.getRemote();

const connectionStatusObserver =
    ash.echeApp.mojom.ConnectionStatusObserver.getRemote();

const keyboardLayoutHandler =
    ash.echeApp.mojom.KeyboardLayoutHandler.getRemote();

const streamActionObserverRouter =
    new ash.echeApp.mojom.StreamActionObserverCallbackRouter();
// Set up a message pipe to the browser process to monitor stream action.
displayStreamHandler.setStreamActionObserver(
    streamActionObserverRouter.$.bindNewPipeAndPassRemote());

const keyboardLayoutObserverRouter =
    new ash.echeApp.mojom.KeyboardLayoutObserverCallbackRouter();
// Set up a message pipe to the browser process to monitor keyboard layout
// changes.
keyboardLayoutHandler.setKeyboardLayoutObserver(
    keyboardLayoutObserverRouter.$.bindNewPipeAndPassRemote());

/**
 * A pipe through which we can send messages to the guest frame.
 * Use an undefined `target` to find the <iframe> automatically.
 * Do not rethrow errors, since handlers installed here are expected to
 * throw exceptions that are handled on the other side of the pipe. And
 * nothing `awaits` async callHandlerForMessageType_(), so they will always
 * be reported as `unhandledrejection` and trigger a crash report.
 */
const guestMessagePipe = new MessagePipe(
    'chrome-untrusted://eche-app',
    /*target=*/ undefined,
    /*rethrow_errors=*/ false);

// Register bi-directional SEND_SIGNAL pipes.
guestMessagePipe.registerHandler(Message.SEND_SIGNAL, async (signal) => {
  console.log('echeapi browser_proxy.js sendSignalingMessage');
  signalMessageExchanger.sendSignalingMessage(signal);
});

guestMessagePipe.registerHandler(
    Message.ACCESSIBILITY_EVENT_DATA, async (event_data) => {
      console.log('echeapi browser_proxy.js handleAccessibilityEventReceived');
      accessibility.handleAccessibilityEventReceived(event_data);
    });

signalingMessageObserverRouter.onReceivedSignalingMessage.addListener(
    (signal) => {
      console.log('echeapi browser_proxy.js onReceivedSignalingMessage');
      guestMessagePipe.sendMessage(Message.SEND_SIGNAL, {
        /** @type {Uint8Array} */ signal,
      });
    });

// Register TEAR_DOWN_SIGNAL pipes.
guestMessagePipe.registerHandler(Message.TEAR_DOWN_SIGNAL, async () => {
  console.log('echeapi browser_proxy.js tearDownSignaling');
  signalMessageExchanger.tearDownSignaling();
});

// window.close() doesn't work from the iframe.
guestMessagePipe.registerHandler(Message.CLOSE_WINDOW, async () => {
  const info = /** @type {!SystemInfo} */ (await systemInfo.getSystemInfo());
  const systemInfoJson = structuredClone(info);
  console.log('echeapi browser_proxy.js window.close');
  displayStreamHandler.onStreamStatusChanged(
      ash.echeApp.mojom.StreamStatus.kStreamStatusStopped);
});

// Register GET_SYSTEM_INFO pipes for wrapping getSystemInfo async api call.
guestMessagePipe.registerHandler(Message.GET_SYSTEM_INFO, async () => {
  console.log('echeapi browser_proxy.js getSystemInfo');
  return /** @type {!SystemInfo} */ (await systemInfo.getSystemInfo());
});

// Register GET_UID pipes for wrapping getUid async api call.
guestMessagePipe.registerHandler(Message.GET_UID, async () => {
  console.log('echeapi browser_proxy.js getUid');
  return /** @type {!UidInfo} */ (await uidGenerator.getUid());
});

guestMessagePipe.registerHandler(Message.IS_ACCESSIBILITY_ENABLED, async () => {
  const result = await accessibility.isAccessibilityEnabled();
  return {result: result.enabled};
});

// Add Screen Backlight state listener and send state via pipes.
systemInfoObserverRouter.onScreenBacklightStateChanged.addListener((state) => {
  console.log('echeapi browser_proxy.js onScreenBacklightStateChanged');
  guestMessagePipe.sendMessage(Message.SCREEN_BACKLIGHT_STATE, {
    /** @type {number} */ state,
  });
});

// Add tablet mode listener and send result via pipes.
systemInfoObserverRouter.onReceivedTabletModeChanged.addListener(
    (isTabletMode) => {
      console.log('echeapi browser_proxy.js onReceivedTabletModeChanged');
      guestMessagePipe.sendMessage(Message.TABLET_MODE, {
        /** @type {boolean} */ isTabletMode,
      });
    });

// Add Android network info listener and send result via pipes.
systemInfoObserverRouter.onAndroidDeviceNetworkInfoChanged.addListener(
    (isDifferentNetwork, androidDeviceOnCellular) => {
      console.log('echeapi browser_proxy.js onAndroidDeviceNetworkInfoChanged');
      guestMessagePipe.sendMessage(Message.ANDROID_NETWORK_INFO, {
        /** @type {boolean} */ isDifferentNetwork,
        /** @type {boolean} */ androidDeviceOnCellular,
      });
    });

accessibilityObserverRouter.enableAccessibilityTreeStreaming.addListener(
    (enabled) => {
      console.log('echeapi browser_proxy.js enableAccessibilityTreeStreaming');
      guestMessagePipe.sendMessage(
          Message.ACCESSIBILITY_SET_TREE_STREAMING_ENABLED, {enabled});
    });

accessibilityObserverRouter.enableExploreByTouch.addListener((enabled) => {
  console.log('echeapi browser_proxy.js enableExploreByTouch');
  guestMessagePipe.sendMessage(
      Message.ACCESSIBILITY_SET_EXPLORE_BY_TOUCH_ENABLED, {enabled});
});

accessibilityObserverRouter.performAction.addListener((action) => {
  return new Promise(async (resolve) => {
    const result = await guestMessagePipe.sendMessage(
        Message.ACCESSIBILITY_PERFORM_ACTION, action);
    // It appears as though false is sent as an empty object. Likely due to
    // proto omitting the value when it is false.
    const payload = typeof result == 'boolean' ? result : false;
    // For mojom to understand what to do, a result key is required.
    resolve({result: payload});
  });
});

accessibilityObserverRouter.refreshWithExtraData.addListener((action) => {
  return new Promise(async (resolve) => {
    const result = await guestMessagePipe.sendMessage(
        Message.ACCESSIBILITY_REFRESH_WITH_EXTRA_DATA, action);
    resolve({result});
  });
});

// Add stream action listener and send result via pipes.
streamActionObserverRouter.onStreamAction.addListener((action) => {
  console.log(`echeapi browser_proxy.js OnStreamAction ${action}`);
  guestMessagePipe.sendMessage(Message.STREAM_ACTION, {
    /** @type {number} */ action,
  });
});

// Add keyboard listener and send result via pipes.
keyboardLayoutObserverRouter.onKeyboardLayoutChanged.addListener(
    (id, longName, shortName, layoutTag) => {
      console.log('echeapi browser_proxy.js onKeyboardLayoutChanged');
      guestMessagePipe.sendMessage(Message.KEYBOARD_LAYOUT_INFO, {
        /** @type {string} */ id,
        /** @type {string} */ longName,
        /** @type {string} */ shortName,
        /** @type {string} */ layoutTag,
      });
    });

guestMessagePipe.registerHandler(Message.SHOW_NOTIFICATION, async (message) => {
  // The C++ layer uses std::u16string, which use 16 bit characters. JS
  // strings support either 8 or 16 bit characters, and must be converted
  // to an array of 16 bit character codes that match std::u16string.
  const titleArray = {
    data: Array.from(message.title, (c) => c.charCodeAt()),
  };
  const messageArray = {
    data: Array.from(message.message, (c) => c.charCodeAt()),
  };
  console.log('echeapi browser_proxy.js showNotification');
  notificationGenerator.showNotification(
      titleArray, messageArray, message.notificationType);
});

guestMessagePipe.registerHandler(Message.SHOW_TOAST, async (message) => {
  // The C++ layer uses std::u16string, which use 16 bit characters. JS
  // strings support either 8 or 16 bit characters, and must be converted
  // to an array of 16 bit character codes that match std::u16string.
  const textArray = {data: Array.from(message.text, (c) => c.charCodeAt())};
  console.log('echeapi browser_proxy.js showToast');
  notificationGenerator.showToast(textArray);
});

guestMessagePipe.registerHandler(
    Message.TIME_HISTOGRAM_MESSAGE, async (message) => {
      console.log('echeapi browser_proxy.js recordTime');
      const histogramData = /** @type {TimeHistogram} */ (message);
      chrome.metricsPrivate.recordTime(
          histogramData.histogram, histogramData.value);
    });

guestMessagePipe.registerHandler(
    Message.ENUM_HISTOGRAM_MESSAGE, async (message) => {
      console.log('echeapi browser_proxy.js recordEnumerationValue');
      const histogramData = /** @type {EnumHistogram} */ (message);
      chrome.metricsPrivate.recordEnumerationValue(
          histogramData.histogram, histogramData.value, histogramData.maxValue);
    });

// Register START_STREAMING pipes.
guestMessagePipe.registerHandler(Message.START_STREAMING, async () => {
  console.log('echeapi browser_proxy.js startStreaming');
  displayStreamHandler.onStreamStatusChanged(
      ash.echeApp.mojom.StreamStatus.kStreamStatusStarted);
});

// Register CHANGE_ORIENTATION.
guestMessagePipe.registerHandler(
    Message.CHANGE_ORIENTATION, async (message) => {
      console.log(
          `echeapi browser_proxy.js ` +
          `onStreamOrientationChanged ${message.isLandscape}`);
      streamOrientationObserver.onStreamOrientationChanged(message.isLandscape);
    });

// Register CONNECTION_STATUS_CHANGED.
guestMessagePipe.registerHandler(
    Message.CONNECTION_STATUS_CHANGED, async (message) => {
      console.log(
          `echeapi browser_proxy.js ` +
          `onConnectionStatusChanged ${message.connectionStatus}`);
      connectionStatusObserver.onConnectionStatusChanged(
          message.connectionStatus);
    });

// Register KEYBOARD_LAYOUT_REQUEST pipes.
guestMessagePipe.registerHandler(Message.KEYBOARD_LAYOUT_REQUEST, async () => {
  console.log('echeapi browser_proxy.js requestCurrentKeyboardLayout');
  keyboardLayoutHandler.requestCurrentKeyboardLayout();
});

// We can't access hash change event inside iframe so parse the notification
// info from the anchor part of the url when hash is changed and send them to
// untrusted section via message pipes.
function locationHashChanged() {
  const urlParams = window.location.hash ?
      new URLSearchParams(window.location.hash.substring(1)) :
      new URLSearchParams(window.location.search.substring(1));
  const notificationId = urlParams.get('notification_id');
  const packageName = urlParams.get('package_name');
  const timestamp = urlParams.get('timestamp');
  const userId = urlParams.get('user_id');
  const notificationInfo = /** @type {!NotificationInfo} */ ({
    notificationId,
    packageName,
    timestamp,
    userId,
  });
  guestMessagePipe.sendMessage(Message.NOTIFICATION_INFO, notificationInfo);
}

window.onhashchange = locationHashChanged;

if ('virtualKeyboard' in navigator) {
  navigator['virtualKeyboard'].overlaysContent = true;
  navigator['virtualKeyboard'].addEventListener('geometrychange', (event) => {
    const {x, y, width, height} = event.target['boundingRect'];
    console.log('Virtual keyboard geometry:', x, y, width, height);
    const isVirtualKeyboardEnabled = width > 0 && height > 0;
    guestMessagePipe.sendMessage(Message.IS_VIRTUAL_KEYBOARD_ENABLED, {
      /** @type {boolean} */ isVirtualKeyboardEnabled,
    });
  });
} else {
  console.log('virtual keyboard is not supported!');
}
