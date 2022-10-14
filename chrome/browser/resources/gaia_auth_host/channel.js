// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const INTERNAL_REQUEST_MESSAGE = 'internal-request-message';

const INTERNAL_REPLY_MESSAGE = 'internal-reply-message';

/**
 * Channel to the background script.
 */
export class Channel {
  constructor() {
    // Message port to use to communicate with background script.
    this.port_ = null;

    // Registered message callbacks.
    this.messageCallbacks_ = {};

    // Internal request id to track pending requests.
    this.nextInternalRequestId_ = 0;

    // Pending internal request callbacks.
    this.internalRequestCallbacks_ = {};
  }

  /**
   * Initialize the channel with given port for the background script.
   */
  init(port) {
    this.port_ = port;
    this.port_.onMessage.addListener(this.onMessage_.bind(this));
  }

  /**
   * Connects to the background script with the given name.
   */
  connect(name) {
    this.port_ = chrome.runtime.connect({name: name});
    this.port_.onMessage.addListener(this.onMessage_.bind(this));
  }

  /**
   * Associates a message name with a callback. When a message with the name
   * is received, the callback will be invoked with the message as its arg.
   * Note only the last registered callback will be invoked.
   */
  registerMessage(name, callback) {
    this.messageCallbacks_[name] = callback;
  }

  /**
   * Sends a message to the other side of the channel.
   */
  send(msg) {
    this.port_.postMessage(msg);
  }

  /**
   * Sends a message to the other side and invokes the callback with
   * the replied object. Useful for message that expects a returned result.
   */
  sendWithCallback(msg, callback) {
    const requestId = this.nextInternalRequestId_++;
    this.internalRequestCallbacks_[requestId] = callback;
    this.send({
      name: INTERNAL_REQUEST_MESSAGE,
      requestId: requestId,
      payload: msg,
    });
  }

  /**
   * Invokes message callback using given message.
   * @return {*} The return value of the message callback or null.
   */
  invokeMessageCallbacks_(msg) {
    const name = msg.name;
    if (this.messageCallbacks_[name]) {
      return this.messageCallbacks_[name](msg);
    }

    console.error('Error: Unexpected message, name=' + name);
    return null;
  }

  /**
   * Invoked when a message is received.
   */
  onMessage_(msg) {
    const name = msg.name;
    if (name === INTERNAL_REQUEST_MESSAGE) {
      const payload = msg.payload;
      const result = this.invokeMessageCallbacks_(payload);
      this.send({
        name: INTERNAL_REPLY_MESSAGE,
        requestId: msg.requestId,
        result: result,
      });
    } else if (name === INTERNAL_REPLY_MESSAGE) {
      const callback = this.internalRequestCallbacks_[msg.requestId];
      delete this.internalRequestCallbacks_[msg.requestId];
      if (callback) {
        callback(msg.result);
      }
    } else {
      this.invokeMessageCallbacks_(msg);
    }
  }
}
