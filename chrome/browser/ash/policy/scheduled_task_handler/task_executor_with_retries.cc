// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/task_executor_with_retries.h"

#include <utility>

#include "base/logging.h"

namespace policy {

TaskExecutorWithRetries::TaskExecutorWithRetries(int max_retries,
                                                 base::TimeDelta retry_time)
    : max_retries_(max_retries), retry_time_(retry_time) {}

TaskExecutorWithRetries::~TaskExecutorWithRetries() = default;

void TaskExecutorWithRetries::Start(AsyncTask task,
                                    RetryFailureCb retry_failure_cb) {
  ResetState();
  retry_failure_cb_ = std::move(retry_failure_cb);
  std::move(task).Run();
}

void TaskExecutorWithRetries::Stop() {
  ResetState();
}

void TaskExecutorWithRetries::ScheduleRetry(AsyncTask task) {
  // Retrying has a limit. In the unlikely scenario this is met, reset all
  // state. Now an update check can only happen when a new policy comes in or
  // Chrome is restarted.
  if (num_retries_ >= max_retries_) {
    LOG(ERROR) << "Aborting task after max retries";
    std::move(retry_failure_cb_).Run();
    ResetState();
    return;
  }

  ++num_retries_;
  retry_timer_.Start(FROM_HERE, retry_time_, std::move(task));
}

void TaskExecutorWithRetries::ResetState() {
  num_retries_ = 0;
  retry_failure_cb_.Reset();
  retry_timer_.Stop();
}

}  // namespace policy
