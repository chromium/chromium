// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_POST_DELAYED_MEMORY_REDUCTION_TASK_H_
#define BASE_MEMORY_POST_DELAYED_MEMORY_REDUCTION_TASK_H_

#include "base/task/sequenced_task_runner.h"

namespace base {

// Context in which a memory reduction task is invoked.
enum class MemoryReductionTaskContext {
  // After the expiration of its delay.
  kDelayExpired,
  // Before the expiration of its delay, to proactively reduce memory.
  kProactive,
};

// This API should be used for posting delayed tasks that reduce memory usage
// while Chrome is backgrounded. On Android 14+, tasks posted this way may be
// run before the delay is elapsed, in the case where Chrome is about to be
// frozen by Android. On other platforms, this is equivalent to directly posting
// the delayed task, using the task runner.
void PostDelayedMemoryReductionTask(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceClosure task,
    base::TimeDelta delay);

// Same as above, but passes a parameter to the task, depending on how it was
// run. On non-Android platforms, will always pass |kDelayExpired|.
void PostDelayedMemoryReductionTask(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceCallback<void(MemoryReductionTaskContext)> task,
    base::TimeDelta delay);

}  // namespace base

#endif  // BASE_MEMORY_POST_DELAYED_MEMORY_REDUCTION_TASK_H_
