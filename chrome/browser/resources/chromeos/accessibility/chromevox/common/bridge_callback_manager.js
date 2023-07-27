// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Manages callbacks across contexts by saving them and replacing
 *     them with BridgeCallbackIds.
 */

import {BridgeAction, BridgeContext, BridgeTarget} from './bridge_constants.js';
import {BridgeHelper} from './bridge_helper.js';

/** @type {!BridgeAction} */
const CALLBACK_ACTION = 'callback';

/**
 * @param {!BridgeContext} context
 * @return {!BridgeTarget}
 */
function getCallbackTargetForContext(context) {
  return /** @type {!BridgeTarget} */ ('callback_' + context);
}

export const BridgeCallbackManager = {
  /** @private {!Array<?function(*)>} */
  callbacks_: [],

  /** @private {boolean} */
  initialized_: false,

  /**
   * This function is used by BridgeCallbackId to save the callback.
   * All other classes should save callbacks by creating a new BridgeCallbackId
   * rather than calling this function.
   *
   * @param {function(*)} callback
   * @param {!BridgeContext} context The current context.
   * @return {number} The index of the given callback in the array.
   */
  addCallbackInternal(callback, context) {
    const index = BridgeCallbackManager.callbacks_.length;
    BridgeCallbackManager.callbacks_.push(callback);

    if (!BridgeCallbackManager.initialized_) {
      BridgeCallbackManager.startListening_(context);
    }

    return index;
  },

  /**
   * @param {!BridgeCallbackId} callbackId
   *
   * Any arguments to be passed to the callback can be appended to the function.
   * They will be converted to JSON in the message passing process, and so
   * functions cannot be passed, type information will be stripped (so methods
   * are no longer available), and source data cannot be directly modified.
   *
   * @return {!Promise}
   */
  performCallback(callbackId, ...args) {
    return BridgeHelper.sendMessage(
        getCallbackTargetForContext(callbackId.context), CALLBACK_ACTION,
        callbackId, args);
  },

  /**
   * @param {!BridgeContext} context
   * @private
   */
  startListening_(context) {
    BridgeHelper.registerHandler(
        getCallbackTargetForContext(context), CALLBACK_ACTION,
        (callbackId, args) => {
          // Replace the callback with null to maintain the other indices.
          const callback =
              BridgeCallbackManager.callbacks_.splice(callbackId.index, 1, null)
                  .pop();  // splice() returns an array of the removed items.
          if (typeof callback === 'function') {
            callback(...args);
          }

          // If there are no callbacks remaining, reset the array.
          if (!BridgeCallbackManager.callbacks_.some(c => Boolean(c))) {
            BridgeCallbackManager.callbacks_ = [];
          }
        });
    BridgeCallbackManager.initialized_ = true;
  },
};

export class BridgeCallbackId {
  /**
   * @param {!BridgeContext} context
   * @param {function(*)} callback
   */
  constructor(context, callback) {
    /** @public {!BridgeContext} */
    this.context = context;

    /** @public {number} */
    this.index = BridgeCallbackManager.addCallbackInternal(callback, context);
  }
}
