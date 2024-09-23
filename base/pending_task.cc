// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pending_task.h"

#include "base/task/task_features.h"

namespace base {

TaskMetadata::TaskMetadata() = default;

TaskMetadata::TaskMetadata(const Location& posted_from,
                           TimeTicks queue_time,
                           TimeTicks delayed_run_time,
                           TimeDelta leeway,
                           subtle::DelayPolicy delay_policy)
    : posted_from(posted_from),
      queue_time(queue_time),
      delayed_run_time(delayed_run_time),
      leeway(leeway),
      delay_policy(delay_policy) {}

TaskMetadata::TaskMetadata(TaskMetadata&& other) = default;
TaskMetadata::TaskMetadata(const TaskMetadata& other) = default;

TaskMetadata::~TaskMetadata() = default;

TaskMetadata& TaskMetadata::operator=(TaskMetadata&& other) = default;
TaskMetadata& TaskMetadata::operator=(const TaskMetadata& other) = default;

PendingTask::PendingTask() = default;

PendingTask::PendingTask(const Location& posted_from,
                         OnceClosure task,
                         TimeTicks queue_time,
                         TimeTicks delayed_run_time,
                         TimeDelta leeway,
                         subtle::DelayPolicy delay_policy)
    : TaskMetadata(posted_from,
                   queue_time,
                   delayed_run_time,
                   leeway,
                   delay_policy),
      task(std::move(task)) {}

PendingTask::PendingTask(const TaskMetadata& metadata, OnceClosure task)
    : TaskMetadata(metadata), task(std::move(task)) {}

PendingTask::PendingTask(PendingTask&& other) = default;

PendingTask::~PendingTask() = default;

PendingTask& PendingTask::operator=(PendingTask&& other) = default;

TimeTicks TaskMetadata::GetDesiredExecutionTime() const {
  if (!delayed_run_time.is_null())
    return delayed_run_time;
  return queue_time;
}

TimeTicks TaskMetadata::earliest_delayed_run_time() const {
  DCHECK(!delayed_run_time.is_null());
  if (delay_policy == subtle::DelayPolicy::kFlexiblePreferEarly)
    return delayed_run_time - leeway;
  return delayed_run_time;
}

TimeTicks TaskMetadata::latest_delayed_run_time() const {
  DCHECK(!delayed_run_time.is_null());
  if (delay_policy == subtle::DelayPolicy::kFlexibleNoSooner)
    return delayed_run_time + leeway;
  return delayed_run_time;
}

}  // namespace base
