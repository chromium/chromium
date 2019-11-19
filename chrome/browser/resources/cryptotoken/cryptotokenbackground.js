// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview CryptoToken background page
 */

'use strict';

/** @const */
var HTTP_ORIGINS_ALLOWED = false;

/** @const */
var LOG_SAVER_EXTENSION_ID = 'fjajfjhkeibgmiggdfehjplbhmfkialk';

var FACTORY_REGISTRY = (function() {
  var windowTimer = new WindowTimer();
  var xhrTextFetcher = new XhrTextFetcher();
  return new FactoryRegistry(
      new XhrAppIdCheckerFactory(xhrTextFetcher),
      new CryptoTokenApprovedOrigin(), new CountdownTimerFactory(windowTimer),
      new CryptoTokenOriginChecker(), windowTimer);
})();

/**
 * @param {*} request The received request
 * @return {boolean} Whether the request is a register/enroll request.
 */
function isRegisterRequest(request) {
  if (!request) {
    return false;
  }
  switch (request.type) {
    case MessageTypes.U2F_REGISTER_REQUEST:
      return true;

    default:
      return false;
  }
}

/**
 * Default response callback to deliver a response to a request.
 * @param {*} request The received request.
 * @param {function(*): void} sendResponse A callback that delivers a response.
 * @param {*} response The response to return.
 */
function defaultResponseCallback(request, sendResponse, response) {
  response['requestId'] = request['requestId'];
  try {
    sendResponse(response);
  } catch (e) {
    console.warn(UTIL_fmt('caught: ' + e.message));
  }
}

/**
 * Response callback that delivers a response to a request only when the
 * sender is a foreground tab.
 * @param {*} request The received request.
 * @param {!MessageSender} sender The message sender.
 * @param {function(*): void} sendResponse A callback that delivers a response.
 * @param {*} response The response to return.
 */
function sendResponseToActiveTabOnly(request, sender, sendResponse, response) {
  // For WebAuthn-proxied requests on Windows, dismissing the native Windows
  // UI after a timeout races with the error being returned here. Hence, skip
  // the focus check for all timeouts.
  if (response.responseData &&
      response.responseData.errorCode == ErrorCodes.TIMEOUT) {
    defaultResponseCallback(request, sendResponse, response);
    return;
  }

  tabInForeground(sender.tab.id).then(function(result) {
    // If the tab is no longer in the foreground, drop the result: the user
    // is no longer interacting with the tab that originated the request.
    if (result) {
      defaultResponseCallback(request, sendResponse, response);
    }
  });
}

/**
 * Common handler for messages received from chrome.runtime.sendMessage and
 * chrome.runtime.connect + postMessage.
 * @param {*} request The received request
 * @param {!MessageSender} sender The message sender
 * @param {function(*): void} sendResponse A callback that delivers a response
 * @return {Closeable} A Closeable request handler.
 */
function messageHandler(request, sender, sendResponse) {
  var responseCallback;
  if (isRegisterRequest(request)) {
    responseCallback =
        sendResponseToActiveTabOnly.bind(null, request, sender, sendResponse);
  } else {
    responseCallback =
        defaultResponseCallback.bind(null, request, sendResponse);
  }
  var closeable = handleWebPageRequest(
      /** @type {Object} */ (request), sender, responseCallback);
  return closeable;
}

/**
 * Listen to individual messages sent from (whitelisted) webpages via
 * chrome.runtime.sendMessage
 * @param {*} request The received request
 * @param {!MessageSender} sender The message sender
 * @param {function(*): void} sendResponse A callback that delivers a response
 * @return {boolean}
 */
function messageHandlerExternal(request, sender, sendResponse) {
  if (sender.id && sender.id === LOG_SAVER_EXTENSION_ID) {
    return handleLogSaverMessage(request);
  }

  messageHandler(request, sender, sendResponse);
  return true;  // Tell Chrome not to destroy sendResponse yet
}
chrome.runtime.onMessageExternal.addListener(messageHandlerExternal);

// Listen to direct connection events, and wire up a message handler on the port
chrome.runtime.onConnectExternal.addListener(function(port) {
  function sendResponse(response) {
    port.postMessage(response);
  }

  var closeable;
  port.onMessage.addListener(function(request) {
    var sender = /** @type {!MessageSender} */ (port.sender);
    closeable = messageHandler(request, sender, sendResponse);
  });
  port.onDisconnect.addListener(function() {
    if (closeable) {
      closeable.close();
    }
  });
});

/**
 * Handles messages from the log-saver app. Temporarily replaces UTIL_fmt with
 * a wrapper that also sends formatted messages to the app.
 * @param {*} request The message received from the app
 * @return {boolean} Used as chrome.runtime.onMessage handler return value
 */
function handleLogSaverMessage(request) {
  if (request === 'start') {
    if (originalUtilFmt) {
      // We're already sending
      return false;
    }
    originalUtilFmt = UTIL_fmt;
    UTIL_fmt = function(s) {
      var line = originalUtilFmt(s);
      chrome.runtime.sendMessage(LOG_SAVER_EXTENSION_ID, line);
      return line;
    };
  } else if (request === 'stop') {
    if (originalUtilFmt) {
      UTIL_fmt = originalUtilFmt;
      originalUtilFmt = null;
    }
  }
  return false;
}

/** @private */
var originalUtilFmt = null;
