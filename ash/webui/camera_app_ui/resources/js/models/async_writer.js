// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../assert.js';
import {AsyncJobQueue} from '../async_job_queue.js';

/**
 * Represents a set of operations of a file-like writable stream. The seek and
 * close operations are optional.
 * @typedef {{
 *   write: function(!Blob): !Promise,
 *   seek: ((function(number): !Promise)|null),
 *   close: ((function(): !Promise)|null)
 * }}
 */
export let AsyncOps;

/**
 * Asynchronous writer.
 */
export class AsyncWriter {
  /**
   * @param {!AsyncOps} ops
   */
  constructor(ops) {
    /**
     * @type {!AsyncJobQueue}
     * @private
     */
    this.queue_ = new AsyncJobQueue();

    /**
     * @type {!AsyncOps}
     * @private
     */
    this.ops_ = ops;

    /**
     * @type {boolean}
     * @private
     */
    this.closed_ = false;
  }

  /**
   * Checks whether the writer supports seek operation.
   * @return {boolean}
   */
  seekable() {
    return this.ops_.seek !== null;
  }

  /**
   * Writes the blob asynchronously with |doWrite|.
   * @param {!Blob} blob
   * @return {!Promise} Resolved when the data is written.
   */
  async write(blob) {
    assert(!this.closed_);
    await this.queue_.push(() => this.ops_.write(blob));
  }

  /**
   * Seeks to the specified |offset|.
   * @param {number} offset
   * @return {!Promise} Resolved when the seek operation is finished.
   */
  async seek(offset) {
    assert(!this.closed_);
    assert(this.seekable());
    await this.queue_.push(() => this.ops_.seek(offset));
  }

  /**
   * Closes the writer. No more write operations are allowed.
   * @return {!Promise} Resolved when all write operations are finished.
   */
  async close() {
    this.closed_ = true;
    if (this.ops_.close !== null) {
      this.queue_.push(() => this.ops_.close());
    }
    await this.queue_.flush();
  }

  /**
   * Combines multiple writers into one writer such that the blob would be
   * written to each of them.
   * @param {...!AsyncWriter} writers
   * @return {!AsyncWriter} The combined writer.
   */
  static combine(...writers) {
    const write = (blob) => {
      return Promise.all(writers.map((writer) => writer.write(blob)));
    };

    const allSeekable = writers.every((writer) => writer.seekable());
    const seekAll = (offset) => {
      return Promise.all(writers.map((writer) => writer.seek(offset)));
    };
    const seek = allSeekable ? seekAll : null;

    const close = () => {
      return Promise.all(writers.map((writer) => writer.close()));
    };

    return new AsyncWriter({write, seek, close});
  }
}
