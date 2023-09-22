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

/**
 * Internal structure used in `AsyncJobQueue` for queued jobs.
 */
interface PendingJob {
  job: () => Awaitable<void>;
  resolve: () => void;
  reject: (error: unknown) => void;
}

/**
 * Returned by `AsyncJobQueue.push` containing `result` of the queued job.
 */
export interface AsyncJobInfo {
  /**
   * The result of the job. Resolved when the job is finished, cleared or
   * dropped, and rejected if the job is run but throws an error.
   * TODO(pihsun): returns different state for cleared or dropped case if
   * there's use for it.
   * TODO(pihsun): really returns the return value of the job from this if
   * there's use case for it. For now all places that use AsyncJobQueue and
   * need result use AsyncJobWithResultQueue, which doesn't support clearing and
   * mode other than 'enqueue', but has a simpler typing because the return
   * value always exist.
   */
  result: Promise<void>;
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
   * Most of the caller don't wait for the job to complete, relies on the queue
   * itself for async operation sequencing and relies on unhandled promise
   * rejection for error handling. So the job result is not directly returned
   * as a promise to avoid triggering @typescript-eslint/no-floating-promises.
   *
   * @return Return The job info containing the `result` of the job.
   */
  push(job: () => Awaitable<void>): AsyncJobInfo {
    if (this.runningPromise === null) {
      const result = Promise.resolve(job());
      this.runningPromise =
          result.catch(() => {/* ignore error from previous job */})
              .then(() => this.handlePendingJobs());
      return {result};
    }

    if (this.mode === 'drop') {
      return {result: Promise.resolve()};
    }
    if (this.mode === 'keepLatest') {
      this.clearInternal();
    }
    const result = new Promise<void>((resolve, reject) => {
      this.pendingJobs.push({job, resolve, reject});
    });
    return {result};
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
    callback: (...args: T) => Promise<void>): (...args: T) => AsyncJobInfo {
  const queue = new AsyncJobQueue(mode);
  return (...args) => queue.push(() => callback(...args));
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
