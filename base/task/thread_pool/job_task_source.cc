// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/job_task_source.h"

#include <bit>
#include <type_traits>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/common/checked_lock.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/pooled_task_runner_delegate.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace internal {

namespace {

// Capped to allow assigning task_ids from a bitfield.
constexpr size_t kMaxWorkersPerJob = 32;
static_assert(
    kMaxWorkersPerJob <=
        std::numeric_limits<
            std::invoke_result<decltype(&JobDelegate::GetTaskId),
                               JobDelegate>::type>::max(),
    "AcquireTaskId return type isn't big enough to fit kMaxWorkersPerJob");

}  // namespace

JobTaskSource::State::State() = default;
JobTaskSource::State::~State() = default;

JobTaskSource::State::Value JobTaskSource::State::Cancel() {
  return {value_.fetch_or(kCanceledMask, std::memory_order_relaxed)};
}

JobTaskSource::State::Value JobTaskSource::State::DecrementWorkerCount() {
  const uint32_t value_before_sub =
      value_.fetch_sub(kWorkerCountIncrement, std::memory_order_relaxed);
  DCHECK((value_before_sub >> kWorkerCountBitOffset) > 0);
  return {value_before_sub};
}

JobTaskSource::State::Value JobTaskSource::State::IncrementWorkerCount() {
  uint32_t value_before_add =
      value_.fetch_add(kWorkerCountIncrement, std::memory_order_relaxed);
  // The worker count must not overflow a uint8_t.
  DCHECK((value_before_add >> kWorkerCountBitOffset) < ((1 << 8) - 1));
  return {value_before_add};
}

JobTaskSource::State::Value JobTaskSource::State::Load() const {
  return {value_.load(std::memory_order_relaxed)};
}

JobTaskSource::JoinFlag::JoinFlag() = default;
JobTaskSource::JoinFlag::~JoinFlag() = default;

void JobTaskSource::JoinFlag::Reset() {
  value_.store(kNotWaiting, std::memory_order_relaxed);
}

void JobTaskSource::JoinFlag::SetWaiting() {
  value_.store(kWaitingForWorkerToYield, std::memory_order_relaxed);
}

bool JobTaskSource::JoinFlag::ShouldWorkerYield() {
  // The fetch_and() sets the state to kWaitingForWorkerToSignal if it was
  // previously kWaitingForWorkerToYield, otherwise it leaves it unchanged.
  return value_.fetch_and(kWaitingForWorkerToSignal,
                          std::memory_order_relaxed) ==
         kWaitingForWorkerToYield;
}

bool JobTaskSource::JoinFlag::ShouldWorkerSignal() {
  return value_.exchange(kNotWaiting, std::memory_order_relaxed) != kNotWaiting;
}

JobTaskSource::JobTaskSource(const Location& from_here,
                             const TaskTraits& traits,
                             RepeatingCallback<void(JobDelegate*)> worker_task,
                             MaxConcurrencyCallback max_concurrency_callback,
                             PooledTaskRunnerDelegate* delegate)
    : TaskSource(traits, TaskSourceExecutionMode::kJob),
      max_concurrency_callback_(std::move(max_concurrency_callback)),
      worker_task_(std::move(worker_task)),
      primary_task_(base::BindRepeating(
          [](JobTaskSource* self) {
            CheckedLock::AssertNoLockHeldOnCurrentThread();
            // Each worker task has its own delegate with associated state.
            JobDelegate job_delegate{self, self->delegate_};
            self->worker_task_.Run(&job_delegate);
          },
          base::Unretained(this))),
      task_metadata_(from_here),
      ready_time_(TimeTicks::Now()),
      delegate_(delegate) {
  DCHECK(delegate_);
  task_metadata_.sequence_num = -1;
}

JobTaskSource::~JobTaskSource() {
  // Make sure there's no outstanding active run operation left.
  DCHECK_EQ(state_.Load().worker_count(), 0U);
}

ExecutionEnvironment JobTaskSource::GetExecutionEnvironment() {
  return {SequenceToken::Create()};
}

