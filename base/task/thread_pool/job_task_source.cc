// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/job_task_source.h"

#include <bit>
#include <limits>
#include <type_traits>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/task/common/checked_lock.h"
#include "base/task/thread_pool/pooled_task_runner_delegate.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

namespace base::internal {

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

JobTaskSourceNew::State::State() = default;
JobTaskSourceNew::State::~State() = default;

JobTaskSourceNew::State::Value JobTaskSourceNew::State::Cancel() {
  return {value_.fetch_or(kCanceledMask, std::memory_order_relaxed)};
}

JobTaskSourceNew::State::Value JobTaskSourceNew::State::IncrementWorkerCount() {
  uint32_t prev =
      value_.fetch_add(kWorkerCountIncrement, std::memory_order_relaxed);
  // The worker count must not overflow a uint8_t.
  DCHECK((prev >> kWorkerCountBitOffset) < ((1 << 8) - 1));
  return {prev};
}

JobTaskSourceNew::State::Value JobTaskSourceNew::State::DecrementWorkerCount() {
  uint32_t prev =
      value_.fetch_sub(kWorkerCountIncrement, std::memory_order_relaxed);
  DCHECK((prev >> kWorkerCountBitOffset) > 0);
  return {prev};
}

JobTaskSourceNew::State::Value JobTaskSourceNew::State::RequestSignalJoin() {
  uint32_t prev = value_.fetch_or(kSignalJoinMask, std::memory_order_relaxed);
  return {prev};
}

bool JobTaskSourceNew::State::FetchAndResetRequestSignalJoin() {
  uint32_t prev = value_.fetch_and(~kSignalJoinMask, std::memory_order_relaxed);
  return !!(prev & kSignalJoinMask);
}

bool JobTaskSourceNew::State::ShouldQueueUponCapacityIncrease() {
  // If `WillRunTask()` is running: setting
  // `kOutsideWillRunTaskOrMustReenqueueMask` ensures that this capacity
  // increase is taken into account in the returned `RunStatus`.
  //
  // If `WillRunTask()` is not running, setting
  // `kOutsideWillRunTaskOrMustReenqueueMask` is a no-op (already set).
  //
  // Release paired with Acquire in `ExitWillRunTask()`, see comment there.
  Value prev{
      value_.fetch_or(kQueuedMask | kOutsideWillRunTaskOrMustReenqueueMask,
                      std::memory_order_release)};
  return !prev.queued() && prev.outside_will_run_task_or_must_reenqueue();
}

JobTaskSourceNew::State::Value JobTaskSourceNew::State::EnterWillRunTask() {
  Value prev{
      value_.fetch_and(~(kQueuedMask | kOutsideWillRunTaskOrMustReenqueueMask),
                       std::memory_order_relaxed)};
  CHECK(prev.outside_will_run_task_or_must_reenqueue());
  return {prev};
}

bool JobTaskSourceNew::State::ExitWillRunTask(bool saturated) {
  uint32_t bits_to_set = kOutsideWillRunTaskOrMustReenqueueMask;
  if (!saturated) {
    // If the task source is not saturated, it will be re-enqueued.
    bits_to_set |= kQueuedMask;
  }

  // Acquire paired with Release in `ShouldQueueUponCapacityIncrease()` or
  // `WillReenqueue()` so that anything that runs after clearing
  // `kOutsideWillRunTaskOrMustReenqueueMask` sees max concurrency changes
  // applied before setting it.
  Value prev{value_.fetch_or(bits_to_set, std::memory_order_acquire)};

  // `kQueuedMask` and `kOutsideWillRunTaskOrMustReenqueueMask` were cleared by
  // `EnterWillRunTask()`. Since then, they may have *both* been set by
  //  `ShouldQueueUponCapacityIncrease()` or `WillReenqueue()`.
  CHECK_EQ(prev.queued(), prev.outside_will_run_task_or_must_reenqueue());

  return prev.outside_will_run_task_or_must_reenqueue();
}

bool JobTaskSourceNew::State::WillReenqueue() {
  // Release paired with Acquire in `ExitWillRunTask()`, see comment there.
  Value prev{
      value_.fetch_or(kQueuedMask | kOutsideWillRunTaskOrMustReenqueueMask,
                      std::memory_order_release)};
  return prev.outside_will_run_task_or_must_reenqueue();
}

JobTaskSourceNew::State::Value JobTaskSourceNew::State::Load() const {
  return {value_.load(std::memory_order_relaxed)};
}

JobTaskSourceNew::JobTaskSourceNew(
    const Location& from_here,
    const TaskTraits& traits,
    RepeatingCallback<void(JobDelegate*)> worker_task,
    MaxConcurrencyCallback max_concurrency_callback,
    PooledTaskRunnerDelegate* delegate)
    : JobTaskSource(traits, nullptr, TaskSourceExecutionMode::kJob),
      max_concurrency_callback_(std::move(max_concurrency_callback)),
      worker_task_(std::move(worker_task)),
      primary_task_(base::BindRepeating(
          [](JobTaskSourceNew* self) {
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
  // Prevent wait on `join_event_` from triggering a ScopedBlockingCall as this
  // would acquire `ThreadGroup::lock_` and cause lock inversion.
  join_event_.declare_only_used_while_idle();
}

JobTaskSourceNew::~JobTaskSourceNew() {
  // Make sure there's no outstanding active run operation left.
  DCHECK_EQ(state_.Load().worker_count(), 0U);
}

ExecutionEnvironment JobTaskSourceNew::GetExecutionEnvironment() {
  return {SequenceToken::Create(), nullptr};
}

void JobTaskSourceNew::WillEnqueue(int sequence_num, TaskAnnotator& annotator) {
  if (task_metadata_.sequence_num != -1) {
    // WillEnqueue() was already called.
    return;
  }
  task_metadata_.sequence_num = sequence_num;
  annotator.WillQueueTask("ThreadPool_PostJob", &task_metadata_);
}

bool JobTaskSourceNew::WillJoin() {
  // Increment worker count to indicate that this thread participates.
  State::Value state_before_add;
  {
    CheckedAutoLock auto_lock(state_.increment_worker_count_lock());
    state_before_add = state_.IncrementWorkerCount();
  }

  // Return when the job is canceled or the (newly incremented) worker count is
  // below or equal to max concurrency.
  if (!state_before_add.canceled() &&
      state_before_add.worker_count() <
          GetMaxConcurrency(state_before_add.worker_count())) {
    return true;
  }
  return WaitForParticipationOpportunity();
}

bool JobTaskSourceNew::RunJoinTask() {
  {
    TRACE_EVENT0("base", "Job.JoinParticipates");
    JobDelegate job_delegate{this, nullptr};
    worker_task_.Run(&job_delegate);
  }

  const auto state = state_.Load();
  // The condition is slightly different from the one in WillJoin() since we're
  // using |state| that was already incremented to include the joining thread.
  if (!state.canceled() &&
      state.worker_count() <= GetMaxConcurrency(state.worker_count() - 1)) {
    return true;
  }

  return WaitForParticipationOpportunity();
}

void JobTaskSourceNew::Cancel(TaskSource::Transaction* transaction) {
  // Sets the kCanceledMask bit on |state_| so that further calls to
  // WillRunTask() never succeed. std::memory_order_relaxed without a lock is
  // safe because this task source never needs to be re-enqueued after Cancel().
  state_.Cancel();
}

bool JobTaskSourceNew::WaitForParticipationOpportunity() {
  TRACE_EVENT0("base", "Job.WaitForParticipationOpportunity");

  // Wait until either:
  //  A) `worker_count` <= "max concurrency" and state is not canceled.
  //  B) All other workers returned and `worker_count` is 1.
  for (;;) {
    auto state = state_.RequestSignalJoin();

    size_t max_concurrency = GetMaxConcurrency(state.worker_count() - 1);

    // Case A:
    if (state.worker_count() <= max_concurrency && !state.canceled()) {
      state_.FetchAndResetRequestSignalJoin();
      return true;
    }

    // Case B:
    // Only the joining thread remains.
    if (state.worker_count() == 1U) {
      DCHECK(state.canceled() || max_concurrency == 0U);
      // WillRunTask() can run concurrently with this. Synchronize with it via a
      // lock to guarantee that the ordering is one of these 2 options:
      // 1. WillRunTask is first. It increments worker count. The condition
      //    below detects that worker count is no longer 1 and we loop again.
      // 2. This runs first. It cancels the job. WillRunTask returns
      //    RunStatus::kDisallowed and doesn't increment the worker count.
      // We definitely don't want this 3rd option (made impossible by the lock):
      // 3. WillRunTask() observes that the job is not canceled. This observes
      //    that the worker count is 1 and returns. JobHandle::Join returns and
      //    its owner deletes state needed by the worker task. WillRunTask()
      //    increments the worker count and the worker task stats running -->
      //    use-after-free.
      CheckedAutoLock auto_lock(state_.increment_worker_count_lock());

      if (state_.Load().worker_count() != 1U) {
        continue;
      }

      state_.Cancel();
      state_.FetchAndResetRequestSignalJoin();
      state_.DecrementWorkerCount();
      return false;
    }

    join_event_.Wait();
  }
}

TaskSource::RunStatus JobTaskSourceNew::WillRunTask() {
  // The lock below prevents a race described in Case B of
  // `WaitForParticipationOpportunity()`.
  CheckedAutoLock auto_lock(state_.increment_worker_count_lock());

  for (;;) {
    auto prev_state = state_.EnterWillRunTask();

    // Don't allow this worker to run the task if either:
    //   A) Job was cancelled.
    //   B) `worker_count` is already at `max_concurrency`.
    //   C) `max_concurrency` was lowered below or to `worker_count`.

    // Case A:
    if (prev_state.canceled()) {
      state_.ExitWillRunTask(/* saturated=*/true);
      return RunStatus::kDisallowed;
    }

    const size_t worker_count_before_increment = prev_state.worker_count();
    const size_t max_concurrency =
        GetMaxConcurrency(worker_count_before_increment);

    if (worker_count_before_increment < max_concurrency) {
      prev_state = state_.IncrementWorkerCount();
      // Worker count may have been decremented since it was read, but not
      // incremented, due to the lock.
      CHECK_LE(prev_state.worker_count(), worker_count_before_increment);
      bool saturated = max_concurrency == (worker_count_before_increment + 1);
      bool concurrency_increased_during_will_run_task =
          state_.ExitWillRunTask(saturated);

      if (saturated && !concurrency_increased_during_will_run_task) {
        return RunStatus::kAllowedSaturated;
      }

      return RunStatus::kAllowedNotSaturated;
    }

    // Case B or C:
    bool concurrency_increased_during_will_run_task =
        state_.ExitWillRunTask(/* saturated=*/true);
    if (!concurrency_increased_during_will_run_task) {
      return RunStatus::kDisallowed;
    }

    // If concurrency increased during `WillRunTask()`, loop again to
    // re-evaluate the `RunStatus`.
  }
}

size_t JobTaskSourceNew::GetRemainingConcurrency() const {
  // It is safe to read |state_| without a lock since this variable is atomic,
  // and no other state is synchronized with GetRemainingConcurrency().
  const auto state = state_.Load();
  if (state.canceled()) {
    return 0;
  }
  const size_t max_concurrency = GetMaxConcurrency(state.worker_count());
  // Avoid underflows.
  if (state.worker_count() > max_concurrency)
    return 0;
  return max_concurrency - state.worker_count();
}

bool JobTaskSourceNew::IsActive() const {
  auto state = state_.Load();
  return GetMaxConcurrency(state.worker_count()) != 0 ||
         state.worker_count() != 0;
}

size_t JobTaskSourceNew::GetWorkerCount() const {
  return state_.Load().worker_count();
}

bool JobTaskSourceNew::NotifyConcurrencyIncrease() {
  const auto state = state_.Load();

  // No need to signal the joining thread of re-enqueue if canceled.
  if (state.canceled()) {
    return true;
  }

  const auto worker_count = state.worker_count();
  const auto max_concurrency = GetMaxConcurrency(worker_count);

  // Signal the joining thread if there is a request to do so and there is room
  // for the joining thread to participate.
  if (worker_count <= max_concurrency &&
      state_.FetchAndResetRequestSignalJoin()) {
    join_event_.Signal();
  }

  // The job should be queued if the max concurrency isn't reached and it's not
  // already queued.
  if (worker_count < max_concurrency &&
      state_.ShouldQueueUponCapacityIncrease()) {
    return delegate_->EnqueueJobTaskSource(this);
  }

  return true;
}

size_t JobTaskSourceNew::GetMaxConcurrency() const {
  return GetMaxConcurrency(state_.Load().worker_count());
}

size_t JobTaskSourceNew::GetMaxConcurrency(size_t worker_count) const {
  return std::min(max_concurrency_callback_.Run(worker_count),
                  kMaxWorkersPerJob);
}

uint8_t JobTaskSourceNew::AcquireTaskId() {
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

void JobTaskSourceNew::ReleaseTaskId(uint8_t task_id) {
  // memory_order_release to match AcquireTaskId().
  uint32_t previous_task_ids = assigned_task_ids_.fetch_and(
      ~(uint32_t(1) << task_id), std::memory_order_release);
  DCHECK(previous_task_ids & (uint32_t(1) << task_id));
}

bool JobTaskSourceNew::ShouldYield() {
  // It's safe to read `state_` without a lock because it's atomic, keeping in
  // mind that threads may not immediately see the new value when it's updated.
  return state_.Load().canceled();
}

PooledTaskRunnerDelegate* JobTaskSourceNew::GetDelegate() const {
  return delegate_;
}

Task JobTaskSourceNew::TakeTask(TaskSource::Transaction* transaction) {
  // JobTaskSource members are not lock-protected so no need to acquire a lock
  // if |transaction| is nullptr.
  DCHECK_GT(state_.Load().worker_count(), 0U);
  DCHECK(primary_task_);
  return {task_metadata_, primary_task_};
}

bool JobTaskSourceNew::DidProcessTask(
    TaskSource::Transaction* /*transaction*/) {
  auto state = state_.Load();
  size_t worker_count_excluding_this = state.worker_count() - 1;

  // Invoke the max concurrency callback before decrementing the worker count,
  // because as soon as the worker count is decremented, JobHandle::Join() can
  // return and state needed the callback may be deleted. Also, as an
  // optimization, avoid invoking the callback if the job is canceled.
  size_t max_concurrency =
      state.canceled() ? 0U : GetMaxConcurrency(worker_count_excluding_this);

  state = state_.DecrementWorkerCount();
  if (state.signal_join() && state_.FetchAndResetRequestSignalJoin()) {
    join_event_.Signal();
  }

  // A canceled task source should not be re-enqueued.
  if (state.canceled()) {
    return false;
  }

  // Re-enqueue if there isn't enough concurrency.
  if (worker_count_excluding_this < max_concurrency) {
    return state_.WillReenqueue();
  }

  return false;
}

// This is a no-op and should always return true.
bool JobTaskSourceNew::WillReEnqueue(TimeTicks now,
                                     TaskSource::Transaction* /*transaction*/) {
  return true;
}

// This is a no-op.
bool JobTaskSourceNew::OnBecomeReady() {
  return false;
}

TaskSourceSortKey JobTaskSourceNew::GetSortKey() const {
  return TaskSourceSortKey(priority_racy(), ready_time_,
                           state_.Load().worker_count());
}

// This function isn't expected to be called since a job is never delayed.
// However, the class still needs to provide an override.
TimeTicks JobTaskSourceNew::GetDelayedSortKey() const {
  return TimeTicks();
}

// This function isn't expected to be called since a job is never delayed.
// However, the class still needs to provide an override.
bool JobTaskSourceNew::HasReadyTasks(TimeTicks now) const {
  NOTREACHED();
  return true;
}

absl::optional<Task> JobTaskSourceNew::Clear(
    TaskSource::Transaction* transaction) {
  Cancel();

  // Nothing is cleared since other workers might still racily run tasks. For
  // simplicity, the destructor will take care of it once all references are
  // released.
  return absl::nullopt;
}

}  // namespace base::internal
