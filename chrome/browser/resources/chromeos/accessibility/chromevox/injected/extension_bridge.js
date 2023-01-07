// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bridge to aid in communication between a Chrome
 * background page and content script.
 */

goog.provide('ContentExtensionBridge');

ContentExtensionBridge = class {
  /** @private */
  constructor() {
    /** @private {!Array<!function()>} */
    this.disconnectListeners_ = [];
    /** @private {number} */
    this.id_ = -1;
    /** @private {boolean} */
    this.connected_ = false;
    /** @private {number} */
    this.pingAttempts_ = 0;
    /** @private {!Array<Object>} */
    this.queuedMessages_ = [];
    /** @private {?Port} */
    this.backgroundPort_ = null;

    this.init_();
  }

  /** Initialize the extension bridge. */
  static init() {
    ContentExtensionBridge.instance = new ContentExtensionBridge();
  }

  /** @param {Object} message The message to be sent. */
  static send(message) {
    ContentExtensionBridge.instance.send_(message);
  }

  /**
   * Provide a function to be called when the connection is
   * disconnected.
   * @param {function()} listener The listener.
   */
  static addDisconnectListener(listener) {
    ContentExtensionBridge.instance.disconnectListeners_.push(listener);
  }

  /**
   * Initialize the extension bridge in a content script context, listening
   * for messages from the background page.
   * @private
   */
  init_() {
    // Listen to requests from the background that don't come from
    // our connection port.
    chrome.extension.onMessage.addListener(
        (request, sender, respond) =>
            this.onSetupMessage_(request, sender, respond));

    this.setupBackgroundPort_();
    this.tryToPingBackgroundPage_();
  }

  /**
   * @param {Object} request
   * @param {*} sender
   * @param {function(*)} sendResponse
   * @private
   */
  onSetupMessage_(request, sender, sendResponse) {
    this.onMessage_(request);
    sendResponse({});
  }

  /**
   * Set up the connection to the background page.
   * @private
   */
  setupBackgroundPort_() {
    this.backgroundPort_ =
        chrome.extension.connect({name: ContentExtensionBridge.PORT_NAME});
    if (!this.backgroundPort_) {
      return;
    }
    this.backgroundPort_.onMessage.addListener(
        message => this.onMessage_(message));
    this.backgroundPort_.onDisconnect.addListener(() => this.onDisconnect_());
  }

  /**
   * @param {Object} message
   * @private
   */
  onMessage_(message) {
    if (message[ContentExtensionBridge.PONG_MSG]) {
      this.gotPongFromBackgroundPage_(message[ContentExtensionBridge.PONG_MSG]);
    }
  }

  /** @private */
  onDisconnect_() {
    // If we're not connected yet, don't give up - try again.
    if (!this.connected_) {
      this.backgroundPort_ = null;
      return;
    }

    this.backgroundPort_ = null;
    this.connected_ = false;
    this.disconnectListeners_.forEach(listener => listener());
  }

  /**
   * Try to ping the background page.
   * @private
   */
  tryToPingBackgroundPage_() {
    // If we already got a pong, great - we're done.
    if (this.connected_) {
      return;
    }

    this.pingAttempts_++;
    if (this.pingAttempts_ > ContentExtensionBridge.MAX_PING_ATTEMPTS) {
      // Could not connect after several ping attempts. Call the disconnect
      // handlers, which will disable ChromeVox.
      this.disconnectListeners_.forEach(listener => listener());
      return;
    }

    // Send the ping.
    const msg = {
      [ContentExtensionBridge.PING_MSG]: 1,
    };

    if (!this.backgroundPort_) {
      this.setupBackgroundPort_();
    }
    if (this.backgroundPort_) {
      this.backgroundPort_.postMessage(msg);
    }

    // Check again after a short while in case we get no response.
    setTimeout(
        () => this.tryToPingBackgroundPage_(),
        ContentExtensionBridge.TIME_BETWEEN_PINGS_MS);
  }

  /**
   * Got pong from the background page, now we know the connection was
   * successful.
   * @param {number} pongId unique id assigned to us by the background page
   * @private
   */
  gotPongFromBackgroundPage_(pongId) {
    this.connected_ = true;
    this.id_ = pongId;

    while (this.queuedMessages_.length > 0) {
      this.send_(this.queuedMessages_.shift());
    }
  }

  /**
   * Send a message from the content script to the background page.
   * @param {Object} message The message to send.
   * @private
   */
  send_(message) {
    if (!this.connected_) {
      // We're not connected to the background page, so queue this message
      // until we're connected.
      this.queuedMessages_.push(message);
      return;
    }

    if (this.backgroundPort_) {
      this.backgroundPort_.postMessage(message);
    } else {
      chrome.extension.sendMessage(message);
    }
  }
};

// Keep these constants in sync with common/extension_bridge.js.

/**
 * The name of the port between the content script and background page.
 * @const {string}
 */
ContentExtensionBridge.PORT_NAME = 'ExtensionBridge.Port';

/**
 * The name of the message between the content script and background to
 * see if they're connected.
 * @const {string}
 */
ContentExtensionBridge.PING_MSG = 'ExtensionBridge.Ping';

/**
 * The name of the message between the background and content script to
 * confirm that they're connected.
 * @const {string}
 */
ContentExtensionBridge.PONG_MSG = 'ExtensionBridge.Pong';

/** @const {number} */
ContentExtensionBridge.MAX_PING_ATTEMPTS = 5;

/** @const {number} */
ContentExtensionBridge.TIME_BETWEEN_PINGS_MS = 500;

ContentExtensionBridge.init();