void JobTaskSource::WillEnqueue(int sequence_num, TaskAnnotator& annotator) {
  if (task_metadata_.sequence_num != -1) {
    // WillEnqueue() was already called.
    return;
  }
  task_metadata_.sequence_num = sequence_num;
  annotator.WillQueueTask("ThreadPool_PostJob", &task_metadata_);
}

bool JobTaskSource::WillJoin() {
  TRACE_EVENT0("base", "Job.WaitForParticipationOpportunity");
  CheckedAutoLock auto_lock(worker_lock_);
  DCHECK(!worker_released_condition_);  // This may only be called once.
  worker_lock_.CreateConditionVariableAndEmplace(worker_released_condition_);
  // Prevent wait from triggering a ScopedBlockingCall as this would cause
  // |ThreadGroup::lock_| to be acquired, causing lock inversion.
  worker_released_condition_->declare_only_used_while_idle();
  const auto state_before_add = state_.IncrementWorkerCount();

  if (!state_before_add.is_canceled() &&
      state_before_add.worker_count() <
          GetMaxConcurrency(state_before_add.worker_count())) {
    return true;
  }
  return WaitForParticipationOpportunity();
}

bool JobTaskSource::RunJoinTask() {
  JobDelegate job_delegate{this, nullptr};
  worker_task_.Run(&job_delegate);

  // It is safe to read |state_| without a lock since this variable is atomic
  // and the call to GetMaxConcurrency() is used for a best effort early exit.
  // Stale values will only cause WaitForParticipationOpportunity() to be
  // called.
  const auto state = TS_UNCHECKED_READ(state_).Load();
  // The condition is slightly different from the one in WillJoin() since we're
  // using |state| that was already incremented to include the joining thread.
  if (!state.is_canceled() &&
      state.worker_count() <= GetMaxConcurrency(state.worker_count() - 1)) {
    return true;
  }

  TRACE_EVENT0("base", "Job.WaitForParticipationOpportunity");
  CheckedAutoLock auto_lock(worker_lock_);
  return WaitForParticipationOpportunity();
}

void JobTaskSource::Cancel(TaskSource::Transaction* transaction) {
  // Sets the kCanceledMask bit on |state_| so that further calls to
  // WillRunTask() never succeed. std::memory_order_relaxed without a lock is
  // safe because this task source never needs to be re-enqueued after Cancel().
  TS_UNCHECKED_READ(state_).Cancel();
}

// EXCLUSIVE_LOCK_REQUIRED(worker_lock_)
bool JobTaskSource::WaitForParticipationOpportunity() {
  DCHECK(!join_flag_.IsWaiting());

  // std::memory_order_relaxed is sufficient because no other state is
  // synchronized with |state_| outside of |lock_|.
  auto state = state_.Load();
  // |worker_count - 1| to exclude the joining thread which is not active.
  size_t max_concurrency = GetMaxConcurrency(state.worker_count() - 1);

  // Wait until either:
  //  A) |worker_count| is below or equal to max concurrency and state is not
  //  canceled.
  //  B) All other workers returned and |worker_count| is 1.
  while (!((state.worker_count() <= max_concurrency && !state.is_canceled()) ||
           state.worker_count() == 1)) {
    // std::memory_order_relaxed is sufficient because no other state is
    // synchronized with |join_flag_| outside of |lock_|.
    join_flag_.SetWaiting();

    // To avoid unnecessarily waiting, if either condition A) or B) change
    // |lock_| is taken and |worker_released_condition_| signaled if necessary:
    // 1- In DidProcessTask(), after worker count is decremented.
    // 2- In NotifyConcurrencyIncrease(), following a max_concurrency increase.
    worker_released_condition_->Wait();
    state = state_.Load();
    // |worker_count - 1| to exclude the joining thread which is not active.
    max_concurrency = GetMaxConcurrency(state.worker_count() - 1);
  }
  // It's possible though unlikely that the joining thread got a participation
  // opportunity without a worker signaling.
  join_flag_.Reset();

  // Case A:
  if (state.worker_count() <= max_concurrency && !state.is_canceled()) {
    return true;
  }
  // Case B:
  // Only the joining thread remains.
  DCHECK_EQ(state.worker_count(), 1U);
  DCHECK(state.is_canceled() || max_concurrency == 0U);
  state_.DecrementWorkerCount();
  // Prevent subsequent accesses to user callbacks.
  state_.Cancel();
  return false;
}

