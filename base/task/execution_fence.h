// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_EXECUTION_FENCE_H_
#define BASE_TASK_EXECUTION_FENCE_H_

#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/task/sequence_manager/task_queue.h"

namespace base {

namespace sequence_manager {
class SequenceManager;
}

// A ScopedThreadPoolExecutionFence prevents new tasks from being scheduled in
// the ThreadPool within its scope. Multiple fences can exist at the same time.
// Upon destruction of all ScopedThreadPoolExecutionFences, tasks that were
// preeempted are released. Note: the constructor of
// ScopedThreadPoolExecutionFence will not wait for currently running tasks (as
// they were posted before entering this scope and do not violate the contract;
// some of them could be CONTINUE_ON_SHUTDOWN and waiting for them to complete
// is ill-advised).
// TODO(crbug.com/454908699): Remove the "Scoped" prefix. It's too verbose.
class BASE_EXPORT ScopedThreadPoolExecutionFence {
 public:
  ScopedThreadPoolExecutionFence();
  ScopedThreadPoolExecutionFence(const ScopedThreadPoolExecutionFence&) =
      delete;
  ScopedThreadPoolExecutionFence& operator=(
      const ScopedThreadPoolExecutionFence&) = delete;
  ~ScopedThreadPoolExecutionFence();
};

// ScopedBestEffortExecutionFence is similar to ScopedThreadPoolExecutionFence,
// but only prevents new tasks of BEST_EFFORT priority from being scheduled.
// See ScopedThreadPoolExecutionFence for the full semantics.
//
// By default this only applies to tasks posted to the ThreadPool, same as
// ScopedThreadPoolExecutionFence. To apply fences to other task queues, call
// AddSequenceManager.
// TODO(crbug.com/454908699): Remove the "Scoped" prefix. It's too verbose.
class BASE_EXPORT ScopedBestEffortExecutionFence {
 public:
  ScopedBestEffortExecutionFence();
  ScopedBestEffortExecutionFence(const ScopedBestEffortExecutionFence&) =
      delete;
  ScopedBestEffortExecutionFence& operator=(
      const ScopedBestEffortExecutionFence&) = delete;
  ~ScopedBestEffortExecutionFence();

  // ScopedBestEffortExecutionFences created after this will also preempt
  // best-effort tasks posted to the backing sequence of `sequence_manager`.
  // Existing fences aren't affected. Does nothing if `sequence_manager` doesn't
  // define a best-effort priority. Must be called on the thread that created
  // `sequence_manager`.
  // TODO(crbug.com/441949788): Currently only sequence managers bound to the
  // main thread are supported.
  static void AddSequenceManager(
      sequence_manager::SequenceManager* sequence_manager);

  // ScopedBestEffortExecutionFences created after this will no longer preempt
  // best-effort tasks posted to the backing sequence of `sequence_manager`.
  // Existing fences aren't affected. Must be called on the thread that called
  // AddSequenceManager().
  static void RemoveSequenceManager(
      sequence_manager::SequenceManager* sequence_manager);

 private:
  // Voters used to disable additional task queues.
  std::vector<std::unique_ptr<sequence_manager::TaskQueue::QueueEnabledVoter>>
      task_queue_voters_;
};

}  // namespace base

#endif  // BASE_TASK_EXECUTION_FENCE_H_
