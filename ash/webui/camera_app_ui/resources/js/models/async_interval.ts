// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WaitableEvent} from '../waitable_event.js';

/**
 * Sleeps |delay| millisecond.
 *
 * @return Resolved after |delay| is passed.
 */
function sleep(delay: number): Promise<void> {
  return new Promise((resolve) => {
    setTimeout(resolve, delay);
  });
}

/**
 * A helper class for setAsyncInterval().
 */
class AsyncIntervalRunner {
  private readonly stopped = new WaitableEvent();

  private readonly runningPromise: Promise<void>;

  constructor(
      private readonly handler: () => Promise<void>,
      private readonly delay: number) {
    this.runningPromise = this.loop();
  }

  /**
   * Stops the loop and wait for the |handler| if it's running.
   */
  async stop(): Promise<void> {
    this.stopped.signal();
    await this.runningPromise;
  }

  /**
   * The main loop for running handler repeatedly.
   */
  private async loop(): Promise<void> {
    while (!this.stopped.isSignaled()) {
      // Wait until |delay| passed or the runner is stopped.
      await Promise.race([sleep(this.delay), this.stopped.wait()]);
      if (this.stopped.isSignaled()) {
        break;
      }
      await this.handler();
    }
  }
}


/**
 * A counter of runner, which is used as the identifier in setAsyncInterval().
 */
let runnerCount = 0;

/**
 * A map from the async interval id to the corresponding runner.
 */
const runnerMap = new Map<number, AsyncIntervalRunner>();

/**
 * Repeatedly calls the async function |handler| and waits until it's resolved,
 * with a fixed delay between the next call and the previous completion time.
 *
 * @return A numeric, non-zero value which identifies the timer.
 */
export function setAsyncInterval(
    handler: () => Promise<void>, delay: number): number {
  const runner = new AsyncIntervalRunner(handler, delay);
  const id = ++runnerCount;
  runnerMap.set(id, runner);
  return id;
}

/**
 * Cancels a timed, repeating async action by |id|, which was returned by the
 * corresponding call of setAsyncInterval().
 *
 * @return Resolved when the last action is finished.
 */
export async function clearAsyncInterval(id: number): Promise<void> {
  const runner = runnerMap.get(id);
  if (runner === undefined) {
    return;
  }
  await runner.stop();
}
