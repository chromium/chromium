// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../assert.js';
// eslint-disable-next-line no-unused-vars
import {AsyncWriter} from './async_writer.js';
// eslint-disable-next-line no-unused-vars
import {VideoProcessorArgs} from './ffmpeg/video_processor_args.js';

/**
 * The interface for a video processor. All methods are marked as async since
 * it will be used with Comlink and Web Workers.
 * @interface
 */
export class VideoProcessor {
  /**
   * @param {!AsyncWriter} output The output writer.
   * @param {!VideoProcessorArgs} processorArgs
   */
  constructor(output, processorArgs) {
    assertNotReached();
  }

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
