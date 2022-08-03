// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/sequence.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/critical_closure.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_features.h"
#include "base/time/time.h"

namespace base {
namespace internal {

Sequence::Transaction::Transaction(Sequence* sequence)
    : TaskSource::Transaction(sequence) {}

Sequence::Transaction::Transaction(Sequence::Transaction&& other) = default;

Sequence::Transaction::~Transaction() = default;

bool Sequence::Transaction::WillPushTask() const {
  // A sequence should be queued if it's not already in the queue and the pool
  // is not running any task from it. Otherwise, one of these must be true:
  // - The Sequence is already queued, or,
  // - A thread is running a Task from the Sequence. It is expected to reenqueue
  //   the Sequence once it's done running the Task.
  // Access to |current_location_| can get racy between calls to WillRunTask()
  // and WillPushTask(). WillRunTask() updates |current_location_| from
  // kImmediateQueue to kInWorker, it can only be called on sequence when
  // sequence is already in immediate queue so this behavior is always
  // guaranteed. Hence, WillPushTask behavior won't be affected no matter if
  // WillRunTask runs before or after it's called since it returns false
  // whether |current_location_| is set to kImmediateQueue or kInWorker.
  auto current_location =
      sequence()->current_location_.load(std::memory_order_relaxed);
  if (current_location == Sequence::SequenceLocation::kImmediateQueue) {
    return false;
  }

  if (current_location == Sequence::SequenceLocation::kInWorker) {
    return false;
  }

  return true;
}

void Sequence::Transaction::PushTask(Task task) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  DCHECK(!task.queue_time.is_null());

  bool should_be_queued = WillPushTask();
  task.task = sequence()->traits_.shutdown_behavior() ==
                      TaskShutdownBehavior::BLOCK_SHUTDOWN
                  ? MakeCriticalClosure(
                        task.posted_from, std::move(task.task),
                        /*is_immediate=*/task.delayed_run_time.is_null())
                  : std::move(task.task);

  if (sequence()->queue_.empty()) {
    sequence()->ready_time_.store(task.GetDesiredExecutionTime(),
                                  std::memory_order_relaxed);
  }
  sequence()->queue_.push(std::move(task));

  if (should_be_queued)
    sequence()->current_location_.store(
        Sequence::SequenceLocation::kImmediateQueue, std::memory_order_relaxed);

  // AddRef() matched by manual Release() when the sequence has no more tasks
  // to run (in DidProcessTask() or Clear()).
  if (should_be_queued && sequence()->task_runner())
    sequence()->task_runner()->AddRef();
}

TaskSource::RunStatus Sequence::WillRunTask() {
  // There should never be a second call to WillRunTask() before DidProcessTask
  // since the RunStatus is always marked a saturated.
  DCHECK(current_location_.load(std::memory_order_relaxed) !=
         Sequence::SequenceLocation::kInWorker);

  // It's ok to access |current_location_| outside of a Transaction since
  // WillRunTask() is externally synchronized, always called in sequence with
  // TakeTask() and DidProcessTask() and only called if sequence is in immediate
  // queue. Even though it can get racy with WillPushTask()/PushTask(), the
  // behavior of each function is not affected as explained in WillPushTask().
  current_location_.store(Sequence::SequenceLocation::kInWorker,
                          std::memory_order_relaxed);

  return RunStatus::kAllowedSaturated;
}

size_t Sequence::GetRemainingConcurrency() const {
  return 1;
}

Task Sequence::TakeTask(TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);

  DCHECK(current_location_.load(std::memory_order_relaxed) ==
         Sequence::SequenceLocation::kInWorker);
  DCHECK(!queue_.empty());
  DCHECK(queue_.front().task);

  auto next_task = std::move(queue_.front());
  queue_.pop();
  if (!queue_.empty()) {
    ready_time_.store(queue_.front().queue_time, std::memory_order_relaxed);
  }
  return next_task;
}

bool Sequence::DidProcessTask(TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);
  // There should never be a call to DidProcessTask without an associated
  // WillRunTask().
  DCHECK(current_location_.load(std::memory_order_relaxed) ==
         Sequence::SequenceLocation::kInWorker);

  // See comment on TaskSource::task_runner_ for lifetime management details.
  if (queue_.empty()) {
    ReleaseTaskRunner();
    current_location_.store(Sequence::SequenceLocation::kNone,
                            std::memory_order_relaxed);
    return false;
  }

  current_location_.store(Sequence::SequenceLocation::kImmediateQueue,
                          std::memory_order_relaxed);
  // Let the caller re-enqueue this non-empty Sequence regardless of
  // |run_result| so it can continue churning through this Sequence's tasks and
  // skip/delete them in the proper scope.
  return true;
}

TaskSourceSortKey Sequence::GetSortKey(
    bool /* disable_fair_scheduling */) const {
  return TaskSourceSortKey(priority_racy(),
                           ready_time_.load(std::memory_order_relaxed));
}

Task Sequence::Clear(TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);
  // See comment on TaskSource::task_runner_ for lifetime management details.
  if (!queue_.empty() && current_location_.load(std::memory_order_relaxed) !=
                             Sequence::SequenceLocation::kInWorker) {
    ReleaseTaskRunner();
  }

  return Task(FROM_HERE,
              base::BindOnce(
                  [](base::queue<Task> queue) {
                    while (!queue.empty())
                      queue.pop();
                  },
                  std::move(queue_)),
              TimeTicks(), TimeDelta());
}

void Sequence::ReleaseTaskRunner() {
  if (!task_runner())
    return;
  // No member access after this point, releasing |task_runner()| might delete
  // |this|.
  task_runner()->Release();
}

Sequence::Sequence(const TaskTraits& traits,
                   TaskRunner* task_runner,
                   TaskSourceExecutionMode execution_mode)
    : TaskSource(traits, task_runner, execution_mode) {}

Sequence::~Sequence() = default;

Sequence::Transaction Sequence::BeginTransaction() {
  return Transaction(this);
}

ExecutionEnvironment Sequence::GetExecutionEnvironment() {
  return {token_, &sequence_local_storage_};
}

Sequence::SequenceLocation Sequence::GetCurrentLocationForTesting() {
  return current_location_.load(std::memory_order_relaxed);
}

}  // namespace internal
}  // namespace base
