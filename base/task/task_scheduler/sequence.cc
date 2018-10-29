// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/sequence.h"

#include <utility>

#include "base/critical_closure.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace base {
namespace internal {

Sequence::Sequence(const TaskTraits& traits) : traits_(traits) {}

bool Sequence::PushTask(Task task) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  DCHECK(task.sequenced_time.is_null());
  task.sequenced_time = base::TimeTicks::Now();

  task.task =
      traits_.shutdown_behavior() == TaskShutdownBehavior::BLOCK_SHUTDOWN
          ? MakeCriticalClosure(std::move(task.task))
          : std::move(task.task);

  AutoSchedulerLock auto_lock(lock_);
  queue_.push(std::move(task));

  // Return true if the sequence was empty before the push.
  return queue_.size() == 1;
}

Optional<Task> Sequence::TakeTask() {
  AutoSchedulerLock auto_lock(lock_);
  DCHECK(!queue_.empty());
  DCHECK(queue_.front().task);

  return std::move(queue_.front());
}

bool Sequence::Pop() {
  AutoSchedulerLock auto_lock(lock_);
  DCHECK(!queue_.empty());
  DCHECK(!queue_.front().task);
  queue_.pop();
  return queue_.empty();
}

SequenceSortKey Sequence::GetSortKey() const {
  base::TimeTicks next_task_sequenced_time;

  {
    AutoSchedulerLock auto_lock(lock_);
    DCHECK(!queue_.empty());

    // Save the sequenced time of the next task in the sequence.
    next_task_sequenced_time = queue_.front().sequenced_time;
  }

  return SequenceSortKey(traits_.priority(), next_task_sequenced_time);
}

Sequence::~Sequence() = default;

}  // namespace internal
}  // namespace base
