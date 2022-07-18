// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of functions and behaviors helpful for message
 * passing between renderers.
 */
goog.provide('BridgeHelper');

goog.require('BridgeAction');
goog.require('BridgeTarget');

/** @typedef {!Object<BridgeAction|string, Function>} */
let TargetHandlers;

/** @private {!Object<BridgeTarget|string, TargetHandlers>} */
BridgeHelper.handlers_ = {};

/**
 * This function should only be used by Bridges (e.g. BackgroundBridge,
 * PanelBridge) and not called directly by other classes.
 *
 * @param {BridgeTarget|string} target The name of the class that will handle
 *     this request.
 * @param {BridgeAction|string} action The name of the intended function or, if
 *     not a direct method of the class, a pseudo-function name.
 * @param {*=} value An optional single parameter to include with the message.
 *     If the method takes multiple parameters, they are passed as named members
 *     of an object literal.
 *
 * @return {!Promise} A promise, that resolves when the handler function has
 *     finished and contains any value returned by the handler.
 */
BridgeHelper.sendMessage = (target, action, value) => {
  return new Promise(
      resolve => chrome.runtime.sendMessage({target, action, value}, resolve));
};

/**
 * @param {BridgeTarget|string} target The name of the class that is registering
 *     the handler.
 * @param {BridgeAction|string} action The name of the intended function or, if
 *     not a direct method of the class, a pseudo-function name.
 * @param {Function} handler A function that performs the indicated action. It
 *     may optionally take a single parameter, and may have an optional return
 *         value that will be forwarded to the requestor.
 *     If the method takes multiple parameters, they are passed as named members
 *         of an object literal.
 */
BridgeHelper.registerHandler = (target, action, handler) => {
  if (!target || !action) {
    return;
  }
  if (!BridgeHelper.handlers_[target]) {
    BridgeHelper.handlers_[target] = {};
  }

  if (BridgeHelper.handlers_[target][action]) {
    throw 'Error: Re-assigning handlers for a specific target/action (' +
        target + '.' + action + ') is not permitted';
  }

  BridgeHelper.handlers_[target][action] = handler;
};

chrome.runtime.onMessage.addListener((message, sender, respond) => {
  const targetHandlers = BridgeHelper.handlers_[message.target];
  if (!targetHandlers || !targetHandlers[message.action]) {
    return;
  }

  const handler = targetHandlers[message.action];
  Promise.resolve(handler(message.value)).then(respond);
  return true; /** Wait for asynchronous response. */
});
