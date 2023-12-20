// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @externs
 * @fileoverview Temporary JS externs file for closure compilation of files
 * that use `message_pipe.ts`. Refer to message_pipe.ts for documentation.
 */

/**
 * @typedef {{
 *     name: string,
 *     message: string,
 *     stack: string,
 * }}
 */
let GenericErrorResponse;

/**
 * @typedef {function(!Object): (!Object|undefined|!Promise<!Object|undefined>)}
 */
let MessageHandler;

/**
 * @template T
 * @param {?T|undefined} condition
 * @return {T} A non-null |condition|.
 * @closurePrimitive {asserts.truthy}
 * @suppress {reportUnknownTypes} because T is not sufficiently constrained.
 */
function assertCast(condition) {}

class MessagePipe {
  /**
   * @param {string} targetOrigin
   * @param {!Window=} target If not specified, the document tree will be
   *     queried for a iframe with src `targetOrigin` to target.
   * @param {boolean=} rethrowErrors
   */
  constructor(targetOrigin, target, rethrowErrors = true) {
    /**
     * @type {boolean}
     */
    this.rethrowErrors = rethrowErrors;
  }

  /**
   * @param {string} messageType
   * @param {!MessageHandler} handler
   */
  registerHandler(messageType, handler) {}

  /**
   * @param {string} messageType
   * @param {!Object=} message
   * @return {!Promise<!Object>}
   */
  async sendMessage(messageType, message = {}) {}

  detach() {}
}
