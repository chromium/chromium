// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bridge to aid in communication between a Chrome
 * background page and content script.
 *
 * It automatically figures out where it's being run and initializes itself
 * appropriately. Then just call send() to send a message from the background
 * to the page or vice versa, and addMessageListener() to provide a message
 * listener.  Messages can be any object that can be serialized using JSON.
 *
 */

export class ExtensionBridge {
  /** @private */
  constructor() {
    /** @private {!Array<!function(Object, Port)>} */
    this.messageListeners_ = [];
    /** @private {!Array<!function()>} */
    this.disconnectListeners_ = [];
    /** @private {number} */
    this.id_ = -1;

    // Used in the background context.
    /** @private {!Array<!Port>} */
    this.portCache_ = [];
    /** @private {number} */
    this.nextPongId_ = 1;

    // Used in the content script context.
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

  /**
   * Initialize the extension bridge. Dynamically figure out whether we're in
   * the background page, content script, or in a page, and call the
   * corresponding function for more specific initialization.
   */
  static init() {
    ExtensionBridge.instance = new ExtensionBridge();
  }

  /**
   * Send a message. If the context is a page, sends a message to the
   * extension background page. If the context is a background page, sends
   * a message to the current active tab (not all tabs).
   *
   * @param {Object} message The message to be sent.
   */
  static send(message) {
    ExtensionBridge.instance.sendBackgroundToContentScript_(message);
  }

  /**
   * Provide a function to listen to messages. In page context, this
   * listens to messages from the background. In background context,
   * this listens to messages from all pages.
   *
   * The function gets called with two parameters: the message, and a
   * port that can be used to send replies.
   *
   * @param {function(Object, Port)} listener The message listener.
   */
  static addMessageListener(listener) {
    ExtensionBridge.instance.messageListeners_.push(listener);
  }

  /**
   * Provide a function to be called when the connection is
   * disconnected.
   * @param {function()} listener The listener.
   */
  static addDisconnectListener(listener) {
    ExtensionBridge.instance.disconnectListeners_.push(listener);
  }

  /** Removes all message listeners from the extension bridge. */
  static removeMessageListeners() {
    ExtensionBridge.instance.messageListeners_ = [];
  }

  /**
   * Returns a unique id for this instance of the script.
   * @return {number}
   */
  static uniqueId() {
    return ExtensionBridge.instance.id_;
  }

  /**
   * Initialize the extension bridge in a background page context by registering
   * a listener for connections from the content script.
   * @private
   */
  init_() {
    this.id_ = 0;
    chrome.extension.onConnect.addListener(
        port => this.backgroundOnConnectHandler_(port));
  }

  /**
   * Listens for connections from the content scripts.
   * @param {!Port} port
   * @private
   */
  backgroundOnConnectHandler_(port) {
    if (port.name !== ExtensionBridge.PORT_NAME) {
      return;
    }

    this.portCache_.push(port);

    port.onMessage.addListener(
        message => this.backgroundOnMessage_(message, port));

    port.onDisconnect.addListener(() => this.backgroundOnDisconnect_(port));
  }

  /**
   * Listens for messages to the background page from a specific port.
   * @param {Object} message
   * @param {!Port} port
   * @private
   */
  backgroundOnMessage_(message, port) {
    if (message[ExtensionBridge.PING_MSG]) {
      const pongMessage = {[ExtensionBridge.PONG_MSG]: this.nextPongId_++};
      port.postMessage(pongMessage);
      return;
    }

    this.messageListeners_.forEach(listener => listener(message, port));
  }

  /**
   * Handles a specific port disconnecting.
   * @param {!Port} port
   * @private
   */
  backgroundOnDisconnect_(port) {
    for (let i = 0; i < this.portCache_.length; i++) {
      if (this.portCache_[i] === port) {
        this.portCache_.splice(i, 1);
        break;
      }
    }
  }

  /**
   * @param {Object} request
   * @param {*} sender
   * @param {function(*)} sendResponse
   * @private
   */
  contentOnMessageHandler_(request, sender, sendResponse) {
    if (request[ExtensionBridge.PONG_MSG]) {
      this.gotPongFromBackgroundPage_(request[ExtensionBridge.PONG_MSG]);
    } else {
      this.messageListeners_.forEach(
          listener => listener(request, this.backgroundPort_));
    }
    sendResponse({});
  }

  /**
   * Set up the connection to the background page.
   * @private
   */
  setupBackgroundPort_() {
    this.backgroundPort_ =
        chrome.extension.connect({name: ExtensionBridge.PORT_NAME});
    if (!this.backgroundPort_) {
      return;
    }
    this.backgroundPort_.onMessage.addListener(
        message => this.contentOnPortMessage_(message));
    this.backgroundPort_.onDisconnect.addListener(
        () => this.contentOnDisconnect_());
  }

  /**
   * @param {Object} message
   * @private
   */
  contentOnPortMessage_(message) {
    if (message[ExtensionBridge.PONG_MSG]) {
      this.gotPongFromBackgroundPage_(message[ExtensionBridge.PONG_MSG]);
    } else {
      this.messageListeners_.forEach(
          listener => listener(message, this.backgroundPort_));
    }
  }

  /** @private */
  contentOnDisconnect_() {
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
    if (this.pingAttempts_ > ExtensionBridge.MAX_PING_ATTEMPTS) {
      // Could not connect after several ping attempts. Call the disconnect
      // handlers, which will disable ChromeVox.
      this.disconnectListeners_.forEach(listener => listener());
      return;
    }

    // Send the ping.
    const msg = {
      [ExtensionBridge.PING_MSG]: 1,
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
        ExtensionBridge.TIME_BETWEEN_PINGS_MS);
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
      this.sendContentScriptToBackground_(this.queuedMessages_.shift());
    }
  }

  /**
   * Send a message from the content script to the background page.
   * @param {Object} message The message to send.
   * @private
   */
  sendContentScriptToBackground_(message) {
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

  /**
   * Send a message from the background page to the content script of the
   * current selected tab.
   *
   * @param {Object} message The message to send.
   * @private
   */
  sendBackgroundToContentScript_(message) {
    this.portCache_.forEach(port => port.postMessage(message));
  }
}

// Keep these constants in sync with injected/extension_bridge.js.

/**
 * The name of the port between the content script and background page.
 * @const {string}
 */
ExtensionBridge.PORT_NAME = 'ExtensionBridge.Port';

/**
 * The name of the message between the content script and background to
 * see if they're connected.
 * @const {string}
 */
ExtensionBridge.PING_MSG = 'ExtensionBridge.Ping';

/**
 * The name of the message between the background and content script to
 * confirm that they're connected.
 * @const {string}
 */
ExtensionBridge.PONG_MSG = 'ExtensionBridge.Pong';

/** @const {number} */
ExtensionBridge.MAX_PING_ATTEMPTS = 5;

/** @const {number} */
ExtensionBridge.TIME_BETWEEN_PINGS_MS = 500;

ExtensionBridge.init();
