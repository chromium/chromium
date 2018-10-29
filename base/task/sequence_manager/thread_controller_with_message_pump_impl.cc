// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"

#include "base/auto_reset.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

namespace base {
namespace sequence_manager {
namespace internal {

ThreadControllerWithMessagePumpImpl::ThreadControllerWithMessagePumpImpl(
    std::unique_ptr<MessagePump> message_pump,
    const TickClock* time_source)
    : associated_thread_(AssociatedThreadId::CreateUnbound()),
      pump_(std::move(message_pump)),
      time_source_(time_source) {
  scoped_set_sequence_local_storage_map_for_current_thread_ = std::make_unique<
      base::internal::ScopedSetSequenceLocalStorageMapForCurrentThread>(
      &sequence_local_storage_map_);
  RunLoop::RegisterDelegateForCurrentThread(this);
}

ThreadControllerWithMessagePumpImpl::~ThreadControllerWithMessagePumpImpl() {
  // Destructors of RunLoop::Delegate and ThreadTaskRunnerHandle
  // will do all the clean-up.
  // ScopedSetSequenceLocalStorageMapForCurrentThread destructor will
  // de-register the current thread as a sequence.
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

void ThreadControllerWithMessagePumpImpl::SetMessageLoop(
    MessageLoop* message_loop) {
  NOTREACHED()
      << "ThreadControllerWithMessagePumpImpl doesn't support MessageLoops";
}

void ThreadControllerWithMessagePumpImpl::SetWorkBatchSize(
    int work_batch_size) {
  DCHECK_GE(work_batch_size, 1);
  main_thread_only().work_batch_size = work_batch_size;
}

void ThreadControllerWithMessagePumpImpl::SetTimerSlack(
    TimerSlack timer_slack) {
  pump_->SetTimerSlack(timer_slack);
}

void ThreadControllerWithMessagePumpImpl::WillQueueTask(
    PendingTask* pending_task) {
  task_annotator_.WillQueueTask("ThreadController::Task", pending_task);
}

void ThreadControllerWithMessagePumpImpl::ScheduleWork() {
  // This assumes that cross thread ScheduleWork isn't frequent enough to
  // warrant ScheduleWork deduplication.
  if (RunsTasksInCurrentSequence()) {
    // Don't post a DoWork if there's an immediate DoWork in flight or if we're
    // inside a top level DoWork. We can rely on a continuation being posted as
    // needed.
    if (main_thread_only().immediate_do_work_posted || InTopLevelDoWork())
      return;
    main_thread_only().immediate_do_work_posted = true;
  }
  pump_->ScheduleWork();
}

void ThreadControllerWithMessagePumpImpl::SetNextDelayedDoWork(
    LazyNow* lazy_now,
    TimeTicks run_time) {
  DCHECK_LT(time_source_->NowTicks(), run_time);

  if (main_thread_only().next_delayed_do_work == run_time)
    return;

  // Don't post a DoWork if there's an immediate DoWork in flight or if we're
  // inside a top level DoWork. We can rely on a continuation being posted as
  // needed.
  if (main_thread_only().immediate_do_work_posted || InTopLevelDoWork())
    return;

  main_thread_only().next_delayed_do_work = run_time;
  pump_->ScheduleDelayedWork(run_time);
}

const TickClock* ThreadControllerWithMessagePumpImpl::GetClock() {
  return time_source_;
}

bool ThreadControllerWithMessagePumpImpl::RunsTasksInCurrentSequence() {
  return associated_thread_->thread_id == PlatformThread::CurrentId();
}

void ThreadControllerWithMessagePumpImpl::SetDefaultTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  main_thread_only().thread_task_runner_handle =
      std::make_unique<ThreadTaskRunnerHandle>(task_runner);
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

bool ThreadControllerWithMessagePumpImpl::DoWork() {
  base::TimeTicks next_run_time;
  main_thread_only().immediate_do_work_posted = false;
  return DoWorkImpl(&next_run_time);
}

bool ThreadControllerWithMessagePumpImpl::DoDelayedWork(
    TimeTicks* next_run_time) {
  main_thread_only().next_delayed_do_work = TimeTicks::Max();
  return DoWorkImpl(next_run_time);
}

bool ThreadControllerWithMessagePumpImpl::DoWorkImpl(
    base::TimeTicks* next_run_time) {
  DCHECK(main_thread_only().task_source);
  bool task_ran = false;

  main_thread_only().do_work_running_count++;

  for (int i = 0; i < main_thread_only().work_batch_size; i++) {
    Optional<PendingTask> task = main_thread_only().task_source->TakeTask();
    if (!task)
      break;

    TRACE_TASK_EXECUTION("ThreadController::Task", *task);
    task_annotator_.RunTask("ThreadController::Task", &*task);
    task_ran = true;

    main_thread_only().task_source->DidRunTask();

    // When Quit() is called we must stop running the batch because the caller
    // expects per-task granularity.
    if (main_thread_only().quit_do_work)
      break;
  }

  main_thread_only().do_work_running_count--;

  if (main_thread_only().quit_do_work) {
    main_thread_only().quit_do_work = false;
    return task_ran;
  }

  LazyNow lazy_now(time_source_);
  TimeDelta do_work_delay =
      main_thread_only().task_source->DelayTillNextTask(&lazy_now);
  DCHECK_GE(do_work_delay, TimeDelta());
  // Schedule a continuation.
  // TODO(altimin, gab): Make this more efficient by merging DoWork
  // and DoDelayedWork and allowing returing base::TimeTicks() when we have
  // immediate work.
  if (do_work_delay.is_zero()) {
    // Need to run new work immediately, but due to the contract of DoWork we
    // only need to return true to ensure that happens.
    *next_run_time = lazy_now.Now();
    main_thread_only().immediate_do_work_posted = true;
    return true;
  } else if (do_work_delay != TimeDelta::Max()) {
    *next_run_time = lazy_now.Now() + do_work_delay;
    // Cancels any previously scheduled delayed wake-ups.
    pump_->ScheduleDelayedWork(*next_run_time);
  } else {
    *next_run_time = base::TimeTicks::Max();
  }

  return task_ran;
}

bool ThreadControllerWithMessagePumpImpl::InTopLevelDoWork() const {
  return main_thread_only().do_work_running_count >
         main_thread_only().nesting_depth;
}

bool ThreadControllerWithMessagePumpImpl::DoIdleWork() {
  // RunLoop::Delegate knows whether we called Run() or RunUntilIdle().
  if (ShouldQuitWhenIdle())
    Quit();
#if defined(OS_WIN)
  bool need_high_res_mode =
      main_thread_only().task_source->HasPendingHighResolutionTasks();
  if (main_thread_only().in_high_res_mode != need_high_res_mode) {
    // On Windows we activate the high resolution timer so that the wait
    // _if_ triggered by the timer happens with good resolution. If we don't
    // do this the default resolution is 15ms which might not be acceptable
    // for some tasks.
    main_thread_only().in_high_res_mode = need_high_res_mode;
    Time::ActivateHighResolutionTimer(need_high_res_mode);
  }
#endif  // defined(OS_WIN)
  return false;
}

void ThreadControllerWithMessagePumpImpl::Run(bool application_tasks_allowed) {
  // No system messages are being processed by this class.
  DCHECK(application_tasks_allowed);

  // MessagePump::Run() blocks until Quit() called, but previously started
  // Run() calls continue to block.
  pump_->Run(this);
}

void ThreadControllerWithMessagePumpImpl::OnBeginNestedRunLoop() {
  main_thread_only().nesting_depth++;
  if (main_thread_only().nesting_observer)
    main_thread_only().nesting_observer->OnBeginNestedRunLoop();
}

void ThreadControllerWithMessagePumpImpl::OnExitNestedRunLoop() {
  main_thread_only().nesting_depth--;
  DCHECK_GE(main_thread_only().nesting_depth, 0);
  if (main_thread_only().nesting_observer)
    main_thread_only().nesting_observer->OnExitNestedRunLoop();
}

void ThreadControllerWithMessagePumpImpl::Quit() {
  // Interrupt a batch of work.
  if (InTopLevelDoWork())
    main_thread_only().quit_do_work = true;
  // If we're in a nested RunLoop, continuation will be posted if necessary.
  pump_->Quit();
}

void ThreadControllerWithMessagePumpImpl::EnsureWorkScheduled() {
  main_thread_only().immediate_do_work_posted = true;
  ScheduleWork();
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
