// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_RETRYABLE_TASK_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_RETRYABLE_TASK_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace android {

// An interface for a task that can be retried.
class RetryableTask {
 public:
  virtual ~RetryableTask() = default;

  // Runs the task. When the task is finished, it should call
  // `should_retry_callback` with `false` if it should stop (e.g., on success)
  // and `true` if it should retry.
  virtual void Run(base::OnceCallback<void(bool)> should_retry_callback) = 0;

  virtual base::WeakPtr<RetryableTask> GetWeakPtr() = 0;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_RETRYABLE_TASK_H_
