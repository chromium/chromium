// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bridge to aid in communication between a Chrome
 * background page and content script.
 *
 * It automatically figures out where it's being run and initializes itself
 * appropriately. Then just call send() to send a message from the background
 * to the page, and addMessageListener() to provide a message listener.
 * Messages can be any object that can be serialized using JSON.
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
   * Send a message from the background page to the content script of the
   * current selected tab.
   *
   * @param {Object} message The message to send.
   */
  static send(message) {
    ExtensionBridge.instance.portCache_.forEach(
        port => port.postMessage(message));
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
