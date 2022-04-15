// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for non-background contexts (options,
 * panel, etc.) to communicate with the background.
 */

goog.provide('BackgroundBridge');

BackgroundBridge.BrailleBackground = {
  /** @return {!Promise<?LibLouis.Translator>} */
  async getDefaultTranslator() {
    return BackgroundBridge
        .sendMessage_('BrailleBackground', 'getDefaultTranslator')
        .then(BackgroundBridge.castTo(LibLouis.Translator));
  },

  /** @param {string} brailleTable The table for this translator to use. */
  async refreshBrailleTable(brailleTable) {
    return BackgroundBridge.sendMessage_(
        'BrailleBackground', 'refreshBrailleTable', brailleTable);
  },
};

// Helper functions:

/** @private {!Object<string, Object<string, Function>>} */
BackgroundBridge.handlers = {};

/**
 * @param {string} target The name of the class that is registering the handler.
 * @param {string} action The name of the intended function or, if not a direct
 *     method of the class, a pseudo-function name.
 * @param {Function} handler A function that performs the indicated action. It
 *     may optionally take a single parameter, and may have an optional return
 *         value that will be forwarded to the requestor.
 *     If the method takes multiple parameters, they are passed as named members
 *         of an object literal.
 */
BackgroundBridge.registerHandler = (target, action, handler) => {
  if (!BackgroundBridge.handlers[target]) {
    BackgroundBridge.handlers[target] = {};
  }
  BackgroundBridge.handlers[target][action] = handler;
};

BackgroundBridge.castTo = (type) => {
  return (obj) => {
    if (obj === null || obj === undefined) {
      return obj;
    }
    Object.setPrototypeOf(obj, type.prototype);
    return obj;
  };
};


/**
 * @param {string} target The name of the class that will handle this request.
 * @param {string} action The name of the intended function or, if not a direct
 *     method of the class, a pseudo-function name.
 * @param {*=} value An optional single parameter to include with the message.
 *     If the method takes multiple parameters, they are passed as named members
 *     of an object literal.
 *
 * @return {!Promise} A promise, that resolves when the handler function has
 *     finished and contains any value returned by the handler.
 * @private
 */
BackgroundBridge.sendMessage_ = (target, action, value) => {
  return new Promise(
      resolve => chrome.runtime.sendMessage({target, action, value}, resolve));
};

chrome.runtime.onMessage.addListener((message, sender, respond) => {
  const targetHandlers = BackgroundBridge.handlers[message.target];
  if (!targetHandlers || !targetHandlers[message.action]) {
    return;
  }

  const handler = targetHandlers[message.action];
  Promise.resolve(handler(message.value)).then(respond);
  return true; /** Wait for asynchronous response. */
});
