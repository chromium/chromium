// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_RETRY_STRATEGY_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_RETRY_STRATEGY_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace android {

class RetryableTask;

// A class to control the retry logic for a task.
class RetryStrategy {
 public:
  RetryStrategy(int max_retries, base::TimeDelta delay);
  ~RetryStrategy();

  RetryStrategy(const RetryStrategy&) = delete;
  RetryStrategy& operator=(const RetryStrategy&) = delete;

  // Starts the `task` with this strategy. `done_callback` is invoked once the
  // all retries are attempted or on early success/failure.
  void Start(RetryableTask* task, base::OnceCallback<void(bool)> done_callback);

 private:
  void RunTask();
  void OnTaskFinished(bool should_retry);

  const int max_retries_;
  const base::TimeDelta delay_;
  int retry_count_ = 0;
  // This is a WeakPtr to RetryableTask in case the task goes away before the
  // retry strategy does.
  base::WeakPtr<RetryableTask> task_ = nullptr;
  base::OnceCallback<void(bool)> done_callback_;

  // Used to ensure that the completion callback passed to the task is cancelled
  // if this strategy object is destroyed (e.g., if its owner is destroyed
  // while a task is in flight).
  base::WeakPtrFactory<RetryStrategy> weak_factory_{this};
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_RETRY_STRATEGY_H_
