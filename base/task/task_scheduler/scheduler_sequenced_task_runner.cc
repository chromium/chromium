// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/scheduler_sequenced_task_runner.h"

#include "base/sequence_token.h"

namespace base {
namespace internal {

SchedulerSequencedTaskRunner::SchedulerSequencedTaskRunner(
    const TaskTraits& traits,
    SchedulerTaskRunnerDelegate* scheduler_task_runner_delegate)
    : traits_(traits),
      scheduler_task_runner_delegate_(scheduler_task_runner_delegate),
      sequence_(MakeRefCounted<Sequence>(traits)) {}

SchedulerSequencedTaskRunner::~SchedulerSequencedTaskRunner() = default;

bool SchedulerSequencedTaskRunner::PostDelayedTask(const Location& from_here,
                                                   OnceClosure closure,
                                                   TimeDelta delay) {
  if (!SchedulerTaskRunnerDelegate::Exists())
    return false;

  Task task(from_here, std::move(closure), delay);
  task.sequenced_task_runner_ref = this;

  // Post the task as part of |sequence_|.
  return scheduler_task_runner_delegate_->PostTaskWithSequence(std::move(task),
                                                               sequence_);
}

bool SchedulerSequencedTaskRunner::PostNonNestableDelayedTask(
    const Location& from_here,
    OnceClosure closure,
    TimeDelta delay) {
  // Tasks are never nested within the task scheduler.
  return PostDelayedTask(from_here, std::move(closure), delay);
}

bool SchedulerSequencedTaskRunner::RunsTasksInCurrentSequence() const {
  return sequence_->token() == SequenceToken::GetForCurrentThread();
}

}  // namespace internal
}  // namespace base
