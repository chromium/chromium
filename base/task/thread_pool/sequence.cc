// Copyright 2016 The Chromium Authors
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

bool Sequence::Transaction::ShouldBeQueued() const {
  // A sequence should be queued to the immediate queue after receiving a new
  // immediate Task, or queued to or updated in the delayed queue after
  // receiving a new delayed Task, if it's not already in the immediate queue
  // and the pool is not running any task from it. WillRunTask() can racily
  // modify |current_location_|, but always from |kImmediateQueue| to
  // |kInWorker|. In that case, ShouldBeQueued() returns false whether
  // WillRunTask() runs immediately before or after.
  // When pushing a delayed task, a sequence can become ready at any time,
  // triggering OnBecomeReady() which racily modifies |current_location_|
  // from kDelayedQueue to kImmediateQueue. In that case this function may
  // return true which immediately becomes incorrect. This race is resolved
  // outside of this class. See my comment on ShouldBeQueued() in the header
  // file.
  auto current_location =
      sequence()->current_location_.load(std::memory_order_relaxed);
  if (current_location == Sequence::SequenceLocation::kImmediateQueue ||
      current_location == Sequence::SequenceLocation::kInWorker) {
    return false;
  }

  return true;
}

bool Sequence::Transaction::TopDelayedTaskWillChange(Task& delayed_task) const {
  if (sequence()->IsEmpty())
    return true;
  return delayed_task.latest_delayed_run_time() <
         sequence()->delayed_queue_.top().latest_delayed_run_time();
}

void Sequence::Transaction::PushImmediateTask(Task task) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  DCHECK(!task.queue_time.is_null());

  auto current_location =
      sequence()->current_location_.load(std::memory_order_relaxed);
  bool was_unretained =
      sequence()->IsEmpty() &&
      current_location != Sequence::SequenceLocation::kInWorker;
  bool queue_was_empty = sequence()->queue_.empty();

  task.task = sequence()->traits_.shutdown_behavior() ==
                      TaskShutdownBehavior::BLOCK_SHUTDOWN
                  ? MakeCriticalClosure(
                        task.posted_from, std::move(task.task),
                        /*is_immediate=*/task.delayed_run_time.is_null())
                  : std::move(task.task);

  sequence()->queue_.push(std::move(task));

  if (queue_was_empty) {
    sequence()->ready_time_.store(sequence()->GetNextReadyTime(),
                                  std::memory_order_relaxed);
  }

  if (current_location == Sequence::SequenceLocation::kDelayedQueue ||
      current_location == Sequence::SequenceLocation::kNone) {
    sequence()->current_location_.store(
        Sequence::SequenceLocation::kImmediateQueue, std::memory_order_relaxed);
  }

  // AddRef() matched by manual Release() when the sequence has no more tasks
  // to run (in DidProcessTask() or Clear()).
  if (was_unretained && sequence()->task_runner())
    sequence()->task_runner()->AddRef();
}

void Sequence::Transaction::PushDelayedTask(Task task) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  DCHECK(!task.queue_time.is_null());
  DCHECK(!task.delayed_run_time.is_null());

  auto current_location =
      sequence()->current_location_.load(std::memory_order_relaxed);
  bool was_unretained =
      sequence()->IsEmpty() &&
      current_location != Sequence::SequenceLocation::kInWorker;

  task.task =
      sequence()->traits_.shutdown_behavior() ==
              TaskShutdownBehavior::BLOCK_SHUTDOWN
          ? MakeCriticalClosure(task.posted_from, std::move(task.task), false)
          : std::move(task.task);

  sequence()->delayed_queue_.insert(std::move(task));

  if (sequence()->queue_.empty()) {
    sequence()->ready_time_.store(sequence()->GetNextReadyTime(),
                                  std::memory_order_relaxed);
  }

  auto expected_location = Sequence::SequenceLocation::kNone;
  sequence()->current_location_.compare_exchange_strong(
      expected_location, Sequence::SequenceLocation::kDelayedQueue,
      std::memory_order_relaxed);

  // AddRef() matched by manual Release() when the sequence has no more tasks
  // to run (in DidProcessTask() or Clear()).
  if (was_unretained && sequence()->task_runner())
    sequence()->task_runner()->AddRef();
}

// Delayed tasks are ordered by latest_delayed_run_time(). The top task may
// not be the first task eligible to run, but tasks will always become ripe
// before their latest_delayed_run_time().
bool Sequence::DelayedTaskGreater::operator()(const Task& lhs,
                                              const Task& rhs) const {
  TimeTicks lhs_latest_delayed_run_time = lhs.latest_delayed_run_time();
  TimeTicks rhs_latest_delayed_run_time = rhs.latest_delayed_run_time();
  return std::tie(lhs_latest_delayed_run_time, lhs.sequence_num) >
         std::tie(rhs_latest_delayed_run_time, rhs.sequence_num);
}

TaskSource::RunStatus Sequence::WillRunTask() {
  // There should never be a second call to WillRunTask() before DidProcessTask
  // since the RunStatus is always marked a saturated.

  DCHECK_EQ(current_location_.load(std::memory_order_relaxed),
            Sequence::SequenceLocation::kImmediateQueue);

  // It's ok to access |current_location_| outside of a Transaction since
  // WillRunTask() is externally synchronized, always called in sequence with
  // OnBecomeReady(), TakeTask(), WillReEnqueue() and DidProcessTask() and only
  // called if sequence is in immediate queue. Even though it can get racy with
  // ShouldBeQueued()/PushImmediateTask()/PushDelayedTask(), the behavior of
  // each function is not affected as explained in ShouldBeQueued().
  current_location_.store(Sequence::SequenceLocation::kInWorker,
                          std::memory_order_relaxed);

  return RunStatus::kAllowedSaturated;
}

