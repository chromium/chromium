// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/retry_strategy.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/android/compositor/retryable_task.h"

namespace android {

RetryStrategy::RetryStrategy(int max_retries, base::TimeDelta delay)
    : max_retries_(max_retries), delay_(delay) {}

RetryStrategy::~RetryStrategy() = default;

void RetryStrategy::Start(RetryableTask* task,
                          base::OnceCallback<void(bool)> done_callback) {
  CHECK(!done_callback_);
  task_ = task->GetWeakPtr();
  done_callback_ = std::move(done_callback);
  RunTask();
}

void RetryStrategy::RunTask() {
  if (!task_) {
    if (done_callback_) {
      std::move(done_callback_).Run(false);
    }
    return;
  }
  task_->Run(base::BindOnce(&RetryStrategy::OnTaskFinished,
                            weak_factory_.GetWeakPtr()));
}

void RetryStrategy::OnTaskFinished(bool should_retry) {
  if (!should_retry || retry_count_ >= max_retries_) {
    if (done_callback_) {
      std::move(done_callback_).Run(!should_retry);
    }
    return;
  }

  retry_count_++;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RetryStrategy::RunTask, weak_factory_.GetWeakPtr()),
      delay_);
}

}  // namespace android
