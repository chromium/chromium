// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/scheduler_single_thread_task_runner_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/task_scheduler/delayed_task_manager.h"
#include "base/task/task_scheduler/priority_queue.h"
#include "base/task/task_scheduler/scheduler_worker.h"
#include "base/task/task_scheduler/sequence.h"
#include "base/task/task_scheduler/task.h"
#include "base/task/task_scheduler/task_tracker.h"
#include "base/task/task_traits.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/win/scoped_com_initializer.h"
#endif  // defined(OS_WIN)

namespace base {
namespace internal {

namespace {

// Boolean indicating whether there's a SchedulerSingleThreadTaskRunnerManager
// instance alive in this process. This variable should only be set when the
// SchedulerSingleThreadTaskRunnerManager instance is brought up (on the main
// thread; before any tasks are posted) and decremented when the instance is
// brought down (i.e., only when unit tests tear down the task environment and
// never in production). This makes the variable const while worker threads are
// up and as such it doesn't need to be atomic. It is used to tell when a task
// is posted from the main thread after the task environment was brought down in
// unit tests so that SchedulerSingleThreadTaskRunnerManager bound TaskRunners
// can return false on PostTask, letting such callers know they should complete
// necessary work synchronously. Note: |!g_manager_is_alive| is generally
// equivalent to |!TaskScheduler::GetInstance()| but has the advantage of being
// valid in task_scheduler unit tests that don't instantiate a full
// TaskScheduler.
bool g_manager_is_alive = false;

// Allows for checking the PlatformThread::CurrentRef() against a set
// PlatformThreadRef atomically without using locks.
class AtomicThreadRefChecker {
 public:
  AtomicThreadRefChecker() = default;
  ~AtomicThreadRefChecker() = default;

  void Set() {
    thread_ref_ = PlatformThread::CurrentRef();
    is_set_.Set();
  }

  bool IsCurrentThreadSameAsSetThread() {
    return is_set_.IsSet() && thread_ref_ == PlatformThread::CurrentRef();
  }

 private:
  AtomicFlag is_set_;
  PlatformThreadRef thread_ref_;

  DISALLOW_COPY_AND_ASSIGN(AtomicThreadRefChecker);
};

class SchedulerWorkerDelegate : public SchedulerWorker::Delegate {
 public:
  SchedulerWorkerDelegate(const std::string& thread_name,
                          SchedulerWorker::ThreadLabel thread_label)
      : thread_name_(thread_name), thread_label_(thread_label) {}

  void set_worker(SchedulerWorker* worker) {
    DCHECK(!worker_);
    worker_ = worker;
  }

  // SchedulerWorker::Delegate:
  void OnCanScheduleSequence(scoped_refptr<Sequence> sequence) override {
    DCHECK(worker_);
    ReEnqueueSequence(std::move(sequence));
    worker_->WakeUp();
  }

  SchedulerWorker::ThreadLabel GetThreadLabel() const final {
    return thread_label_;
  }

  void OnMainEntry(const SchedulerWorker* /* worker */) override {
    thread_ref_checker_.Set();
    PlatformThread::SetName(thread_name_);
  }

  scoped_refptr<Sequence> GetWork(SchedulerWorker* worker) override {
    std::unique_ptr<PriorityQueue::Transaction> transaction(
        priority_queue_.BeginTransaction());
    return transaction->IsEmpty() ? nullptr : transaction->PopSequence();
  }

  void DidRunTask() override {}

  void ReEnqueueSequence(scoped_refptr<Sequence> sequence) override {
    DCHECK(sequence);
    const SequenceSortKey sequence_sort_key = sequence->GetSortKey();
    std::unique_ptr<PriorityQueue::Transaction> transaction(
        priority_queue_.BeginTransaction());
    transaction->Push(std::move(sequence), sequence_sort_key);
  }

  TimeDelta GetSleepTimeout() override { return TimeDelta::Max(); }

  bool RunsTasksInCurrentSequence() {
    // We check the thread ref instead of the sequence for the benefit of COM
    // callbacks which may execute without a sequence context.
    return thread_ref_checker_.IsCurrentThreadSameAsSetThread();
  }

