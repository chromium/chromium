// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asynchronous job queue.
 */
export class AsyncJobQueue {
  /**
   * @public
   */
  constructor() {
    /**
     * @type {!Promise<unknown>}
     * @private
     */
    this.promise_ = Promise.resolve();

    /**
     * Flag for canceling all future jobs.
     * @private {boolean}
     */
    this.clearing_ = false;
  }

  /**
   * Pushes the given job into queue.
   * @template T
   * @param {function(): !Promise<T>} job
   * @return {!Promise<T|null>} Resolved when the job is finished.
   */
  push(job) {
    /** @type {!Promise<T|null>} */
    const promise = this.promise_.then(() => {
      if (this.clearing_) {
        return null;
      }
      return job();
    });
    this.promise_ = promise;
    return promise;
  }

  /**
   * Flushes the job queue.
   * @return {!Promise<void>} Resolved when all jobs in the queue are finished.
   */
  async flush() {
    await this.promise_;
  }

  /**
   * Clears all not-yet-scheduled jobs and waits for current job finished.
   */
  async clear() {
    this.clearing_ = true;
    await this.flush();
    this.clearing_ = false;
  }
}
