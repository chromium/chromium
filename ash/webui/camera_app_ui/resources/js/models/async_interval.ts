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

  constructor(
      private readonly handler: (stopped: WaitableEvent) => Promise<void>,
      private readonly delay: number) {
    void this.loop();
  }

  /**
   * Stops the loop.
   */
  stop(): void {
    this.stopped.signal();
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
      await this.handler(this.stopped);
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
 * @param handler Handler to be called, a |stopped| WaitableEvent is passed in
 *     and the handler should act as it's cancelled after |stopped| is signaled.
 * @param delayMs Delay between calls to |handler| in milliseconds.
 * @return A numeric, non-zero value which identifies the timer.
 */
export function setAsyncInterval(
    handler: (stopped: WaitableEvent) => Promise<void>,
    delayMs: number): number {
  const runner = new AsyncIntervalRunner(handler, delayMs);
  const id = ++runnerCount;
  runnerMap.set(id, runner);
  return id;
}

/**
 * Cancels a timed, repeating async action by |id|, which was returned by the
 * corresponding call of setAsyncInterval().
 */
export function clearAsyncInterval(id: number): void {
  const runner = runnerMap.get(id);
  if (runner === undefined) {
    return;
  }
  runnerMap.delete(id);
  runner.stop();
}