  void OnMainExit(SchedulerWorker* /* worker */) override {}

 private:
  const std::string thread_name_;
  const SchedulerWorker::ThreadLabel thread_label_;

  // The SchedulerWorker that has |this| as a delegate. Must be set before
  // starting or posting a task to the SchedulerWorker, because it's used in
  // OnMainEntry() and OnCanScheduleSequence() (called when a sequence held up
  // by WillScheduleSequence() in PostTaskNow() can be scheduled).
  SchedulerWorker* worker_ = nullptr;
  PriorityQueue priority_queue_;

  AtomicThreadRefChecker thread_ref_checker_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerDelegate);
};

#if defined(OS_WIN)

class SchedulerWorkerCOMDelegate : public SchedulerWorkerDelegate {
 public:
  SchedulerWorkerCOMDelegate(const std::string& thread_name,
                             SchedulerWorker::ThreadLabel thread_label,
                             TrackedRef<TaskTracker> task_tracker)
      : SchedulerWorkerDelegate(thread_name, thread_label),
        task_tracker_(std::move(task_tracker)) {}

  ~SchedulerWorkerCOMDelegate() override { DCHECK(!scoped_com_initializer_); }

  // SchedulerWorker::Delegate:
  void OnMainEntry(const SchedulerWorker* worker) override {
    SchedulerWorkerDelegate::OnMainEntry(worker);

    scoped_com_initializer_ = std::make_unique<win::ScopedCOMInitializer>();
  }

  scoped_refptr<Sequence> GetWork(SchedulerWorker* worker) override {
    // This scheme below allows us to cover the following scenarios:
    // * Only SchedulerWorkerDelegate::GetWork() has work:
    //   Always return the sequence from GetWork().
    // * Only the Windows Message Queue has work:
    //   Always return the sequence from GetWorkFromWindowsMessageQueue();
    // * Both SchedulerWorkerDelegate::GetWork() and the Windows Message Queue
    //   have work:
    //   Process sequences from each source round-robin style.
    scoped_refptr<Sequence> sequence;
    if (get_work_first_) {
      sequence = SchedulerWorkerDelegate::GetWork(worker);
      if (sequence)
        get_work_first_ = false;
    }

    if (!sequence) {
      sequence = GetWorkFromWindowsMessageQueue();
      if (sequence)
        get_work_first_ = true;
    }

    if (!sequence && !get_work_first_) {
      // This case is important if we checked the Windows Message Queue first
      // and found there was no work. We don't want to return null immediately
      // as that could cause the thread to go to sleep while work is waiting via
      // SchedulerWorkerDelegate::GetWork().
      sequence = SchedulerWorkerDelegate::GetWork(worker);
    }
    return sequence;
  }

  void OnMainExit(SchedulerWorker* /* worker */) override {
    scoped_com_initializer_.reset();
  }

  void WaitForWork(WaitableEvent* wake_up_event) override {
    DCHECK(wake_up_event);
    const TimeDelta sleep_time = GetSleepTimeout();
    const DWORD milliseconds_wait =
        sleep_time.is_max() ? INFINITE : sleep_time.InMilliseconds();
    const HANDLE wake_up_event_handle = wake_up_event->handle();
    MsgWaitForMultipleObjectsEx(1, &wake_up_event_handle, milliseconds_wait,
                                QS_ALLINPUT, 0);
  }

 private:
  scoped_refptr<Sequence> GetWorkFromWindowsMessageQueue() {
    MSG msg;
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE) {
      Task pump_message_task(FROM_HERE,
                             Bind(
                                 [](MSG msg) {
                                   TranslateMessage(&msg);
                                   DispatchMessage(&msg);
                                 },
                                 std::move(msg)),
                             TimeDelta());
      if (task_tracker_->WillPostTask(&pump_message_task,
                                      TaskShutdownBehavior::SKIP_ON_SHUTDOWN)) {
        bool was_empty =
            message_pump_sequence_->PushTask(std::move(pump_message_task));
        DCHECK(was_empty) << "GetWorkFromWindowsMessageQueue() does not expect "
                             "queueing of pump tasks.";
        return message_pump_sequence_;
      }
    }
    return nullptr;
  }

