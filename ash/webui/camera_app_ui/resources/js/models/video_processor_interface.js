// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../assert.js';

/**
 * The interface for a video processor. All methods are marked as async since
 * it will be used with Comlink and Web Workers.
 * @interface
 */
export class VideoProcessor {
  /**
   * Writes a chunk data into the processor.
   * @param {!Blob} blob
   * @return {!Promise}
   * @abstract
   */
  async write(blob) {
    assertNotReached();
  }

  /**
   * Closes the processor. No more write operations are allowed.
   * @return {!Promise} Resolved when all the data are processed and flushed.
   * @abstract
   */
  async close() {
    assertNotReached();
  }

  /**
   * Cancels the remaining tasks. No more write operations are allowed.
   * @return {!Promise}
   * @abstract
   */
  async cancel() {
    assertNotReached();
  }
}
