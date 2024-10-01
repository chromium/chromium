// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

type Awaitable<T> = Promise<T>|T;

/**
 * Internal structure used in `AsyncJobQueue` for queued jobs.
 */
interface PendingJob {
  job: () => Promise<void>| void;
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
      // We always run the job asynchronously.
      const result = Promise.resolve().then(() => job());
      this.runningPromise = result
                              .catch(
                                () => {
                                  /* ignore error from previous job */
                                },
                              )
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

  /**
   * Returns whether there's any pending job in the queue.
   */
  hasPendingJob(): boolean {
    return this.pendingJobs.length > 0;
  }
}
