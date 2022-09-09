// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of functions and behaviors helpful for message
 * passing between renderers.
 */

import {BridgeAction, BridgeTarget} from './bridge_constants.js';

/** @typedef {!Object<BridgeAction|string, Function>}*/
let TargetHandlers;
export class BridgeHelper {
  /**
   * This function should only be used by Bridges (e.g. BackgroundBridge,
   * PanelBridge) and not called directly by other classes.
   *
   * @param {BridgeTarget|string} target The name of the class that will handle
   *     this request.
   * @param {BridgeAction|string} action The name of the intended function or,
   *     if not a direct method of the class, a pseudo-function name.
   *
   * Any arguments to be passed through can be appended to the function. They
   *     must be convertible to JSON objects for message passing - i.e., no
   *     functions or type information will be retained, and no direct
   *     modification of the original context is possible.
   *
   * @return {!Promise} A promise, that resolves when the handler function has
   *     finished and contains any value returned by the handler.
   */
  static sendMessage(target, action, ...args) {
    return new Promise(
        resolve => chrome.runtime.sendMessage({target, action, args}, resolve));
  }

  /**
   * @param {BridgeTarget|string} target The name of the class that is
   *     registering the handler.
   * @param {BridgeAction|string} action The name of the intended function or,
   *     if not a direct method of the class, a pseudo-function name.
   * @param {Function} handler A function that performs the indicated action. It
   *     may optionally take a single parameter, and may have an optional return
   *         value that will be forwarded to the requestor.
   *     If the method takes multiple parameters, they are passed as named
   * members of an object literal.
   */
  static registerHandler(target, action, handler) {
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
  }
}

/** @private {!Object<BridgeTarget|string, TargetHandlers>} */
BridgeHelper.handlers_ = {};

chrome.runtime.onMessage.addListener((message, sender, respond) => {
  const targetHandlers = BridgeHelper.handlers_[message.target];
  if (!targetHandlers || !targetHandlers[message.action]) {
    return;
  }

  const handler = targetHandlers[message.action];
  Promise.resolve(handler(...message.args)).then(respond);
  return true; /** Wait for asynchronous response. */
});
