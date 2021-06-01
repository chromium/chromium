// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump.h"
#include "base/threading/hang_watcher.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if defined(OS_IOS)
#include "base/message_loop/message_pump_mac.h"
#elif defined(OS_ANDROID)
#include "base/message_loop/message_pump_android.h"
#endif

namespace base {
namespace sequence_manager {
namespace internal {
namespace {

// Returns |next_run_time| capped at 1 day from |lazy_now|. This is used to
// mitigate https://crbug.com/850450 where some platforms are unhappy with
// delays > 100,000,000 seconds. In practice, a diagnosis metric showed that no
// sleep > 1 hour ever completes (always interrupted by an earlier MessageLoop
// event) and 99% of completed sleeps are the ones scheduled for <= 1 second.
// Details @ https://crrev.com/c/1142589.
TimeTicks CapAtOneDay(TimeTicks next_run_time, LazyNow* lazy_now) {
  return std::min(next_run_time, lazy_now->Now() + TimeDelta::FromDays(1));
}

}  // namespace

ThreadControllerWithMessagePumpImpl::ThreadControllerWithMessagePumpImpl(
    const SequenceManager::Settings& settings)
    : associated_thread_(AssociatedThreadId::CreateUnbound()),
      work_deduplicator_(associated_thread_),
      time_source_(settings.clock) {
}

ThreadControllerWithMessagePumpImpl::ThreadControllerWithMessagePumpImpl(
    std::unique_ptr<MessagePump> message_pump,
    const SequenceManager::Settings& settings)
    : ThreadControllerWithMessagePumpImpl(settings) {
  BindToCurrentThread(std::move(message_pump));
}

ThreadControllerWithMessagePumpImpl::~ThreadControllerWithMessagePumpImpl() {
  // Destructors of MessagePump::Delegate and ThreadTaskRunnerHandle
  // will do all the clean-up.
  // ScopedSetSequenceLocalStorageMapForCurrentThread destructor will
  // de-register the current thread as a sequence.

#if defined(OS_WIN)
  if (main_thread_only().in_high_res_mode) {
    main_thread_only().in_high_res_mode = false;
    Time::ActivateHighResolutionTimer(false);
  }
#endif
}

// static
std::unique_ptr<ThreadControllerWithMessagePumpImpl>
ThreadControllerWithMessagePumpImpl::CreateUnbound(
    const SequenceManager::Settings& settings) {
  return base::WrapUnique(new ThreadControllerWithMessagePumpImpl(settings));
}

ThreadControllerWithMessagePumpImpl::MainThreadOnly::MainThreadOnly() = default;

ThreadControllerWithMessagePumpImpl::MainThreadOnly::~MainThreadOnly() =
    default;

void ThreadControllerWithMessagePumpImpl::SetSequencedTaskSource(
    SequencedTaskSource* task_source) {
  DCHECK(task_source);
  DCHECK(!main_thread_only().task_source);
  main_thread_only().task_source = task_source;
}

void ThreadControllerWithMessagePumpImpl::BindToCurrentThread(
    std::unique_ptr<MessagePump> message_pump) {
  associated_thread_->BindToCurrentThread();
  pump_ = std::move(message_pump);
  work_id_provider_ = WorkIdProvider::GetForCurrentThread();
  RunLoop::RegisterDelegateForCurrentThread(this);
  scoped_set_sequence_local_storage_map_for_current_thread_ = std::make_unique<
      base::internal::ScopedSetSequenceLocalStorageMapForCurrentThread>(
      &sequence_local_storage_map_);
  {
    base::internal::CheckedAutoLock task_runner_lock(task_runner_lock_);
    if (task_runner_)
      InitializeThreadTaskRunnerHandle();
  }
  if (work_deduplicator_.BindToCurrentThread() ==
      ShouldScheduleWork::kScheduleImmediate) {
    pump_->ScheduleWork();
  }
}

void ThreadControllerWithMessagePumpImpl::SetWorkBatchSize(
    int work_batch_size) {
  DCHECK_GE(work_batch_size, 1);
  main_thread_only().work_batch_size = work_batch_size;
}

void ThreadControllerWithMessagePumpImpl::SetTimerSlack(
    TimerSlack timer_slack) {
  DCHECK(RunsTasksInCurrentSequence());
  pump_->SetTimerSlack(timer_slack);
}

void ThreadControllerWithMessagePumpImpl::WillQueueTask(
    PendingTask* pending_task,
    const char* task_queue_name) {
  task_annotator_.WillQueueTask("SequenceManager PostTask", pending_task,
                                task_queue_name);
}

void ThreadControllerWithMessagePumpImpl::ScheduleWork() {
  base::internal::CheckedLock::AssertNoLockHeldOnCurrentThread();
  if (work_deduplicator_.OnWorkRequested() ==
      ShouldScheduleWork::kScheduleImmediate) {
    pump_->ScheduleWork();
  }
}

void ThreadControllerWithMessagePumpImpl::SetNextDelayedDoWork(
    LazyNow* lazy_now,
    TimeTicks run_time) {
  DCHECK_LT(lazy_now->Now(), run_time);

  if (main_thread_only().next_delayed_do_work == run_time)
    return;

  // Cap at one day but remember the exact time for the above equality check on
  // the next round.
  main_thread_only().next_delayed_do_work = run_time;
  run_time = CapAtOneDay(run_time, lazy_now);

  // It's very rare for PostDelayedTask to be called outside of a DoWork in
  // production, so most of the time this does nothing.
  if (work_deduplicator_.OnDelayedWorkRequested() ==
      ShouldScheduleWork::kScheduleImmediate) {
    // |pump_| can't be null as all postTasks are cross-thread before binding,
    // and delayed cross-thread postTasks do the thread hop through an immediate
    // task.
    pump_->ScheduleDelayedWork(run_time);
  }
}

const TickClock* ThreadControllerWithMessagePumpImpl::GetClock() {
  return time_source_;
}

bool ThreadControllerWithMessagePumpImpl::RunsTasksInCurrentSequence() {
  return associated_thread_->IsBoundToCurrentThread();
}

void ThreadControllerWithMessagePumpImpl::SetDefaultTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  base::internal::CheckedAutoLock lock(task_runner_lock_);
  task_runner_ = task_runner;
  if (associated_thread_->IsBound()) {
    DCHECK(associated_thread_->IsBoundToCurrentThread());
    // Thread task runner handle will be created in BindToCurrentThread().
    InitializeThreadTaskRunnerHandle();
  }
}

void ThreadControllerWithMessagePumpImpl::InitializeThreadTaskRunnerHandle() {
  // Only one ThreadTaskRunnerHandle can exist at any time,
  // so reset the old one.
  main_thread_only().thread_task_runner_handle.reset();
  main_thread_only().thread_task_runner_handle =
      std::make_unique<ThreadTaskRunnerHandle>(task_runner_);
  // When the task runner is known, bind the power manager. Power notifications
  // are received through that sequence.
  power_monitor_.BindToCurrentThread();
}

void ThreadControllerWithMessagePumpImpl::MaybeStartWatchHangsInScope() {
  if (base::HangWatcher::IsEnabled()) {
    // If run_level_tracker.num_run_level() == 1 this starts the first scope. If
    // it's greater than 1 then this cancels the existing scope and starts a new
    // one. This behavior is desired since #task-in-task-implies-nested (see
    // RunLevelTracker class comments). In a nested loop it's desirable to
    // cancel the hang watching that applies to the outer loop since the
    // expectations that were setup with regards to its expected runtime do not
    // apply anymore.
    hang_watch_scope_.emplace(base::WatchHangsInScope::kDefaultHangWatchTime);
  }
}

scoped_refptr<SingleThreadTaskRunner>
ThreadControllerWithMessagePumpImpl::GetDefaultTaskRunner() {
  base::internal::CheckedAutoLock lock(task_runner_lock_);
  return task_runner_;
}

void ThreadControllerWithMessagePumpImpl::RestoreDefaultTaskRunner() {
  // There's no default task runner unlike with the MessageLoop.
  main_thread_only().thread_task_runner_handle.reset();
}

void ThreadControllerWithMessagePumpImpl::AddNestingObserver(
    RunLoop::NestingObserver* observer) {
  DCHECK(!main_thread_only().nesting_observer);
  DCHECK(observer);
  main_thread_only().nesting_observer = observer;
  RunLoop::AddNestingObserverOnCurrentThread(this);
}

void ThreadControllerWithMessagePumpImpl::RemoveNestingObserver(
    RunLoop::NestingObserver* observer) {
  DCHECK_EQ(main_thread_only().nesting_observer, observer);
  main_thread_only().nesting_observer = nullptr;
  RunLoop::RemoveNestingObserverOnCurrentThread(this);
}

const scoped_refptr<AssociatedThreadId>&
ThreadControllerWithMessagePumpImpl::GetAssociatedThread() const {
  return associated_thread_;
}

void ThreadControllerWithMessagePumpImpl::OnBeginWorkItem() {
  MaybeStartWatchHangsInScope();
  work_id_provider_->IncrementWorkId();
  main_thread_only().run_level_tracker.OnTaskStarted();
}

void ThreadControllerWithMessagePumpImpl::OnEndWorkItem() {
  // Work completed, stop hang watching this specific work item.
  hang_watch_scope_.reset();
  work_id_provider_->IncrementWorkId();
  main_thread_only().run_level_tracker.OnTaskEnded();
}

void ThreadControllerWithMessagePumpImpl::BeforeWait() {
  work_id_provider_->IncrementWorkId();
  // The loop is going to sleep, stop watching for hangs.
  hang_watch_scope_.reset();
  main_thread_only().run_level_tracker.OnIdle();
}

MessagePump::Delegate::NextWorkInfo
ThreadControllerWithMessagePumpImpl::DoWork() {
  MessagePump::Delegate::NextWorkInfo next_work_info{};

  work_deduplicator_.OnWorkStarted();
  LazyNow continuation_lazy_now(time_source_);
  TimeDelta delay_till_next_task = DoWorkImpl(&continuation_lazy_now);

  // If we are yielding after DoWorkImpl (a work batch) set the flag boolean.
  // This will inform the MessagePump to schedule a new continuation based on
  // the information below, but even if its immediate let the native sequence
  // have a chance to run.
  if (!main_thread_only().yield_to_native_after_batch.is_null() &&
      continuation_lazy_now.Now() <
          main_thread_only().yield_to_native_after_batch) {
    next_work_info.yield_to_native = true;
  }
  // Schedule a continuation.
  WorkDeduplicator::NextTask next_task =
      delay_till_next_task.is_zero() ? WorkDeduplicator::NextTask::kIsImmediate
                                     : WorkDeduplicator::NextTask::kIsDelayed;
  if (work_deduplicator_.DidCheckForMoreWork(next_task) ==
      ShouldScheduleWork::kScheduleImmediate) {
    // Need to run new work immediately, but due to the contract of DoWork
    // we only need to return a null TimeTicks to ensure that happens.
    return next_work_info;
  }

  // While the math below would saturate when |delay_till_next_task.is_max()|;
  // special-casing here avoids unnecessarily sampling Now() when out of work.
  if (delay_till_next_task.is_max()) {
    main_thread_only().next_delayed_do_work = TimeTicks::Max();
    next_work_info.delayed_run_time = TimeTicks::Max();
    return next_work_info;
  }

  // The MessagePump will schedule the delay on our behalf, so we need to update
  // |main_thread_only().next_delayed_do_work|.
  // TODO(gab, alexclarke): Replace DelayTillNextTask() with NextTaskTime() to
  // avoid converting back-and-forth between TimeTicks and TimeDelta.
  main_thread_only().next_delayed_do_work =
      continuation_lazy_now.Now() + delay_till_next_task;

  // Don't request a run time past |main_thread_only().quit_runloop_after|.
  if (main_thread_only().next_delayed_do_work >
      main_thread_only().quit_runloop_after) {
    main_thread_only().next_delayed_do_work =
        main_thread_only().quit_runloop_after;
    // If we've passed |quit_runloop_after| there's no more work to do.
    if (continuation_lazy_now.Now() >= main_thread_only().quit_runloop_after) {
      next_work_info.delayed_run_time = TimeTicks::Max();
      return next_work_info;
    }
  }

  next_work_info.delayed_run_time = CapAtOneDay(
      main_thread_only().next_delayed_do_work, &continuation_lazy_now);
  next_work_info.recent_now = continuation_lazy_now.Now();
  return next_work_info;
}

TimeDelta ThreadControllerWithMessagePumpImpl::DoWorkImpl(
    LazyNow* continuation_lazy_now) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
               "ThreadControllerImpl::DoWork");

