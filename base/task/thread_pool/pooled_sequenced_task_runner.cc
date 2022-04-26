// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/pooled_sequenced_task_runner.h"

#include "base/sequence_token.h"
#include "base/task/default_delayed_task_handle_delegate.h"
#include "base/task/task_features.h"

namespace base {
namespace internal {

namespace {

// Leeway value applied to delayed tasks. An atomic is used here because the
// value is queried from multiple threads when tasks are posted cross-thread,
// which can race with its initialization.
std::atomic<TimeDelta> g_task_leeway{PendingTask::kDefaultLeeway};

}  // namespace

// static
void PooledSequencedTaskRunner::InitializeFeatures() {
  g_task_leeway.store(kTaskLeewayParam.Get(), std::memory_order_relaxed);
}

PooledSequencedTaskRunner::PooledSequencedTaskRunner(
    const TaskTraits& traits,
    PooledTaskRunnerDelegate* pooled_task_runner_delegate)
    : pooled_task_runner_delegate_(pooled_task_runner_delegate),
      sequence_(MakeRefCounted<Sequence>(traits,
                                         this,
                                         TaskSourceExecutionMode::kSequenced)) {
}

PooledSequencedTaskRunner::~PooledSequencedTaskRunner() = default;

bool PooledSequencedTaskRunner::PostDelayedTask(const Location& from_here,
                                                OnceClosure closure,
                                                TimeDelta delay) {
  if (!PooledTaskRunnerDelegate::MatchesCurrentDelegate(
          pooled_task_runner_delegate_)) {
    return false;
  }

  Task task(from_here, std::move(closure), TimeTicks::Now(), delay,
            g_task_leeway.load(std::memory_order_relaxed));

  // Post the task as part of |sequence_|.
  return pooled_task_runner_delegate_->PostTaskWithSequence(std::move(task),
                                                            sequence_);
}

bool PooledSequencedTaskRunner::PostDelayedTaskAt(
    subtle::PostDelayedTaskPassKey,
    const Location& from_here,
    OnceClosure closure,
    TimeTicks delayed_run_time,
    subtle::DelayPolicy delay_policy) {
  if (!PooledTaskRunnerDelegate::MatchesCurrentDelegate(
          pooled_task_runner_delegate_)) {
    return false;
  }

  Task task(from_here, std::move(closure), TimeTicks::Now(), delayed_run_time,
            g_task_leeway.load(std::memory_order_relaxed), delay_policy);

  // Post the task as part of |sequence_|.
  return pooled_task_runner_delegate_->PostTaskWithSequence(std::move(task),
                                                            sequence_);
}

bool PooledSequencedTaskRunner::PostNonNestableDelayedTask(
    const Location& from_here,
    OnceClosure closure,
    TimeDelta delay) {
  // Tasks are never nested within the thread pool.
  return PostDelayedTask(from_here, std::move(closure), delay);
}

bool PooledSequencedTaskRunner::RunsTasksInCurrentSequence() const {
  return sequence_->token() == SequenceToken::GetForCurrentThread();
}

void PooledSequencedTaskRunner::UpdatePriority(TaskPriority priority) {
  pooled_task_runner_delegate_->UpdatePriority(sequence_, priority);
}

}  // namespace internal
}  // namespace base
