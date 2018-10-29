// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/platform_native_worker_pool_win.h"

#include "base/task/task_scheduler/task_tracker.h"

namespace base {
namespace internal {

PlatformNativeWorkerPoolWin::PlatformNativeWorkerPoolWin(
    TrackedRef<TaskTracker> task_tracker,
    TrackedRef<Delegate> delegate)
    : SchedulerWorkerPool(std::move(task_tracker),
                          std::move(delegate)) {}

PlatformNativeWorkerPoolWin::~PlatformNativeWorkerPoolWin() {
#if DCHECK_IS_ON()
  // Verify join_for_testing has been called to ensure that there is no more
  // outstanding work. Otherwise, work may try to de-reference an invalid
  // pointer to this class.
  DCHECK(join_for_testing_returned_.IsSet());
#endif
  ::DestroyThreadpoolEnvironment(&environment_);
  ::CloseThreadpoolWork(work_);
  ::CloseThreadpool(pool_);
}

void PlatformNativeWorkerPoolWin::Start() {
  ::InitializeThreadpoolEnvironment(&environment_);

  pool_ = ::CreateThreadpool(nullptr);
  DCHECK(pool_) << "LastError: " << ::GetLastError();
  ::SetThreadpoolThreadMinimum(pool_, 1);
  ::SetThreadpoolThreadMaximum(pool_, 256);

  work_ = ::CreateThreadpoolWork(&RunNextSequence, this, &environment_);
  DCHECK(work_) << "LastError: " << GetLastError();
  ::SetThreadpoolCallbackPool(&environment_, pool_);

  size_t local_num_sequences_before_start;
  {
    auto transaction(priority_queue_.BeginTransaction());
    DCHECK(!started_);
    started_ = true;
    local_num_sequences_before_start = transaction->Size();
  }

  // Schedule sequences added to |priority_queue_| before Start().
  for (size_t i = 0; i < local_num_sequences_before_start; ++i)
    ::SubmitThreadpoolWork(work_);
}

void PlatformNativeWorkerPoolWin::JoinForTesting() {
  ::WaitForThreadpoolWorkCallbacks(work_, true);
#if DCHECK_IS_ON()
  DCHECK(!join_for_testing_returned_.IsSet());
  join_for_testing_returned_.Set();
#endif
}

void PlatformNativeWorkerPoolWin::ReEnqueueSequence(
    scoped_refptr<Sequence> sequence) {
  OnCanScheduleSequence(std::move(sequence));
}

// static
void CALLBACK PlatformNativeWorkerPoolWin::RunNextSequence(
    PTP_CALLBACK_INSTANCE,
    void* scheduler_worker_pool_windows_impl,
    PTP_WORK) {
  auto* worker_pool = static_cast<PlatformNativeWorkerPoolWin*>(
      scheduler_worker_pool_windows_impl);

  worker_pool->BindToCurrentThread();

  scoped_refptr<Sequence> sequence = worker_pool->GetWork();
  DCHECK(sequence);

  sequence = worker_pool->task_tracker_->RunAndPopNextTask(
      std::move(sequence.get()), worker_pool);

  // Re-enqueue sequence and then submit another task to the Windows thread
  // pool.
  if (sequence)
    worker_pool->OnCanScheduleSequence(std::move(sequence));

  worker_pool->UnbindFromCurrentThread();
}

scoped_refptr<Sequence> PlatformNativeWorkerPoolWin::GetWork() {
  auto transaction(priority_queue_.BeginTransaction());

  // The PQ should never be empty here as there's a 1:1 correspondence between
  // a call to ScheduleSequence()/SubmitThreadpoolWork() and GetWork().
  DCHECK(!transaction->IsEmpty());
  return transaction->PopSequence();
}

void PlatformNativeWorkerPoolWin::OnCanScheduleSequence(
    scoped_refptr<Sequence> sequence) {
  const SequenceSortKey sequence_sort_key = sequence->GetSortKey();
  auto transaction(priority_queue_.BeginTransaction());

  transaction->Push(std::move(sequence), sequence_sort_key);
  if (started_) {
    // TODO(fdoray): Handle priorities by having different work objects and
    // using ::SetThreadpoolCallbackPriority() and
    // ::SetThreadpoolCallbackRunsLong().
    ::SubmitThreadpoolWork(work_);
  }
}

}  // namespace internal
}  // namespace base