  bool get_work_first_ = true;
  const scoped_refptr<Sequence> message_pump_sequence_ =
      MakeRefCounted<Sequence>(TaskTraits(MayBlock()));
  const TrackedRef<TaskTracker> task_tracker_;
  std::unique_ptr<win::ScopedCOMInitializer> scoped_com_initializer_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerCOMDelegate);
};

#endif  // defined(OS_WIN)

}  // namespace

class SchedulerSingleThreadTaskRunnerManager::SchedulerSingleThreadTaskRunner
    : public SingleThreadTaskRunner {
 public:
  // Constructs a SchedulerSingleThreadTaskRunner that indirectly controls the
  // lifetime of a dedicated |worker| for |traits|.
  SchedulerSingleThreadTaskRunner(
      SchedulerSingleThreadTaskRunnerManager* const outer,
      const TaskTraits& traits,
      SchedulerWorker* worker,
      SingleThreadTaskRunnerThreadMode thread_mode)
      : outer_(outer),
        worker_(worker),
        thread_mode_(thread_mode),
        sequence_(MakeRefCounted<Sequence>(traits)) {
    DCHECK(outer_);
    DCHECK(worker_);
  }

  // SingleThreadTaskRunner:
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure closure,
                       TimeDelta delay) override {
    if (!g_manager_is_alive)
      return false;

    Task task(from_here, std::move(closure), delay);
    task.single_thread_task_runner_ref = this;

    if (!outer_->task_tracker_->WillPostTask(
            &task, sequence_->traits().shutdown_behavior())) {
      return false;
    }

    if (task.delayed_run_time.is_null()) {
      PostTaskNow(std::move(task));
    } else {
      outer_->delayed_task_manager_->AddDelayedTask(
          std::move(task),
          BindOnce(&SchedulerSingleThreadTaskRunner::PostTaskNow,
                   Unretained(this)));
    }
    return true;
  }

  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure closure,
                                  TimeDelta delay) override {
    // Tasks are never nested within the task scheduler.
    return PostDelayedTask(from_here, std::move(closure), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    if (!g_manager_is_alive)
      return false;
    return GetDelegate()->RunsTasksInCurrentSequence();
  }

 private:
  ~SchedulerSingleThreadTaskRunner() override {
    // Only unregister if this is a DEDICATED SingleThreadTaskRunner. SHARED
    // task runner SchedulerWorkers are managed separately as they are reused.
    // |g_manager_is_alive| avoids a use-after-free should this
    // SchedulerSingleThreadTaskRunner outlive its manager. It is safe to access
    // |g_manager_is_alive| without synchronization primitives as it is const
    // for the lifetime of the manager and ~SchedulerSingleThreadTaskRunner()
    // either happens prior to the end of JoinForTesting() (which happens-before
    // manager's destruction) or on main thread after the task environment's
    // entire destruction (which happens-after the manager's destruction). Yes,
    // there's a theoretical use case where the last ref to this
    // SchedulerSingleThreadTaskRunner is handed to a thread not controlled by
    // task_scheduler and that this ends up causing
    // ~SchedulerSingleThreadTaskRunner() to race with
    // ~SchedulerSingleThreadTaskRunnerManager() but this is intentionally not
    // supported (and it doesn't matter in production where we leak the task
    // environment for such reasons). TSan should catch this weird paradigm
    // should anyone elect to use it in a unit test and the error would point
    // here.
    if (g_manager_is_alive &&
        thread_mode_ == SingleThreadTaskRunnerThreadMode::DEDICATED) {
      outer_->UnregisterSchedulerWorker(worker_);
    }
  }

  void PostTaskNow(Task task) {
    const bool sequence_was_empty = sequence_->PushTask(std::move(task));
    if (sequence_was_empty) {
      if (outer_->task_tracker_->WillScheduleSequence(sequence_,
                                                      GetDelegate())) {
        GetDelegate()->ReEnqueueSequence(sequence_);
        worker_->WakeUp();
      }
    }
  }

  SchedulerWorkerDelegate* GetDelegate() const {
    return static_cast<SchedulerWorkerDelegate*>(worker_->delegate());
  }

  SchedulerSingleThreadTaskRunnerManager* const outer_;
  SchedulerWorker* const worker_;
  const SingleThreadTaskRunnerThreadMode thread_mode_;
  const scoped_refptr<Sequence> sequence_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerSingleThreadTaskRunner);
};

