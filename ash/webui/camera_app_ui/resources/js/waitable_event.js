// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A waitable event for synchronization between asynchronous jobs.
 * @template T
 */
export class WaitableEvent {
  /**
   * @public
   */
  constructor() {
    /**
     * @type {boolean}
     * @private
     */
    this.isSignaled_ = false;

    /**
     * @type {function(T): void}
     * @private
     */
    this.resolve_;

    /**
     * @type {!Promise<T>}
     * @private
     */
    this.promise_ = new Promise((resolve) => {
      this.resolve_ = resolve;
    });
  }

  /**
   * @return {boolean} Whether the event is signaled
   */
  isSignaled() {
    return this.isSignaled_;
  }

  /**
   * Signals the event.
   * @param {T=} value
   */
  signal(value) {
    if (this.isSignaled_) {
      return;
    }
    this.isSignaled_ = true;
    this.resolve_(value);
  }

  /**
   * @return {!Promise<T>} Resolved when the event is signaled.
   */
  wait() {
    return this.promise_;
  }

  /**
   * @param {number} timeout Timeout in ms.
   * @return {!Promise<T>} Resolved when the event is signaled, or rejected when
   *     timed out.
   */
  timedWait(timeout) {
    const timeoutPromise = new Promise((_resolve, reject) => {
      setTimeout(() => {
        reject(new Error(`Timed out after ${timeout}ms`));
      }, timeout);
    });
    return Promise.race([this.promise_, timeoutPromise]);
  }
}
