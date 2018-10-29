// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/tasks.h"

namespace base {
namespace sequence_manager {

Task::Task(internal::PostedTask posted_task,
           TimeTicks desired_run_time,
           internal::EnqueueOrder sequence_order,
           internal::EnqueueOrder enqueue_order,
           internal::WakeUpResolution resolution)
    : PendingTask(posted_task.location,
                  std::move(posted_task.callback),
                  desired_run_time,
                  posted_task.nestable),
      task_type(posted_task.task_type),
      enqueue_order_(enqueue_order) {
  // We use |sequence_num| in DelayedWakeUp for ordering purposes and it
  // may wrap around to a negative number during the static cast, hence,
  // the relevant code is especially sensitive to a potential change of
  // |PendingTask::sequence_num|'s type.
  static_assert(std::is_same<decltype(sequence_num), int>::value, "");
  sequence_num = static_cast<int>(sequence_order);
  this->is_high_res = resolution == internal::WakeUpResolution::kHigh;
}

namespace internal {

PostedTask::PostedTask(OnceClosure callback,
                       Location location,
                       TimeDelta delay,
                       Nestable nestable,
                       int task_type)
    : callback(std::move(callback)),
      location(location),
      delay(delay),
      nestable(nestable),
      task_type(task_type) {}

PostedTask::PostedTask(PostedTask&& move_from) noexcept
    : callback(std::move(move_from.callback)),
      location(move_from.location),
      delay(move_from.delay),
      nestable(move_from.nestable),
      task_type(move_from.task_type) {}

PostedTask::~PostedTask() = default;

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
