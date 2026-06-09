// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/pooled_parallel_task_runner.h"

#include "base/task/thread_pool/pooled_task_runner_delegate.h"
#include "base/task/thread_pool/sequence.h"

namespace base::internal {

PooledParallelTaskRunner::PooledParallelTaskRunner(
    const TaskTraits& traits,
    PooledTaskRunnerDelegate* pooled_task_runner_delegate,
    bool inherit_task_importance_by_default)
    : traits_(traits),
      pooled_task_runner_delegate_(pooled_task_runner_delegate),
      inherit_task_importance_by_default_(inherit_task_importance_by_default) {}

PooledParallelTaskRunner::~PooledParallelTaskRunner() = default;

bool PooledParallelTaskRunner::PostDelayedTask(const Location& from_here,
                                               OnceClosure closure,
                                               TimeDelta delay) {
  if (!PooledTaskRunnerDelegate::MatchesCurrentDelegate(
          pooled_task_runner_delegate_)) {
    return false;
  }

  // Post the task as part of a one-off single-task Sequence.
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      traits_, nullptr, TaskSourceExecutionMode::kParallel,
      GetCurrentTaskImportance(), inherit_task_importance_by_default_);

  return pooled_task_runner_delegate_->PostTaskWithSequence(
      Task(from_here, std::move(closure), TimeTicks::Now(), delay),
      std::move(sequence));
}

}  // namespace base::internal