  if (!main_thread_only().task_execution_allowed) {
    // Broadcast in a trace event that application tasks were disallowed. This
    // helps spot nested loops that intentionally starve application tasks.
    TRACE_EVENT0("base", "ThreadController: application tasks disallowed");
    if (main_thread_only().quit_runloop_after == TimeTicks::Max())
      return TimeDelta::Max();
    return main_thread_only().quit_runloop_after - continuation_lazy_now->Now();
  }

  DCHECK(main_thread_only().task_source);

  for (int i = 0; i < main_thread_only().work_batch_size; i++) {
    // Include SelectNextTask() in the scope of the work item. This ensures it's
    // covered in tracing and hang reports. This is particularly important when
    // SelectNextTask() finds no work immediately after a wakeup, otherwise the
    // power-inefficient wakeup is invisible in tracing.
    auto work_item_scope = BeginWorkItem();

    const SequencedTaskSource::SelectTaskOption select_task_option =
        power_monitor_.IsProcessInPowerSuspendState()
            ? SequencedTaskSource::SelectTaskOption::kSkipDelayedTask
            : SequencedTaskSource::SelectTaskOption::kDefault;
    Task* task =
        main_thread_only().task_source->SelectNextTask(select_task_option);
    if (!task)
      break;

    // Execute the task and assume the worst: it is probably not reentrant.
    AutoReset<bool> ban_nested_application_tasks(
        &main_thread_only().task_execution_allowed, false);

    // Trace-parsing tools (DevTools, Lighthouse, etc) consume this event to
    // determine long tasks.
    // See https://crbug.com/681863 and https://crbug.com/874982
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "RunTask");

