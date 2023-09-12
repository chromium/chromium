// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Awaitable} from './type.js';

/**
 * Different modes of the AsyncJobQueue.
 *
 * * `enqueue` queues every jobs. Useful when we just want to ensure no two job
 *   runs at the same time, but all jobs need to be run eventually.
 * * `drop` don't queue new job when the current one is still running. Useful
 *   for things like user input.
 * * `keepLatest` only keep the newest job in the pending queue. Useful for
 *   things like update event handler, that if multiple new update events
 *   arrive while we're handing the current one, we only want to run the
 *   handler once more when the current handler is done.
 */
type AsyncJobQueueMode = 'drop'|'enqueue'|'keepLatest';

interface PendingJob {
  job: () => Awaitable<void>;
  resolve: () => void;
  reject: (error: unknown) => void;
}

/**
 * Asynchronous job queue that supports different queuing behavior, and
 * clearing all pending jobs.
 */
export class AsyncJobQueue {
  /**
   * The current "running" promise.
   *
   * This is null if no current job is running, and is set by the first job
   * pushed while the queue is not running. This is resolved and set to null
   * only when all the queued jobs are processed.
   */
  private runningPromise: Promise<void>|null = null;

  /**
   * The pending jobs.
   *
   * The first call to `push` when the queue is idle won't be included in this
   * array, but all other calls to `push` will add a job to this array.
   *
   * Depending on `mode`, this array will have a max size of 0 ("drop" mode), 1
   * ("keepLatest" mode), or infinity ("enqueue" mode).
   */
  private pendingJobs: PendingJob[] = [];

  /**
   * Constructs a new `AsyncJobQueue`.
   *
   * See the documentation of `AsyncJobQueueMode` for possible modes.
   */
  constructor(private readonly mode: AsyncJobQueueMode = 'enqueue') {}

  /**
   * Handles all job in `queuedJobs`.
   *
   * This should only be called in the promise chain of `runningPromise`.
   */
  private async handlePendingJobs(): Promise<void> {
    while (true) {
      const pendingJob = this.pendingJobs.shift();
      if (pendingJob === undefined) {
        break;
      }
      try {
        await pendingJob.job();
        pendingJob.resolve();
      } catch (e) {
        pendingJob.reject(e);
      }
    }
    this.runningPromise = null;
  }

  /**
   * Pushes the given job into queue.
   *
   * @return Resolved when the job is finished, cleared or dropped.
   */
  push(job: () => Awaitable<void>): Promise<void> {
    if (this.runningPromise === null) {
      const result = Promise.resolve(job());
      this.runningPromise =
          result.catch(() => {/* ignore error from previous job */})
              .then(() => this.handlePendingJobs());
      return result;
    }

    if (this.mode === 'drop') {
      return Promise.resolve();
    }
    if (this.mode === 'keepLatest') {
      this.clearInternal();
    }
    return new Promise((resolve, reject) => {
      this.pendingJobs.push({job, resolve, reject});
    });
  }

  /**
   * Flushes the job queue.
   *
   * @return Resolved when all jobs in the queue are finished.
   */
  flush(): Promise<void> {
    if (this.runningPromise === null) {
      return Promise.resolve();
    }
    return this.runningPromise;
  }

  private clearInternal(): void {
    for (const job of this.pendingJobs) {
      job.resolve();
    }
    this.pendingJobs = [];
  }

  /**
   * Clears all not-yet-scheduled jobs and waits for current job finished.
   */
  clear(): Promise<void> {
    this.clearInternal();
    return this.flush();
  }
}

/**
 * Transform an asynchronous callback to a synchronous one.
 *
 * The callback will be queued in a single AsyncJobQueue with the given `mode`.
 */
export function queuedAsyncCallback<T extends unknown[]>(
    mode: AsyncJobQueueMode,
    callback: (...args: T) => Promise<void>): (...args: T) => void {
  const queue = new AsyncJobQueue(mode);
  return (...args) => {
    // The callback are queued inside the async queue and not awaited.
    // Error will be logged by unhandled promise rejection.
    void queue.push(() => callback(...args));
  };
}

/**
 * Asynchronous job queue that returns the value of job result. Use this only
 * when job needs return value.
 */
export class AsyncJobWithResultQueue {
  private promise: Promise<unknown> = Promise.resolve();

  /**
   * Pushes the given job into queue.
   *
   * @return Resolved with the job return value when the job is finished.
   */
  push<T>(job: () => Awaitable<T>): Promise<T> {
    const promise =
        this.promise.catch(() => {/* ignore error from previous job */})
            .then(job);
    this.promise = promise;
    return promise;
  }

  /**
   * Flushes the job queue.
   *
   * @return Resolved when all jobs in the queue are finished.
   */
  async flush(): Promise<void> {
    await this.promise;
  }
}
