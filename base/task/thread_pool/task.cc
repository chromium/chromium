// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/task.h"

#include <utility>

namespace base {
namespace internal {

Task::Task(const Location& posted_from,
           OnceClosure task,
           TimeTicks queue_time,
           TimeDelta delay,
           TimeDelta leeway,
           int sequence_num)
    : Task(posted_from,
           std::move(task),
           queue_time,
           delay.is_zero() ? TimeTicks() : queue_time + delay,
           leeway,
           subtle::DelayPolicy::kFlexibleNoSooner,
           sequence_num) {}

Task::Task(const Location& posted_from,
           OnceClosure task,
           TimeTicks queue_time,
           TimeTicks delayed_run_time,
           TimeDelta leeway,
           subtle::DelayPolicy delay_policy,
           int sequence_num)
    : PendingTask(posted_from,
                  std::move(task),
                  queue_time,
                  delayed_run_time,
                  leeway,
                  delay_policy) {
  this->sequence_num = sequence_num;
}

Task::Task(const TaskMetadata& metadata, OnceClosure task)
    : PendingTask(metadata, std::move(task)) {}

// This should be "= default but MSVC has trouble with "noexcept = default" in
// this case.
Task::Task(Task&& other) noexcept : PendingTask(std::move(other)) {}

Task& Task::operator=(Task&& other) = default;

}  // namespace internal
}  // namespace base
