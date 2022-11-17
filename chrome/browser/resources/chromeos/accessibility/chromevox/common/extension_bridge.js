// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bridge to aid in communication between a Chrome
 * background page and content script.
 *
 * Use addMessageListener() to provide a message listener.
 * Messages can be any object that can be serialized using JSON.
 */

export class ExtensionBridge {
  /** @private */
  constructor() {
    /** @private {!Array<!function(Object, Port)>} */
    this.messageListeners_ = [];
    /** @private {number} */
    this.nextPongId_ = 1;
    /** @private {!Array<!Port>} */
    this.portCache_ = [];

    this.init_();
  }

  /**
   * Initialize the extension bridge in a background page context by registering
   * a listener for connections from the content script.
   * @private
   */
  init_() {
    chrome.extension.onConnect.addListener(
        port => this.onConnectHandler_(port));
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
   * Provide a function to listen to messages from all pages.
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
   * Listens for connections from the content scripts.
   * @param {!Port} port
   * @private
   */
  onConnectHandler_(port) {
    if (port.name !== ExtensionBridge.PORT_NAME) {
      return;
    }

    this.portCache_.push(port);

    port.onMessage.addListener(message => this.onMessage_(message, port));

    port.onDisconnect.addListener(() => this.onDisconnect_(port));
  }

  /**
   * Listens for messages to the background page from a specific port.
   * @param {Object} message
   * @param {!Port} port
   * @private
   */
  onMessage_(message, port) {
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
  onDisconnect_(port) {
    for (let i = 0; i < this.portCache_.length; i++) {
      if (this.portCache_[i] === port) {
        this.portCache_.splice(i, 1);
        break;
      }
    }
  }
}

/** @private {ExtensionBridge} */
ExtensionBridge.instance;

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
