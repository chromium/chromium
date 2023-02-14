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
 * AsyncIntervalRunner handles calling an async function repeatedly with a
 * fixed delay between calls.
 */
export class AsyncIntervalRunner {
  private readonly stopped = new WaitableEvent();

  /**
   * Repeatedly calls the async function |handler| and waits until it's
   * resolved, with a fixed delay between the next call and the previous
   * completion time.
   *
   * @param handler Handler to be called, a |stopped| WaitableEvent is passed in
   *     and the handler should act as it's cancelled after |stopped| is
   *     signaled.
   * @param delayMs Delay between calls to |handler| in milliseconds.
   */
  constructor(
      private readonly handler: (stopped: WaitableEvent) => Promise<void>,
      private readonly delayMs: number) {
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
      await Promise.race([sleep(this.delayMs), this.stopped.wait()]);
      if (this.stopped.isSignaled()) {
        break;
      }
      await this.handler(this.stopped);
    }
  }
}
