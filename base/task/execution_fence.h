// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_EXECUTION_FENCE_H_
#define BASE_TASK_EXECUTION_FENCE_H_

#include "base/base_export.h"

namespace base {

// A ScopedThreadPoolExecutionFence prevents new tasks from being scheduled in
// the ThreadPool within its scope. Multiple fences can exist at the same time.
// Upon destruction of all ScopedThreadPoolExecutionFences, tasks that were
// preeempted are released. Note: the constructor of
// ScopedThreadPoolExecutionFence will not wait for currently running tasks (as
// they were posted before entering this scope and do not violate the contract;
// some of them could be CONTINUE_ON_SHUTDOWN and waiting for them to complete
// is ill-advised).
class BASE_EXPORT ScopedThreadPoolExecutionFence {
 public:
  ScopedThreadPoolExecutionFence();
  ScopedThreadPoolExecutionFence(const ScopedThreadPoolExecutionFence&) =
      delete;
  ScopedThreadPoolExecutionFence& operator=(
      const ScopedThreadPoolExecutionFence&) = delete;
  ~ScopedThreadPoolExecutionFence();
};

// ScopedBestEffortExecutionFence is similar to ScopedThreadPoolExecutionFence,
// but only prevents new tasks of BEST_EFFORT priority from being scheduled.
// See ScopedThreadPoolExecutionFence for the full semantics.
// TODO(crbug.com/441949788): By default, this only applies to tasks in the
// ThreadPool. Add a way to opt-in other threads.
class BASE_EXPORT ScopedBestEffortExecutionFence {
 public:
  ScopedBestEffortExecutionFence();
  ScopedBestEffortExecutionFence(const ScopedBestEffortExecutionFence&) =
      delete;
  ScopedBestEffortExecutionFence& operator=(
      const ScopedBestEffortExecutionFence&) = delete;
  ~ScopedBestEffortExecutionFence();
};

}  // namespace base

#endif  // BASE_TASK_EXECUTION_FENCE_H_
