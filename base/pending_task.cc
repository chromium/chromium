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

TimeTicks PendingTask::GetDesiredExecutionTime() const {
  if (!delayed_run_time.is_null())
    return delayed_run_time;
  return queue_time;
}

}  // namespace base
