// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WaitableEvent} from '../waitable_event.js';

/**
 * Sleeps |delay| millisecond.
 * @param {number} delay
 * @return {!Promise} Resolved after |delay| is passed.
 */
function sleep(delay) {
  return new Promise((resolve) => {
    setTimeout(resolve, delay);
  });
}

/**
 * A helper class for setAsyncInterval().
 */
class AsyncIntervalRunner {
  /**
   * @param {function(): !Promise} handler
   * @param {number} delay
   */
  constructor(handler, delay) {
    /**
     * @type {function(): !Promise}
     * @private
     */
    this.handler_ = handler;

    /**
     * @type {number}
     * @private
     */
    this.delay_ = delay;

    /**
     * @type {!WaitableEvent}
     * @private
     */
    this.stopped_ = new WaitableEvent();

    /**
     * @type {!Promise}
     * @private
     */
    this.runningPromise_ = this.loop_();
  }


  /**
   * Stops the loop and wait for the |handler| if it's running.
   * @return {!Promise}
   */
  async stop() {
    this.stopped_.signal();
    await this.runningPromise_;
  }

  /**
   * The main loop for running handler repeatedly.
   * @return {!Promise}
   */
  async loop_() {
    while (!this.stopped_.isSignaled()) {
      // Wait until |delay| passed or the runner is stopped.
      await Promise.race([sleep(this.delay_), this.stopped_.wait()]);
      if (this.stopped_.isSignaled()) {
        break;
      }
      await this.handler_();
    }
  }
}


/**
 * A counter of runner, which is used as the identifier in setAsyncInterval().
 * @type {number}
 */
let runnerCount = 0;

/**
 * A map from the async interval id to the corresponding runner.
 * @type {!Map<number, !AsyncIntervalRunner>}
 */
const runnerMap = new Map();

/**
 * Repeatedly calls the async function |handler| and waits until it's resolved,
 * with a fixed delay between the next call and the previous completion time.
 * @param {function(): !Promise} handler
 * @param {number} delay
 * @return {number} A numeric, non-zero value which identifies the timer.
 */
export function setAsyncInterval(handler, delay) {
  const runner = new AsyncIntervalRunner(handler, delay);
  const id = ++runnerCount;
  runnerMap.set(id, runner);
  return id;
}

/**
 * Cancels a timed, repeating async action by |id|, which was returned by the
 * corresponding call of setAsyncInterval().
 * @param {number} id
 * @return {!Promise} Resolved when the last action is finished.
 */
export async function clearAsyncInterval(id) {
  const runner = runnerMap.get(id);
  if (runner === undefined) {
    return;
  }
  await runner.stop();
}
