// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/post_delayed_memory_reduction_task.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/pre_freeze_background_memory_trimmer.h"
#endif

namespace base {

void PostDelayedMemoryReductionTask(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceClosure task,
    base::TimeDelta delay) {
#if BUILDFLAG(IS_ANDROID)
  android::PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      std::move(task_runner), from_here, std::move(task), delay);
#else
  task_runner->PostDelayedTask(from_here, std::move(task), delay);
#endif
}

void PostDelayedMemoryReductionTask(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceCallback<void(MemoryReductionTaskContext)> task,
    base::TimeDelta delay) {
#if BUILDFLAG(IS_ANDROID)
  android::PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      std::move(task_runner), from_here, std::move(task), delay);
#else
  task_runner->PostDelayedTask(
      from_here,
      BindOnce(std::move(task), MemoryReductionTaskContext::kDelayExpired),
      delay);
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace base