TaskSource::RunStatus JobTaskSource::WillRunTask() {
  CheckedAutoLock auto_lock(worker_lock_);
  auto state_before_add = state_.Load();

  // Don't allow this worker to run the task if either:
  //   A) |state_| was canceled.
  //   B) |worker_count| is already at |max_concurrency|.
  //   C) |max_concurrency| was lowered below or to |worker_count|.
  // Case A:
  if (state_before_add.is_canceled()) {
    return RunStatus::kDisallowed;
  }

  const size_t max_concurrency =
      GetMaxConcurrency(state_before_add.worker_count());
  if (state_before_add.worker_count() < max_concurrency) {
    state_before_add = state_.IncrementWorkerCount();
  }
  const size_t worker_count_before_add = state_before_add.worker_count();
  // Case B) or C):
  if (worker_count_before_add >= max_concurrency) {
    return RunStatus::kDisallowed;
  }

  DCHECK_LT(worker_count_before_add, max_concurrency);
  return max_concurrency == worker_count_before_add + 1
             ? RunStatus::kAllowedSaturated
             : RunStatus::kAllowedNotSaturated;
}

size_t JobTaskSource::GetRemainingConcurrency() const {
  // It is safe to read |state_| without a lock since this variable is atomic,
  // and no other state is synchronized with GetRemainingConcurrency().
  const auto state = TS_UNCHECKED_READ(state_).Load();
  if (state.is_canceled()) {
    return 0;
  }
  const size_t max_concurrency = GetMaxConcurrency(state.worker_count());
  // Avoid underflows.
  if (state.worker_count() > max_concurrency)
    return 0;
  return max_concurrency - state.worker_count();
}

bool JobTaskSource::IsActive() const {
  CheckedAutoLock auto_lock(worker_lock_);
  auto state = state_.Load();
  return GetMaxConcurrency(state.worker_count()) != 0 ||
         state.worker_count() != 0;
}

size_t JobTaskSource::GetWorkerCount() const {
  return TS_UNCHECKED_READ(state_).Load().worker_count();
}

void JobTaskSource::NotifyConcurrencyIncrease() {
  // Avoid unnecessary locks when NotifyConcurrencyIncrease() is spuriously
  // called.
  if (GetRemainingConcurrency() == 0) {
    return;
  }

  {
    // Lock is taken to access |join_flag_| below and signal
    // |worker_released_condition_|.
    CheckedAutoLock auto_lock(worker_lock_);
    if (join_flag_.ShouldWorkerSignal()) {
      worker_released_condition_->Signal();
    }
  }

  // Make sure the task source is in the queue if not already.
  // Caveat: it's possible but unlikely that the task source has already reached
  // its intended concurrency and doesn't need to be enqueued if there
  // previously were too many worker. For simplicity, the task source is always
  // enqueued and will get discarded if already saturated when it is popped from
  // the priority queue.
  delegate_->EnqueueJobTaskSource(this);
}

size_t JobTaskSource::GetMaxConcurrency() const {
  return GetMaxConcurrency(TS_UNCHECKED_READ(state_).Load().worker_count());
}

size_t JobTaskSource::GetMaxConcurrency(size_t worker_count) const {
  return std::min(max_concurrency_callback_.Run(worker_count),
                  kMaxWorkersPerJob);
}

