// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pending_task.h"


namespace base {

PendingTask::PendingTask() = default;

PendingTask::PendingTask(const Location& posted_from, OnceClosure task)
    : PendingTask(posted_from, std::move(task), TimeTicks(), TimeTicks()) {}

PendingTask::PendingTask(const Location& posted_from,
                         OnceClosure task,
                         TimeTicks queue_time,
                         TimeTicks delayed_run_time)
    : task(std::move(task)),
      posted_from(posted_from),
      queue_time(queue_time),
      delayed_run_time(delayed_run_time) {}

PendingTask::PendingTask(PendingTask&& other) = default;

PendingTask::~PendingTask() = default;

PendingTask& PendingTask::operator=(PendingTask&& other) = default;

bool PendingTask::operator<(const PendingTask& other) const {
  // Since the top of a priority queue is defined as the "greatest" element, we
  // need to invert the comparison here.  We want the smaller time to be at the
  // top of the heap.

  if (delayed_run_time < other.delayed_run_time)
    return false;

  if (delayed_run_time > other.delayed_run_time)
    return true;

  // If the times happen to match, then we use the sequence number to decide.
  // Compare the difference to support integer roll-over.
  return (sequence_num - other.sequence_num) > 0;
}

TimeTicks PendingTask::GetDesiredExecutionTime() const {
  if (!delayed_run_time.is_null())
    return delayed_run_time;
  return queue_time;
}

}  // namespace base