    {
      // Trace events should finish before we call DidRunTask to ensure that
      // SequenceManager trace events do not interfere with them.
      TRACE_TASK_EXECUTION("ThreadControllerImpl::RunTask", *task);
      task_annotator_.RunTask("SequenceManager RunTask", task);
    }

    // This processes microtasks and is intentionally included in
    // |work_item_scope|.
    main_thread_only().task_source->DidRunTask();

    // When Quit() is called we must stop running the batch because the caller
    // expects per-task granularity.
    if (main_thread_only().quit_pending)
      break;
  }

  if (main_thread_only().quit_pending)
    return TimeDelta::Max();

  work_deduplicator_.WillCheckForMoreWork();

  // Re-check the state of the power after running tasks. An executed task may
  // have been a power change notification.
  const SequencedTaskSource::SelectTaskOption select_task_option =
      power_monitor_.IsProcessInPowerSuspendState()
          ? SequencedTaskSource::SelectTaskOption::kSkipDelayedTask
          : SequencedTaskSource::SelectTaskOption::kDefault;
  TimeDelta do_work_delay = main_thread_only().task_source->DelayTillNextTask(
      continuation_lazy_now, select_task_option);
  DCHECK_GE(do_work_delay, TimeDelta());
  return do_work_delay;
}

bool ThreadControllerWithMessagePumpImpl::DoIdleWork() {
  TRACE_EVENT0("sequence_manager", "SequenceManager::DoIdleWork");
#if defined(OS_WIN)
  if (!power_monitor_.IsProcessInPowerSuspendState()) {
    // Avoid calling Time::ActivateHighResolutionTimer() between
    // suspend/resume as the system hangs if we do (crbug.com/1074028).
    // OnResume() will generate a task on this thread per the
    // ThreadControllerPowerMonitor observer and DoIdleWork() will thus get
    // another chance to set the right high-resolution-timer-state before
    // going to sleep after resume.

    const bool need_high_res_mode =
        main_thread_only().task_source->HasPendingHighResolutionTasks();
    if (main_thread_only().in_high_res_mode != need_high_res_mode) {
      // On Windows we activate the high resolution timer so that the wait
      // _if_ triggered by the timer happens with good resolution. If we don't
      // do this the default resolution is 15ms which might not be acceptable
      // for some tasks.
      main_thread_only().in_high_res_mode = need_high_res_mode;
      Time::ActivateHighResolutionTimer(need_high_res_mode);
    }
  }
#endif  // defined(OS_WIN)

  {
    auto work_item_scope = BeginWorkItem();
    if (main_thread_only().task_source->OnSystemIdle()) {
      // The OnSystemIdle() callback resulted in more immediate work, so
      // schedule a DoWork callback. For some message pumps returning true from
      // here is sufficient to do that but not on mac.
      pump_->ScheduleWork();
      return false;
    }
  }

  main_thread_only().run_level_tracker.OnIdle();

  // Check if any runloop timeout has expired.
  if (main_thread_only().quit_runloop_after != TimeTicks::Max() &&
      main_thread_only().quit_runloop_after <= time_source_->NowTicks()) {
    Quit();
    return false;
  }

  // RunLoop::Delegate knows whether we called Run() or RunUntilIdle().
  if (ShouldQuitWhenIdle())
    Quit();

  return false;
}