uint8_t JobTaskSource::AcquireTaskId() {
  static_assert(kMaxWorkersPerJob <= sizeof(assigned_task_ids_) * 8,
                "TaskId bitfield isn't big enough to fit kMaxWorkersPerJob.");
  uint32_t assigned_task_ids =
      assigned_task_ids_.load(std::memory_order_relaxed);
  uint32_t new_assigned_task_ids = 0;
  int task_id = 0;
  // memory_order_acquire on success, matched with memory_order_release in
  // ReleaseTaskId() so that operations done by previous threads that had
  // the same task_id become visible to the current thread.
  do {
    // Count trailing one bits. This is the id of the right-most 0-bit in
    // |assigned_task_ids|.
    task_id = std::countr_one(assigned_task_ids);
    new_assigned_task_ids = assigned_task_ids | (uint32_t(1) << task_id);
  } while (!assigned_task_ids_.compare_exchange_weak(
      assigned_task_ids, new_assigned_task_ids, std::memory_order_acquire,
      std::memory_order_relaxed));
  return static_cast<uint8_t>(task_id);
}

void JobTaskSource::ReleaseTaskId(uint8_t task_id) {
  // memory_order_release to match AcquireTaskId().
  uint32_t previous_task_ids = assigned_task_ids_.fetch_and(
      ~(uint32_t(1) << task_id), std::memory_order_release);
  DCHECK(previous_task_ids & (uint32_t(1) << task_id));
}

bool JobTaskSource::ShouldYield() {
  // It is safe to read |join_flag_| and |state_| without a lock since these
  // variables are atomic, keeping in mind that threads may not immediately see
  // the new value when it is updated.
  return TS_UNCHECKED_READ(join_flag_).ShouldWorkerYield() ||
         TS_UNCHECKED_READ(state_).Load().is_canceled();
}

Task JobTaskSource::TakeTask(TaskSource::Transaction* transaction) {
  // JobTaskSource members are not lock-protected so no need to acquire a lock
  // if |transaction| is nullptr.
  DCHECK_GT(TS_UNCHECKED_READ(state_).Load().worker_count(), 0U);
  DCHECK(primary_task_);
  return {task_metadata_, primary_task_};
}

bool JobTaskSource::DidProcessTask(TaskSource::Transaction* /*transaction*/) {
  // Lock is needed to access |join_flag_| below and signal
  // |worker_released_condition_|.
  CheckedAutoLock auto_lock(worker_lock_);
  const auto state_before_sub = state_.DecrementWorkerCount();

  if (join_flag_.ShouldWorkerSignal()) {
    worker_released_condition_->Signal();
  }

  // A canceled task source should never get re-enqueued.
  if (state_before_sub.is_canceled()) {
    return false;
  }

  DCHECK_GT(state_before_sub.worker_count(), 0U);

  // Re-enqueue the TaskSource if the task ran and the worker count is below the
  // max concurrency.
  // |worker_count - 1| to exclude the returning thread.
  return state_before_sub.worker_count() <=
         GetMaxConcurrency(state_before_sub.worker_count() - 1);
}

// This is a no-op and should always return true.
bool JobTaskSource::WillReEnqueue(TimeTicks now,
                                  TaskSource::Transaction* /*transaction*/) {
  return true;
}

// This is a no-op.
bool JobTaskSource::OnBecomeReady() {
  return false;
}

TaskSourceSortKey JobTaskSource::GetSortKey() const {
  return TaskSourceSortKey(priority_racy(), ready_time_,
                           TS_UNCHECKED_READ(state_).Load().worker_count());
}

// This function isn't expected to be called since a job is never delayed.
// However, the class still needs to provide an override.
TimeTicks JobTaskSource::GetDelayedSortKey() const {
  return TimeTicks();
}

// This function isn't expected to be called since a job is never delayed.
// However, the class still needs to provide an override.
bool JobTaskSource::HasReadyTasks(TimeTicks now) const {
  NOTREACHED();
}

std::optional<Task> JobTaskSource::Clear(TaskSource::Transaction* transaction) {
  Cancel();

  // Nothing is cleared since other workers might still racily run tasks. For
  // simplicity, the destructor will take care of it once all references are
  // released.
  return std::nullopt;
}

}  // namespace internal
}  // namespace base