SchedulerSingleThreadTaskRunnerManager::SchedulerSingleThreadTaskRunnerManager(
    TrackedRef<TaskTracker> task_tracker,
    DelayedTaskManager* delayed_task_manager)
    : task_tracker_(std::move(task_tracker)),
      delayed_task_manager_(delayed_task_manager) {
  DCHECK(task_tracker_);
  DCHECK(delayed_task_manager_);
#if defined(OS_WIN)
  static_assert(arraysize(shared_com_scheduler_workers_) ==
                    arraysize(shared_scheduler_workers_),
                "The size of |shared_com_scheduler_workers_| must match "
                "|shared_scheduler_workers_|");
  static_assert(arraysize(shared_com_scheduler_workers_[0]) ==
                    arraysize(shared_scheduler_workers_[0]),
                "The size of |shared_com_scheduler_workers_| must match "
                "|shared_scheduler_workers_|");
#endif  // defined(OS_WIN)
  DCHECK(!g_manager_is_alive);
  g_manager_is_alive = true;
}

SchedulerSingleThreadTaskRunnerManager::
    ~SchedulerSingleThreadTaskRunnerManager() {
  DCHECK(g_manager_is_alive);
  g_manager_is_alive = false;
}

void SchedulerSingleThreadTaskRunnerManager::Start(
    SchedulerWorkerObserver* scheduler_worker_observer) {
  DCHECK(!scheduler_worker_observer_);
  scheduler_worker_observer_ = scheduler_worker_observer;

  decltype(workers_) workers_to_start;
  {
    AutoSchedulerLock auto_lock(lock_);
    started_ = true;
    workers_to_start = workers_;
  }

  // Start workers that were created before this method was called.
  // Workers that already need to wake up are already signaled as part of
  // SchedulerSingleThreadTaskRunner::PostTaskNow(). As a result, it's
  // unnecessary to call WakeUp() for each worker (in fact, an extraneous
  // WakeUp() would be racy and wrong - see https://crbug.com/862582).
  for (scoped_refptr<SchedulerWorker> worker : workers_to_start)
    worker->Start(scheduler_worker_observer_);
}

scoped_refptr<SingleThreadTaskRunner>
SchedulerSingleThreadTaskRunnerManager::CreateSingleThreadTaskRunnerWithTraits(
    const TaskTraits& traits,
    SingleThreadTaskRunnerThreadMode thread_mode) {
  return CreateTaskRunnerWithTraitsImpl<SchedulerWorkerDelegate>(traits,
                                                                 thread_mode);
}

#if defined(OS_WIN)
scoped_refptr<SingleThreadTaskRunner>
SchedulerSingleThreadTaskRunnerManager::CreateCOMSTATaskRunnerWithTraits(
    const TaskTraits& traits,
    SingleThreadTaskRunnerThreadMode thread_mode) {
  return CreateTaskRunnerWithTraitsImpl<SchedulerWorkerCOMDelegate>(
      traits, thread_mode);
}
#endif  // defined(OS_WIN)

