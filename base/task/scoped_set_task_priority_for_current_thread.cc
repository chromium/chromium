// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/scoped_set_task_priority_for_current_thread.h"

#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {
namespace internal {

namespace {

ABSL_CONST_INIT thread_local TaskPriority task_priority_for_current_thread =
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
  return task_priority_for_current_thread;
}

}  // namespace internal
}  // namespace base
