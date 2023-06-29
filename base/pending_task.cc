// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pending_task.h"

#include "base/task/task_features.h"

namespace base {

namespace {

// TODO(crbug.com/1153139): Reconcile with GetDefaultTaskLeeway() and
// kMinLowResolutionThresholdMs once GetDefaultTaskLeeway() == 16ms.
constexpr base::TimeDelta kMaxPreciseDelay = Milliseconds(64);

subtle::DelayPolicy MaybeOverrideDelayPolicy(subtle::DelayPolicy delay_policy,
                                             TimeTicks queue_time,
                                             TimeTicks delayed_run_time) {
  if (delayed_run_time.is_null())
    return subtle::DelayPolicy::kFlexibleNoSooner;
  DCHECK(!queue_time.is_null());
  if (delayed_run_time - queue_time >= kMaxPreciseDelay &&
      delay_policy == subtle::DelayPolicy::kPrecise) {
    return subtle::DelayPolicy::kFlexibleNoSooner;
  }
  return delay_policy;
}

}  // namespace

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
      delay_policy(MaybeOverrideDelayPolicy(delay_policy,
                                            queue_time,
                                            delayed_run_time)) {}

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
