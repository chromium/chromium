// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_traits.h"

#include <stddef.h>

#include <ostream>

#include "base/notreached.h"

namespace base {

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

ThreadType TaskPriorityToThreadType(TaskPriority priority) {
  switch (priority) {
    case TaskPriority::BEST_EFFORT:
      return ThreadType::kBackground;
    case TaskPriority::USER_VISIBLE:
      return ThreadType::kUtility;
    case TaskPriority::USER_BLOCKING:
      return ThreadType::kDefault;
  }
}

ThreadType EffectiveThreadType(const TaskTraits& traits,
                               ThreadType originating_thread_type) {
  if (traits.inherit_thread_type()) {
    return std::min(traits.max_thread_type(), originating_thread_type);
  }
  return TaskPriorityToThreadType(traits.priority());
}

}  // namespace internal
}  // namespace base