// static
SchedulerSingleThreadTaskRunnerManager::ContinueOnShutdown
SchedulerSingleThreadTaskRunnerManager::TraitsToContinueOnShutdown(
    const TaskTraits& traits) {
  if (traits.shutdown_behavior() == TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
    return IS_CONTINUE_ON_SHUTDOWN;
  return IS_NOT_CONTINUE_ON_SHUTDOWN;
}

template <typename DelegateType>
scoped_refptr<
    SchedulerSingleThreadTaskRunnerManager::SchedulerSingleThreadTaskRunner>
SchedulerSingleThreadTaskRunnerManager::CreateTaskRunnerWithTraitsImpl(
    const TaskTraits& traits,
    SingleThreadTaskRunnerThreadMode thread_mode) {
  DCHECK(thread_mode != SingleThreadTaskRunnerThreadMode::SHARED ||
         !traits.with_base_sync_primitives())
      << "Using WithBaseSyncPrimitives() on a shared SingleThreadTaskRunner "
         "may cause deadlocks. Either reevaluate your usage (e.g. use "
         "SequencedTaskRunner) or use "
         "SingleThreadTaskRunnerThreadMode::DEDICATED.";
  // To simplify the code, |dedicated_worker| is a local only variable that
  // allows the code to treat both the DEDICATED and SHARED cases similarly for
  // SingleThreadTaskRunnerThreadMode. In DEDICATED, the scoped_refptr is backed
  // by a local variable and in SHARED, the scoped_refptr is backed by a member
  // variable.
  SchedulerWorker* dedicated_worker = nullptr;
  SchedulerWorker*& worker =
      thread_mode == SingleThreadTaskRunnerThreadMode::DEDICATED
          ? dedicated_worker
          : GetSharedSchedulerWorkerForTraits<DelegateType>(traits);
  bool new_worker = false;
  bool started;
  {
    AutoSchedulerLock auto_lock(lock_);
    if (!worker) {
      const auto& environment_params =
          kEnvironmentParams[GetEnvironmentIndexForTraits(traits)];
      std::string worker_name;
      if (thread_mode == SingleThreadTaskRunnerThreadMode::SHARED)
        worker_name += "Shared";
      worker_name += environment_params.name_suffix;
      worker = CreateAndRegisterSchedulerWorker<DelegateType>(
          worker_name, thread_mode,
          CanUseBackgroundPriorityForSchedulerWorker()
              ? environment_params.priority_hint
              : ThreadPriority::NORMAL);
      new_worker = true;
    }
    started = started_;
  }

  if (new_worker && started)
    worker->Start(scheduler_worker_observer_);

  return MakeRefCounted<SchedulerSingleThreadTaskRunner>(this, traits, worker,
                                                         thread_mode);
}

void SchedulerSingleThreadTaskRunnerManager::JoinForTesting() {
  decltype(workers_) local_workers;
  {
    AutoSchedulerLock auto_lock(lock_);
    local_workers = std::move(workers_);
  }

  for (const auto& worker : local_workers)
    worker->JoinForTesting();

  {
    AutoSchedulerLock auto_lock(lock_);
    DCHECK(workers_.empty())
        << "New worker(s) unexpectedly registered during join.";
    workers_ = std::move(local_workers);
  }

  // Release shared SchedulerWorkers at the end so they get joined above. If
  // this call happens before the joins, the SchedulerWorkers are effectively
  // detached and may outlive the SchedulerSingleThreadTaskRunnerManager.
  ReleaseSharedSchedulerWorkers();
}

template <>
std::unique_ptr<SchedulerWorkerDelegate>
SchedulerSingleThreadTaskRunnerManager::CreateSchedulerWorkerDelegate<
    SchedulerWorkerDelegate>(const std::string& name,
                             int id,
                             SingleThreadTaskRunnerThreadMode thread_mode) {
  return std::make_unique<SchedulerWorkerDelegate>(
      StringPrintf("TaskSchedulerSingleThread%s%d", name.c_str(), id),
      thread_mode == SingleThreadTaskRunnerThreadMode::DEDICATED
          ? SchedulerWorker::ThreadLabel::DEDICATED
          : SchedulerWorker::ThreadLabel::SHARED);
}

#if defined(OS_WIN)
template <>
std::unique_ptr<SchedulerWorkerDelegate>
SchedulerSingleThreadTaskRunnerManager::CreateSchedulerWorkerDelegate<
    SchedulerWorkerCOMDelegate>(const std::string& name,
                                int id,
                                SingleThreadTaskRunnerThreadMode thread_mode) {
  return std::make_unique<SchedulerWorkerCOMDelegate>(
      StringPrintf("TaskSchedulerSingleThreadCOMSTA%s%d", name.c_str(), id),
      thread_mode == SingleThreadTaskRunnerThreadMode::DEDICATED
          ? SchedulerWorker::ThreadLabel::DEDICATED_COM
          : SchedulerWorker::ThreadLabel::SHARED_COM,
      task_tracker_);
}
#endif  // defined(OS_WIN)

template <typename DelegateType>
SchedulerWorker*
SchedulerSingleThreadTaskRunnerManager::CreateAndRegisterSchedulerWorker(
    const std::string& name,
    SingleThreadTaskRunnerThreadMode thread_mode,
    ThreadPriority priority_hint) {
  lock_.AssertAcquired();
  int id = next_worker_id_++;
  std::unique_ptr<SchedulerWorkerDelegate> delegate =
      CreateSchedulerWorkerDelegate<DelegateType>(name, id, thread_mode);
  SchedulerWorkerDelegate* delegate_raw = delegate.get();
  scoped_refptr<SchedulerWorker> worker = MakeRefCounted<SchedulerWorker>(
      priority_hint, std::move(delegate), task_tracker_);
  delegate_raw->set_worker(worker.get());
  workers_.emplace_back(std::move(worker));
  return workers_.back().get();
}

template <>
SchedulerWorker*&
SchedulerSingleThreadTaskRunnerManager::GetSharedSchedulerWorkerForTraits<
    SchedulerWorkerDelegate>(const TaskTraits& traits) {
  return shared_scheduler_workers_[GetEnvironmentIndexForTraits(traits)]
                                  [TraitsToContinueOnShutdown(traits)];
}

#if defined(OS_WIN)
template <>
SchedulerWorker*&
SchedulerSingleThreadTaskRunnerManager::GetSharedSchedulerWorkerForTraits<
    SchedulerWorkerCOMDelegate>(const TaskTraits& traits) {
  return shared_com_scheduler_workers_[GetEnvironmentIndexForTraits(traits)]
                                      [TraitsToContinueOnShutdown(traits)];
}
#endif  // defined(OS_WIN)

void SchedulerSingleThreadTaskRunnerManager::UnregisterSchedulerWorker(
    SchedulerWorker* worker) {
  // Cleanup uses a SchedulerLock, so call Cleanup() after releasing
  // |lock_|.
  scoped_refptr<SchedulerWorker> worker_to_destroy;
  {
    AutoSchedulerLock auto_lock(lock_);

    // Skip when joining (the join logic takes care of the rest).
    if (workers_.empty())
      return;

    auto worker_iter =
        std::find_if(workers_.begin(), workers_.end(),
                     [worker](const scoped_refptr<SchedulerWorker>& candidate) {
                       return candidate.get() == worker;
                     });
    DCHECK(worker_iter != workers_.end());
    worker_to_destroy = std::move(*worker_iter);
    workers_.erase(worker_iter);
  }
  worker_to_destroy->Cleanup();
}

void SchedulerSingleThreadTaskRunnerManager::ReleaseSharedSchedulerWorkers() {
  decltype(shared_scheduler_workers_) local_shared_scheduler_workers;
#if defined(OS_WIN)
  decltype(shared_com_scheduler_workers_) local_shared_com_scheduler_workers;
#endif
  {
    AutoSchedulerLock auto_lock(lock_);
    for (size_t i = 0; i < arraysize(shared_scheduler_workers_); ++i) {
      for (size_t j = 0; j < arraysize(shared_scheduler_workers_[i]); ++j) {
        local_shared_scheduler_workers[i][j] = shared_scheduler_workers_[i][j];
        shared_scheduler_workers_[i][j] = nullptr;
#if defined(OS_WIN)
        local_shared_com_scheduler_workers[i][j] =
            shared_com_scheduler_workers_[i][j];
        shared_com_scheduler_workers_[i][j] = nullptr;
#endif
      }
    }
  }

  for (size_t i = 0; i < arraysize(local_shared_scheduler_workers); ++i) {
    for (size_t j = 0; j < arraysize(local_shared_scheduler_workers[i]); ++j) {
      if (local_shared_scheduler_workers[i][j])
        UnregisterSchedulerWorker(local_shared_scheduler_workers[i][j]);
#if defined(OS_WIN)
      if (local_shared_com_scheduler_workers[i][j])
        UnregisterSchedulerWorker(local_shared_com_scheduler_workers[i][j]);
#endif
    }
  }
}

}  // namespace internal
}  // namespace base
