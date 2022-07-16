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
     * @private {!Promise}
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
   * @param {function(): !Promise} job
   * @return {!Promise} Resolved when the job is finished.
   */
  push(job) {
    this.promise_ = this.promise_.then(() => {
      if (this.clearing_) {
        return;
      }
      return job();
    });
    return this.promise_;
  }

  /**
   * Flushes the job queue.
   * @return {!Promise} Resolved when all jobs in the queue are finished.
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
