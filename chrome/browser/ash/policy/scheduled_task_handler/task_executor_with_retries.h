// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TASK_EXECUTOR_WITH_RETRIES_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TASK_EXECUTOR_WITH_RETRIES_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace policy {

// This class runs a task that can fail. In case of failure it allows the caller
// to retry the task using a OneShotTimer i.e. the retry timer is not suspend
// aware. The caller must hold a wake lock if it wants the task to run till
// success or retry failure without the device suspending. Any callbacks passed
// to its API will not be invoked if an object of this class is destroyed.
class TaskExecutorWithRetries {
 public:
  using AsyncTask = base::OnceClosure;
  using RetryFailureCb = base::OnceCallback<void()>;

  // |description| - String identifying this object.
  // |get_ticks_since_boot_fn| - Callback that returns current ticks from boot.
  // Used for scheduling retry timer.
  // |max_retries| - Maximum number of retries after which trying the task is
  // given up.
  // |retry_time| - Time between each retry.
  TaskExecutorWithRetries(int max_retries, base::TimeDelta retry_time);

  TaskExecutorWithRetries(const TaskExecutorWithRetries&) = delete;
  TaskExecutorWithRetries& operator=(const TaskExecutorWithRetries&) = delete;

  ~TaskExecutorWithRetries();

  // Runs |task| and caches |retry_failure_cb| which will be called when
  // |max_retries_| is reached and |task| couldn't be run successfully.
  // Consecutive calls override any state and pending callbacks associated with
  // the previous call. |retry_failure_cb| will return the task that was last
  // scheduled using |ScheduleRetry|.
  void Start(AsyncTask task, RetryFailureCb retry_failure_cb);

  // Resets state and stops all pending callbacks.
  void Stop();

  // Cancels all outstanding |RetryTask| calls and schedules a new |RetryTask|
  // call on the calling sequence to run |task|.
  void ScheduleRetry(AsyncTask task);

 private:
  // Resets state including stopping all pending callbacks.
  void ResetState();

  // Maximum number of retries after which trying the task is given up.
  const int max_retries_;

  // Time between each retry.
  const base::TimeDelta retry_time_;

  // Current retry iteration. Capped at |max_retries_|.
  int num_retries_ = 0;

  // Callback to call after |max_retries_| have been reached and |task| wasn't
  // successfully scheduled.
  RetryFailureCb retry_failure_cb_;

  // Timer used to retry |task| passed in |ScheduleRetry|.
  base::OneShotTimer retry_timer_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TASK_EXECUTOR_WITH_RETRIES_H_