void ThreadControllerWithMessagePumpImpl::Run(bool application_tasks_allowed,
                                              TimeDelta timeout) {
  DCHECK(RunsTasksInCurrentSequence());
  // RunLoops can be nested so we need to restore the previous value of
  // |quit_runloop_after| upon exit. NB we could use saturated arithmetic here
  // but don't because we have some tests which assert the number of calls to
  // Now.
  AutoReset<TimeTicks> quit_runloop_after(
      &main_thread_only().quit_runloop_after,
      (timeout == TimeDelta::Max()) ? TimeTicks::Max()
                                    : time_source_->NowTicks() + timeout);

  main_thread_only().run_level_tracker.OnRunLoopStarted(
      RunLevelTracker::kSelectingNextTask);

  // Quit may have been called outside of a Run(), so |quit_pending| might be
  // true here. We can't use InTopLevelDoWork() in Quit() as this call may be
  // outside top-level DoWork but still in Run().
  main_thread_only().quit_pending = false;
  if (application_tasks_allowed && !main_thread_only().task_execution_allowed) {
    // Allow nested task execution as explicitly requested.
    DCHECK(RunLoop::IsNestedOnCurrentThread());
    main_thread_only().task_execution_allowed = true;
    pump_->Run(this);
    main_thread_only().task_execution_allowed = false;
  } else {
    pump_->Run(this);
  }

  main_thread_only().run_level_tracker.OnRunLoopEnded();
  main_thread_only().quit_pending = false;

  // All work items should be over when exiting the loop so hang watching should
  // not be live.
  DCHECK(!hang_watch_scope_);
}

