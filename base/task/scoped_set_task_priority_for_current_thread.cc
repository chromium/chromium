// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/scoped_set_task_priority_for_current_thread.h"

#include "base/compiler_specific.h"

namespace base {
namespace internal {

namespace {

constinit thread_local TaskPriority task_priority_for_current_thread =
    TaskPriority::USER_BLOCKING;

}  // namespace

ScopedSetTaskPriorityForCurrentThread::ScopedSetTaskPriorityForCurrentThread(
    TaskPriority priority)
    : resetter_(&task_priority_for_current_thread,
                priority,
                TaskPriority::USER_BLOCKING) {}

ScopedSetTaskPriorityForCurrentThread::
    ~ScopedSetTaskPriorityForCurrentThread() = default;

TaskPriority GetTaskPriorityForCurrentThread() {
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&task_priority_for_current_thread, sizeof(TaskPriority));

  return task_priority_for_current_thread;
}

}  // namespace internal
}  // namespace base
