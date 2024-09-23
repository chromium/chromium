// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/sequence.h"

#include <utility>

#include "base/check.h"
#include "base/critical_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/stack_allocated.h"
#include "base/task/task_features.h"
#include "base/time/time.h"

namespace base {
namespace internal {

namespace {

// Asserts that a lock is acquired and annotates the scope such that
// base/thread_annotations.h can recognize that the lock is acquired.
class SCOPED_LOCKABLE AnnotateLockAcquired {
  STACK_ALLOCATED();

 public:
  explicit AnnotateLockAcquired(const CheckedLock& lock)
      EXCLUSIVE_LOCK_FUNCTION(lock)
      : acquired_lock_(lock) {
    acquired_lock_.AssertAcquired();
  }

  ~AnnotateLockAcquired() UNLOCK_FUNCTION() { acquired_lock_.AssertAcquired(); }

 private:
  const CheckedLock& acquired_lock_;
};

void MaybeMakeCriticalClosure(TaskShutdownBehavior shutdown_behavior,
                              Task& task) {
  switch (shutdown_behavior) {
    case TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN:
      // Nothing to do.
      break;
    case TaskShutdownBehavior::SKIP_ON_SHUTDOWN:
      // MakeCriticalClosure() is arguably useful for SKIP_ON_SHUTDOWN, possibly
      // in combination with is_immediate=false. However, SKIP_ON_SHUTDOWN is
      // the default and hence the theoretical benefits don't warrant the
      // performance implications.
      break;
    case TaskShutdownBehavior::BLOCK_SHUTDOWN:
      task.task =
          MakeCriticalClosure(task.posted_from, std::move(task.task),
                              /*is_immediate=*/task.delayed_run_time.is_null());
      break;
  }
}

}  // namespace

Sequence::Transaction::Transaction(Sequence* sequence)
    : TaskSource::Transaction(sequence) {}

Sequence::Transaction::Transaction(Sequence::Transaction&& other) = default;

Sequence::Transaction::~Transaction() = default;

bool Sequence::Transaction::WillPushImmediateTask() {
  // In a Transaction.
  AnnotateLockAcquired annotate(sequence()->lock_);

  bool was_immediate =
      sequence()->is_immediate_.exchange(true, std::memory_order_relaxed);
  return !was_immediate;
}

void Sequence::Transaction::PushImmediateTask(Task task) {
  // In a Transaction.
  AnnotateLockAcquired annotate(sequence()->lock_);

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  DCHECK(!task.queue_time.is_null());
  DCHECK(sequence()->is_immediate_.load(std::memory_order_relaxed));

  bool was_unretained = sequence()->IsEmpty() && !sequence()->has_worker_;
  bool queue_was_empty = sequence()->queue_.empty();

  MaybeMakeCriticalClosure(sequence()->traits_.shutdown_behavior(), task);

  sequence()->queue_.push(std::move(task));

  if (queue_was_empty)
    sequence()->UpdateReadyTimes();

  // AddRef() matched by manual Release() when the sequence has no more tasks
  // to run (in DidProcessTask() or Clear()).
  if (was_unretained && sequence()->task_runner())
    sequence()->task_runner()->AddRef();
}

bool Sequence::Transaction::PushDelayedTask(Task task) {
  // In a Transaction.
  AnnotateLockAcquired annotate(sequence()->lock_);

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  DCHECK(!task.queue_time.is_null());
  DCHECK(!task.delayed_run_time.is_null());

  bool top_will_change = sequence()->DelayedSortKeyWillChange(task);
  bool was_empty = sequence()->IsEmpty();

  MaybeMakeCriticalClosure(sequence()->traits_.shutdown_behavior(), task);

  sequence()->delayed_queue_.insert(std::move(task));

  if (sequence()->queue_.empty())
    sequence()->UpdateReadyTimes();

  // AddRef() matched by manual Release() when the sequence has no more tasks
  // to run (in DidProcessTask() or Clear()).
  if (was_empty && !sequence()->has_worker_ && sequence()->task_runner())
    sequence()->task_runner()->AddRef();

  return top_will_change;
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
  DCHECK(!has_worker_);

  // It's ok to access |has_worker_| outside of a Transaction since
  // WillRunTask() is externally synchronized, always called in sequence with
  // TakeTask() and DidProcessTask() and only called if HasReadyTasks(), which
  // means it won't race with Push[Immediate/Delayed]Task().
  has_worker_ = true;
  return RunStatus::kAllowedSaturated;
}

bool Sequence::OnBecomeReady() {
  DCHECK(!has_worker_);
  // std::memory_order_relaxed is sufficient because no other state is
  // synchronized with |is_immediate_| outside of |lock_|.
  return !is_immediate_.exchange(true, std::memory_order_relaxed);
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

void Sequence::UpdateReadyTimes() {
  DCHECK(!IsEmpty());
  if (queue_.empty()) {
    latest_ready_time_.store(delayed_queue_.top().latest_delayed_run_time(),
                             std::memory_order_relaxed);
    earliest_ready_time_.store(delayed_queue_.top().earliest_delayed_run_time(),
                               std::memory_order_relaxed);
    return;
  }

  if (delayed_queue_.empty()) {
    latest_ready_time_.store(queue_.front().queue_time,
                             std::memory_order_relaxed);
  } else {
    latest_ready_time_.store(
        std::min(queue_.front().queue_time,
                 delayed_queue_.top().latest_delayed_run_time()),
        std::memory_order_relaxed);
  }
  earliest_ready_time_.store(TimeTicks(), std::memory_order_relaxed);
}

Task Sequence::TakeTask(TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);
  AnnotateLockAcquired annotate(lock_);

  DCHECK(has_worker_);
  DCHECK(is_immediate_.load(std::memory_order_relaxed));
  DCHECK(!queue_.empty() || !delayed_queue_.empty());

  auto next_task = TakeEarliestTask();

  if (!IsEmpty())
    UpdateReadyTimes();

  return next_task;
}

bool Sequence::DidProcessTask(TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);
  AnnotateLockAcquired annotate(lock_);

