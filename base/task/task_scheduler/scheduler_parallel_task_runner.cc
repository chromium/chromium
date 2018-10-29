// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/scheduler_parallel_task_runner.h"

#include "base/task/task_scheduler/sequence.h"

namespace base {
namespace internal {

SchedulerParallelTaskRunner::SchedulerParallelTaskRunner(
    const TaskTraits& traits,
    SchedulerTaskRunnerDelegate* scheduler_task_runner_delegate)
    : traits_(traits),
      scheduler_task_runner_delegate_(scheduler_task_runner_delegate) {}

SchedulerParallelTaskRunner::~SchedulerParallelTaskRunner() = default;

bool SchedulerParallelTaskRunner::PostDelayedTask(const Location& from_here,
                                                  OnceClosure closure,
                                                  TimeDelta delay) {
  if (!SchedulerTaskRunnerDelegate::Exists())
    return false;

  // Post the task as part of a one-off single-task Sequence.
  return scheduler_task_runner_delegate_->PostTaskWithSequence(
      Task(from_here, std::move(closure), delay),
      MakeRefCounted<Sequence>(traits_));
}

bool SchedulerParallelTaskRunner::RunsTasksInCurrentSequence() const {
  return scheduler_task_runner_delegate_->IsRunningPoolWithTraits(traits_);
}

}  // namespace internal
}  // namespace base
