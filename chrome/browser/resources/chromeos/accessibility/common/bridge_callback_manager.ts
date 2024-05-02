// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Manages callbacks across contexts by saving them and replacing
 *     them with BridgeCallbackIds.
 */

import {ActionType, BridgeHelper, TargetType} from './bridge_helper.js';

type MaybeFunction = Function | null;

/** Contexts should be constants, defined in a central place. */
export type ContextType = string;

export class BridgeCallbackManager {
  private static callbacks_: MaybeFunction[] = [];
  private static initialized_ = false;

  /**
   * This function is used by BridgeCallbackId to save the callback.
   * All other classes should save callbacks by creating a new BridgeCallbackId
   * rather than calling this function.
   *
   * @param context The current context.
   * @return The index of the given callback in the array.
   */
  static addCallbackInternal(callback: Function, context: ContextType): number {
    const index = BridgeCallbackManager.callbacks_.length;
    BridgeCallbackManager.callbacks_.push(callback);

    if (!BridgeCallbackManager.initialized_) {
      BridgeCallbackManager.startListening_(context);
    }

    return index;
  }

  /**
   * Any arguments to be passed to the callback can be appended to the function.
   * They will be converted to JSON in the message passing process, and so
   * functions cannot be passed, type information will be stripped (so methods
   * are no longer available), and source data cannot be directly modified.
   */
  static performCallback(
      callbackId: BridgeCallbackId, ...args: any[]): Promise<any> {
    return BridgeHelper.sendMessage(
        getCallbackTargetForContext(callbackId.context), CALLBACK_ACTION,
        callbackId, args);
  }

  private static startListening_(context: ContextType): void {
    BridgeHelper.registerHandler(
        getCallbackTargetForContext(context), CALLBACK_ACTION,
        (callbackId: BridgeCallbackId, args: any[]) => {
          // Replace the callback with null to maintain the other indices.
          const callback =
              BridgeCallbackManager.callbacks_.splice(callbackId.index, 1, null)
                  .pop();  // splice() returns an array of the removed items.
          if (typeof callback === 'function') {
            callback(...args);
          }

          // If there are no callbacks remaining, reset the array.
          if (!BridgeCallbackManager.callbacks_.some(
              (callback: MaybeFunction) => Boolean(callback))) {
            BridgeCallbackManager.callbacks_ = [];
          }
        });
    BridgeCallbackManager.initialized_ = true;
  }
}

export class BridgeCallbackId {
  context: ContextType;
  index: number;

  constructor(context: ContextType, callback: Function) {
    this.context = context;
    this.index = BridgeCallbackManager.addCallbackInternal(callback, context);
  }
}

// Local to module.

const CALLBACK_ACTION: ActionType = 'callback';

function getCallbackTargetForContext(context: ContextType): TargetType {
  return ('callback_' + context) as TargetType;
}
