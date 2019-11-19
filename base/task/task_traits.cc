// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_traits.h"

#include <stddef.h>

#include <ostream>

#include "base/logging.h"

namespace base {

// The state of |FeatureList::IsEnabled(kNoPriorityInheritanceFromThreadPool)|
// as controlled by intenal::SetNoPriorityInheritanceFromThreadPool().
bool g_no_priority_inheritance_from_thread_pool = false;

void TaskTraits::InheritPriority(TaskPriority priority) {
  if (priority_set_explicitly() || g_no_priority_inheritance_from_thread_pool)
    return;

  priority_ = static_cast<uint8_t>(priority);
}

const char* TaskPriorityToString(TaskPriority task_priority) {
  switch (task_priority) {
    case TaskPriority::BEST_EFFORT:
      return "BEST_EFFORT";
    case TaskPriority::USER_VISIBLE:
      return "USER_VISIBLE";
    case TaskPriority::USER_BLOCKING:
      return "USER_BLOCKING";
  }
  NOTREACHED();
  return "";
}

const char* TaskShutdownBehaviorToString(
    TaskShutdownBehavior shutdown_behavior) {
  switch (shutdown_behavior) {
    case TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN:
      return "CONTINUE_ON_SHUTDOWN";
    case TaskShutdownBehavior::SKIP_ON_SHUTDOWN:
      return "SKIP_ON_SHUTDOWN";
    case TaskShutdownBehavior::BLOCK_SHUTDOWN:
      return "BLOCK_SHUTDOWN";
  }
  NOTREACHED();
  return "";
}

std::ostream& operator<<(std::ostream& os, const TaskPriority& task_priority) {
  os << TaskPriorityToString(task_priority);
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const TaskShutdownBehavior& shutdown_behavior) {
  os << TaskShutdownBehaviorToString(shutdown_behavior);
  return os;
}

namespace internal {
void SetNoPriorityInheritanceFromThreadPool() {
  g_no_priority_inheritance_from_thread_pool = true;
}
}  // namespace internal

}  // namespace base