void Sequence::OnBecomeReady() {
  // This should always be called from a worker thread at a time and it will be
  // called only before WillRunTask().
  DCHECK(current_location_.load(std::memory_order_relaxed) ==
         Sequence::SequenceLocation::kDelayedQueue);

  // It's ok to access |current_location_| outside of a Transaction since
  // OnBecomeReady() is externally synchronized and always called in sequence
  // with WillRunTask(). This can get racy with
  // ShouldBeQueued()/PushDelayedTask(). See comment in
  // ShouldBeQueued() to see how races with this function are resolved.
  current_location_.store(Sequence::SequenceLocation::kImmediateQueue,
                          std::memory_order_relaxed);
}

size_t Sequence::GetRemainingConcurrency() const {
  return 1;
}

Task Sequence::TakeNextImmediateTask() {
  Task next_task = std::move(queue_.front());
  queue_.pop();
  return next_task;
}

Task Sequence::TakeEarliestTask() {
  if (queue_.empty())
    return delayed_queue_.take_top();

  if (delayed_queue_.empty())
    return TakeNextImmediateTask();

  // Both queues contain at least a task. Decide from which one the task should
  // be taken.
  if (queue_.front().queue_time <=
      delayed_queue_.top().latest_delayed_run_time())
    return TakeNextImmediateTask();

  return delayed_queue_.take_top();
}

TimeTicks Sequence::GetNextReadyTime() {
  if (queue_.empty())
    return delayed_queue_.top().latest_delayed_run_time();

  if (delayed_queue_.empty())
    return queue_.front().queue_time;

  return std::min(queue_.front().queue_time,
                  delayed_queue_.top().latest_delayed_run_time());
}

Task Sequence::TakeTask(TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);

  DCHECK(current_location_.load(std::memory_order_relaxed) ==
         Sequence::SequenceLocation::kInWorker);
  DCHECK(!queue_.empty() || !delayed_queue_.empty());

  auto next_task = TakeEarliestTask();

  if (!IsEmpty())
    ready_time_.store(GetNextReadyTime(), std::memory_order_relaxed);

  return next_task;
}

bool Sequence::DidProcessTask(TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);
  // There should never be a call to DidProcessTask without an associated
  // WillRunTask().
  DCHECK(current_location_.load(std::memory_order_relaxed) ==
         Sequence::SequenceLocation::kInWorker);

  // See comment on TaskSource::task_runner_ for lifetime management details.
  if (IsEmpty()) {
    ReleaseTaskRunner();
    current_location_.store(Sequence::SequenceLocation::kNone,
                            std::memory_order_relaxed);
    return false;
  }

  // Let the caller re-enqueue this non-empty Sequence regardless of
  // |run_result| so it can continue churning through this Sequence's tasks and
  // skip/delete them in the proper scope.
  return true;
}

bool Sequence::WillReEnqueue(TimeTicks now,
                             TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);
  // This should always be called from a worker thread and it will be
  // called after DidProcessTask().
  DCHECK(current_location_.load(std::memory_order_relaxed) ==
         Sequence::SequenceLocation::kInWorker);

  bool has_ready_tasks = HasReadyTasks(now);
  if (has_ready_tasks) {
    current_location_.store(Sequence::SequenceLocation::kImmediateQueue,
                            std::memory_order_relaxed);
  } else {
    current_location_.store(Sequence::SequenceLocation::kDelayedQueue,
                            std::memory_order_relaxed);
  }

  return has_ready_tasks;
}

bool Sequence::HasReadyTasks(TimeTicks now) const {
  return HasRipeDelayedTasks(now) || HasImmediateTasks();
}

bool Sequence::HasRipeDelayedTasks(TimeTicks now) const {
  if (delayed_queue_.empty())
    return false;

  if (!delayed_queue_.top().task.MaybeValid())
    return true;

  return delayed_queue_.top().earliest_delayed_run_time() <= now;
}

bool Sequence::HasImmediateTasks() const {
  return !queue_.empty();
}

TaskSourceSortKey Sequence::GetSortKey() const {
  return TaskSourceSortKey(priority_racy(),
                           ready_time_.load(std::memory_order_relaxed));
}

TimeTicks Sequence::GetDelayedSortKey() const {
  return GetReadyTime();
}

Task Sequence::Clear(TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);
  // See comment on TaskSource::task_runner_ for lifetime management details.
  if (!IsEmpty() && current_location_.load(std::memory_order_relaxed) !=
                        Sequence::SequenceLocation::kInWorker) {
    ReleaseTaskRunner();
  }

  return Task(
      FROM_HERE,
      base::BindOnce(
          [](base::queue<Task> queue,
             base::IntrusiveHeap<Task, DelayedTaskGreater> delayed_queue) {
            while (!queue.empty())
              queue.pop();

            while (!delayed_queue.empty())
              delayed_queue.pop();
          },
          std::move(queue_), std::move(delayed_queue_)),
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

bool Sequence::IsEmpty() const {
  return queue_.empty() && delayed_queue_.empty();
}

TimeTicks Sequence::GetReadyTime() const {
  return ready_time_.load(std::memory_order_relaxed);
}

}  // namespace internal
}  // namespace base
