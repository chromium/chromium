// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pending_task.h"

#include "base/task/task_features.h"

namespace base {

PendingTask::PendingTask() = default;

PendingTask::PendingTask(const Location& posted_from,
                         OnceClosure task,
                         TimeTicks queue_time,
                         TimeTicks delayed_run_time,
                         TimeDelta leeway,
                         subtle::DelayPolicy delay_policy)
    : task(std::move(task)),
      posted_from(posted_from),
      queue_time(queue_time),
      delayed_run_time(delayed_run_time),
      leeway(leeway),
      delay_policy(delay_policy) {}

PendingTask::PendingTask(PendingTask&& other) = default;

PendingTask::~PendingTask() = default;

PendingTask& PendingTask::operator=(PendingTask&& other) = default;

TimeTicks PendingTask::GetDesiredExecutionTime() const {
  if (!delayed_run_time.is_null())
    return delayed_run_time;
  return queue_time;
}

TimeTicks PendingTask::earliest_delayed_run_time() const {
  DCHECK(!delayed_run_time.is_null());
  if (delay_policy == subtle::DelayPolicy::kFlexiblePreferEarly)
    return delayed_run_time - leeway;
  return delayed_run_time;
}

TimeTicks PendingTask::latest_delayed_run_time() const {
  DCHECK(!delayed_run_time.is_null());
  if (delay_policy == subtle::DelayPolicy::kFlexibleNoSooner)
    return delayed_run_time + leeway;
  return delayed_run_time;
}

}  // namespace base