  // There should never be a call to DidProcessTask without an associated
  // WillRunTask().
  DCHECK(has_worker_);
  has_worker_ = false;

  // See comment on TaskSource::task_runner_ for lifetime management details.
  if (IsEmpty()) {
    is_immediate_.store(false, std::memory_order_relaxed);
    ReleaseTaskRunner();
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
  AnnotateLockAcquired annotate(lock_);

  // This should always be called from a worker thread and it will be
  // called after DidProcessTask().
  DCHECK(is_immediate_.load(std::memory_order_relaxed));

  bool has_ready_tasks = HasReadyTasks(now);
  if (!has_ready_tasks)
    is_immediate_.store(false, std::memory_order_relaxed);

  return has_ready_tasks;
}

bool Sequence::DelayedSortKeyWillChange(const Task& delayed_task) const {
  // If sequence has already been picked up by a worker or moved, no need to
  // proceed further here.
  if (is_immediate_.load(std::memory_order_relaxed)) {
    return false;
  }

  if (IsEmpty()) {
    return true;
  }

  return delayed_task.latest_delayed_run_time() <
         delayed_queue_.top().latest_delayed_run_time();
}

bool Sequence::HasReadyTasks(TimeTicks now) const {
  return now >= TS_UNCHECKED_READ(earliest_ready_time_)
                    .load(std::memory_order_relaxed);
}

bool Sequence::HasImmediateTasks() const {
  return !queue_.empty();
}

TaskSourceSortKey Sequence::GetSortKey() const {
  return TaskSourceSortKey(
      priority_racy(),
      TS_UNCHECKED_READ(latest_ready_time_).load(std::memory_order_relaxed));
}

TimeTicks Sequence::GetDelayedSortKey() const {
  return TS_UNCHECKED_READ(latest_ready_time_).load(std::memory_order_relaxed);
}

std::optional<Task> Sequence::Clear(TaskSource::Transaction* transaction) {
  CheckedAutoLockMaybe auto_lock(transaction ? nullptr : &lock_);
  AnnotateLockAcquired annotate(lock_);

  // See comment on TaskSource::task_runner_ for lifetime management details.
  if (!IsEmpty() && !has_worker_) {
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
      TimeTicks(), TimeDelta(), TimeDelta(),
      static_cast<int>(reinterpret_cast<intptr_t>(this)));
}

void Sequence::ReleaseTaskRunner() {
  if (!task_runner())
    return;
  // No member access after this point, releasing |task_runner()| might delete
  // |this|.
  task_runner()->Release();
}

Sequence::Sequence(const TaskTraits& traits,
                   SequencedTaskRunner* task_runner,
                   TaskSourceExecutionMode execution_mode)
    : TaskSource(traits, execution_mode), task_runner_(task_runner) {}

Sequence::~Sequence() = default;

Sequence::Transaction Sequence::BeginTransaction() {
  return Transaction(this);
}

ExecutionEnvironment Sequence::GetExecutionEnvironment() {
  if (execution_mode() == TaskSourceExecutionMode::kSingleThread) {
    return {token_, &sequence_local_storage_,
            static_cast<SingleThreadTaskRunner*>(task_runner())};
  }
  return {token_, &sequence_local_storage_, task_runner()};
}

bool Sequence::IsEmpty() const {
  return queue_.empty() && delayed_queue_.empty();
}

}  // namespace internal
}  // namespace base