void ThreadControllerWithMessagePumpImpl::OnBeginNestedRunLoop() {
  // We don't need to ScheduleWork here! That's because the call to pump_->Run()
  // above, which is always called for RunLoop().Run(), guarantees a call to
  // DoWork on all platforms.
  if (main_thread_only().nesting_observer)
    main_thread_only().nesting_observer->OnBeginNestedRunLoop();
}

void ThreadControllerWithMessagePumpImpl::OnExitNestedRunLoop() {
  if (main_thread_only().nesting_observer)
    main_thread_only().nesting_observer->OnExitNestedRunLoop();
}

void ThreadControllerWithMessagePumpImpl::Quit() {
  DCHECK(RunsTasksInCurrentSequence());
  // Interrupt a batch of work.
  main_thread_only().quit_pending = true;

  // If we're in a nested RunLoop, continuation will be posted if necessary.
  pump_->Quit();
}

void ThreadControllerWithMessagePumpImpl::EnsureWorkScheduled() {
  if (work_deduplicator_.OnWorkRequested() ==
      ShouldScheduleWork::kScheduleImmediate) {
    pump_->ScheduleWork();
  }
}

void ThreadControllerWithMessagePumpImpl::SetTaskExecutionAllowed(
    bool allowed) {
  if (allowed) {
    // We need to schedule work unconditionally because we might be about to
    // enter an OS level nested message loop. Unlike a RunLoop().Run() we don't
    // get a call to DoWork on entering for free.
    work_deduplicator_.OnWorkRequested();  // Set the pending DoWork flag.
    pump_->ScheduleWork();
  } else {
    // We've (probably) just left an OS level nested message loop. Make sure a
    // subsequent PostTask within the same Task doesn't ScheduleWork with the
    // pump (this will be done anyway when the task exits).
    work_deduplicator_.OnWorkStarted();
  }
  main_thread_only().task_execution_allowed = allowed;
}

bool ThreadControllerWithMessagePumpImpl::IsTaskExecutionAllowed() const {
  return main_thread_only().task_execution_allowed;
}

MessagePump* ThreadControllerWithMessagePumpImpl::GetBoundMessagePump() const {
  return pump_.get();
}

void ThreadControllerWithMessagePumpImpl::PrioritizeYieldingToNative(
    base::TimeTicks prioritize_until) {
  main_thread_only().yield_to_native_after_batch = prioritize_until;
}

#if defined(OS_IOS)
void ThreadControllerWithMessagePumpImpl::AttachToMessagePump() {
  static_cast<MessagePumpCFRunLoopBase*>(pump_.get())->Attach(this);
}

void ThreadControllerWithMessagePumpImpl::DetachFromMessagePump() {
  static_cast<MessagePumpCFRunLoopBase*>(pump_.get())->Detach();
}
#elif defined(OS_ANDROID)
void ThreadControllerWithMessagePumpImpl::AttachToMessagePump() {
  static_cast<MessagePumpForUI*>(pump_.get())->Attach(this);
}
#endif

bool ThreadControllerWithMessagePumpImpl::ShouldQuitRunLoopWhenIdle() {
  if (main_thread_only().run_level_tracker.num_run_levels() == 0)
    return false;
  // It's only safe to call ShouldQuitWhenIdle() when in a RunLoop.
  return ShouldQuitWhenIdle();
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
