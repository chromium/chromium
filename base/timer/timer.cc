// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/timer.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/tick_clock.h"

namespace base {
namespace internal {

namespace {

// This feature controls whether or not the scheduled task is always abandoned
// when the timer is stopped or reset. The re-use of the scheduled task is an
// optimization that ensures a timer can not leave multiple canceled tasks in
// the task queue.
constexpr Feature kAlwaysAbandonScheduledTask{"AlwaysAbandonScheduledTask",
                                              FEATURE_DISABLED_BY_DEFAULT};

// Cache of the state of the kAlwaysAbandonScheduledTask feature. This avoids
// the need to constantly query its enabled state through
// FeatureList::IsEnabled().
bool g_is_always_abandon_scheduled_task_enabled =
    kAlwaysAbandonScheduledTask.default_state == FEATURE_ENABLED_BY_DEFAULT;

}  // namespace

// TaskDestructionDetector's role is to detect when the scheduled task is
// deleted without being executed. It can be disabled when the timer no longer
// wants to be notified.
class TaskDestructionDetector {
 public:
  explicit TaskDestructionDetector(TimerBase* timer) : timer_(timer) {}

  TaskDestructionDetector(const TaskDestructionDetector&) = delete;
  TaskDestructionDetector& operator=(const TaskDestructionDetector&) = delete;

  ~TaskDestructionDetector() {
    // If this instance is getting destroyed before it was disabled, notify the
    // timer.
    if (timer_)
      timer_->OnTaskDestroyed();
  }

  // Disables this instance so that the timer is no longer notified in the
  // destructor.
  void Disable() { timer_ = nullptr; }

 private:
  // `timer_` is not a raw_ptr<...> for performance reasons (based on analysis
  // of sampling profiler data and tab_search:top100:2020).
  TimerBase* timer_;
};

// static
void TimerBase::InitializeFeatures() {
  g_is_always_abandon_scheduled_task_enabled =
      FeatureList::IsEnabled(kAlwaysAbandonScheduledTask);
}

TimerBase::TimerBase(const Location& posted_from)
    : posted_from_(posted_from), task_destruction_detector_(nullptr) {
  // It is safe for the timer to be created on a different thread/sequence than
  // the one from which the timer APIs are called. The first call to the
  // checker's CalledOnValidSequence() method will re-bind the checker, and
  // later calls will verify that the same task runner is used.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

TimerBase::~TimerBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AbandonScheduledTask();
}

bool TimerBase::IsRunning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_running_;
}

void TimerBase::SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(!IsRunning());
  task_runner_.swap(task_runner);
}

scoped_refptr<SequencedTaskRunner> TimerBase::GetTaskRunner() {
  return task_runner_ ? task_runner_ : SequencedTaskRunnerHandle::Get();
}

void TimerBase::OnTaskDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(task_destruction_detector_);
  task_destruction_detector_ = nullptr;

  delayed_task_handle_.CancelTask();
  is_running_ = false;

  // It's safe to destroy or restart Timer on another sequence after it has been
  // stopped.
  DETACH_FROM_SEQUENCE(sequence_checker_);

  OnStop();
  // No more member accesses here: |this| could be deleted after OnStop() call.
}

void TimerBase::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_running_ = false;
  AbandonScheduledTask();

  OnStop();
  // No more member accesses here: |this| could be deleted after Stop() call.
}

void TimerBase::AbandonScheduledTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (task_destruction_detector_) {
    task_destruction_detector_->Disable();
    task_destruction_detector_ = nullptr;

    DCHECK(delayed_task_handle_.IsValid());
    delayed_task_handle_.CancelTask();
  }

  // It's safe to destroy or restart Timer on another sequence after the task is
  // abandoned.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DelayTimerBase::DelayTimerBase(const TickClock* tick_clock)
    : tick_clock_(tick_clock) {}

DelayTimerBase::DelayTimerBase(const Location& posted_from,
                               TimeDelta delay,
                               const TickClock* tick_clock)
    : TimerBase(posted_from), delay_(delay), tick_clock_(tick_clock) {}

DelayTimerBase::~DelayTimerBase() = default;

TimeDelta DelayTimerBase::GetCurrentDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delay_;
}

void DelayTimerBase::StartInternal(const Location& posted_from,
                                   TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  posted_from_ = posted_from;
  delay_ = delay;

  Reset();
}

void DelayTimerBase::AbandonAndStop() {
  // Note: Stop() is more or less re-implemented here because it cannot be
  // called without rebinding the |sequence_checker_| to the current sequence
  // after the call to AbandonScheduledTask().
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_running_ = false;

  AbandonScheduledTask();

  OnStop();
  // No more member accesses here: |this| could be deleted at this point.
}

void DelayTimerBase::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EnsureNonNullUserTask();

  if (!g_is_always_abandon_scheduled_task_enabled) {
    // If there's no pending task, start one up and return.
    if (!task_destruction_detector_) {
      ScheduleNewTask(delay_);
      return;
    }

    // Set the new |desired_run_time_|.
    if (delay_ > Microseconds(0))
      desired_run_time_ = Now() + delay_;
    else
      desired_run_time_ = TimeTicks();

    // We can use the existing scheduled task if it arrives before the new
    // |desired_run_time_|.
    if (desired_run_time_ >= scheduled_run_time_) {
      is_running_ = true;
      return;
    }
  }

  // We can't reuse the |scheduled_task_|, so abandon it and post a new one.
  AbandonScheduledTask();
  ScheduleNewTask(delay_);
}

// TODO(1262205): Merge with TimerBase::Stop() once the "always abandon
// scheduled task" feature is launched.
void DelayTimerBase::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_running_ = false;
  if (g_is_always_abandon_scheduled_task_enabled)
    AbandonScheduledTask();

  OnStop();
  // No more member accesses here: |this| could be deleted after Stop() call.
}

void DelayTimerBase::ScheduleNewTask(TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!task_destruction_detector_);
  is_running_ = true;
  auto task_destruction_detector =
      std::make_unique<TaskDestructionDetector>(this);
  task_destruction_detector_ = task_destruction_detector.get();

  // Ignore negative deltas.
  // TODO(pmonette): Fix callers providing negative deltas and ban passing them.
  if (delay < TimeDelta())
    delay = TimeDelta();

  delayed_task_handle_ = GetTaskRunner()->PostCancelableDelayedTask(
      base::subtle::PostDelayedTaskPassKey(), posted_from_,
      BindOnce(&DelayTimerBase::OnScheduledTaskInvoked, Unretained(this),
               std::move(task_destruction_detector)),
      delay);
  scheduled_run_time_ = desired_run_time_ = Now() + delay;
}

TimeTicks DelayTimerBase::Now() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tick_clock_ ? tick_clock_->NowTicks() : TimeTicks::Now();
}

void DelayTimerBase::OnScheduledTaskInvoked(
    std::unique_ptr<TaskDestructionDetector> task_destruction_detector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delayed_task_handle_.IsValid());

  // The scheduled task is currently running so its destruction detector is no
  // longer needed.
  task_destruction_detector->Disable();
  task_destruction_detector_ = nullptr;
  task_destruction_detector.reset();

  // The timer may have been stopped.
  if (!is_running_)
    return;

  // First check if we need to delay the task because of a new target time.
  if (desired_run_time_ > scheduled_run_time_) {
    // Now() can be expensive, so only call it if we know the user has changed
    // the |desired_run_time_|.
    TimeTicks now = Now();
    // Task runner may have called us late anyway, so only post a continuation
    // task if the |desired_run_time_| is in the future.
    if (desired_run_time_ > now) {
      // Post a new task to span the remaining time.
      ScheduleNewTask(desired_run_time_ - now);
      return;
    }
  }

  RunUserTask();
  // No more member accesses here: |this| could be deleted at this point.
}

}  // namespace internal

OneShotTimer::OneShotTimer() = default;
OneShotTimer::OneShotTimer(const TickClock* tick_clock)
    : internal::DelayTimerBase(tick_clock) {}
OneShotTimer::~OneShotTimer() = default;

void OneShotTimer::Start(const Location& posted_from,
                         TimeDelta delay,
                         OnceClosure user_task) {
  user_task_ = std::move(user_task);
  StartInternal(posted_from, delay);
}

void OneShotTimer::FireNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!task_runner_) << "FireNow() is incompatible with SetTaskRunner()";
  DCHECK(IsRunning());

  RunUserTask();
}

void OneShotTimer::OnStop() {
  user_task_.Reset();
  // No more member accesses here: |this| could be deleted after freeing
  // |user_task_|.
}

void OneShotTimer::RunUserTask() {
  // Make a local copy of the task to run. The Stop method will reset the
  // |user_task_| member.
  OnceClosure task = std::move(user_task_);
  Stop();
  DCHECK(task);
  std::move(task).Run();
  // No more member accesses here: |this| could be deleted at this point.
}

void OneShotTimer::EnsureNonNullUserTask() {
  DCHECK(user_task_);
}

RepeatingTimer::RepeatingTimer() = default;
RepeatingTimer::RepeatingTimer(const TickClock* tick_clock)
    : internal::DelayTimerBase(tick_clock) {}
RepeatingTimer::~RepeatingTimer() = default;

RepeatingTimer::RepeatingTimer(const Location& posted_from,
                               TimeDelta delay,
                               RepeatingClosure user_task)
    : internal::DelayTimerBase(posted_from, delay),
      user_task_(std::move(user_task)) {}
RepeatingTimer::RepeatingTimer(const Location& posted_from,
                               TimeDelta delay,
                               RepeatingClosure user_task,
                               const TickClock* tick_clock)
    : internal::DelayTimerBase(posted_from, delay, tick_clock),
      user_task_(std::move(user_task)) {}

void RepeatingTimer::Start(const Location& posted_from,
                           TimeDelta delay,
                           RepeatingClosure user_task) {
  user_task_ = std::move(user_task);
  StartInternal(posted_from, delay);
}

void RepeatingTimer::OnStop() {}
void RepeatingTimer::RunUserTask() {
  // Make a local copy of the task to run in case the task destroy the timer
  // instance.
  RepeatingClosure task = user_task_;
  ScheduleNewTask(GetCurrentDelay());
  task.Run();
  // No more member accesses here: |this| could be deleted at this point.
}

void RepeatingTimer::EnsureNonNullUserTask() {
  DCHECK(user_task_);
}

RetainingOneShotTimer::RetainingOneShotTimer() = default;
RetainingOneShotTimer::RetainingOneShotTimer(const TickClock* tick_clock)
    : internal::DelayTimerBase(tick_clock) {}
RetainingOneShotTimer::~RetainingOneShotTimer() = default;

RetainingOneShotTimer::RetainingOneShotTimer(const Location& posted_from,
                                             TimeDelta delay,
                                             RepeatingClosure user_task)
    : internal::DelayTimerBase(posted_from, delay),
      user_task_(std::move(user_task)) {}
RetainingOneShotTimer::RetainingOneShotTimer(const Location& posted_from,
                                             TimeDelta delay,
                                             RepeatingClosure user_task,
                                             const TickClock* tick_clock)
    : internal::DelayTimerBase(posted_from, delay, tick_clock),
      user_task_(std::move(user_task)) {}

void RetainingOneShotTimer::Start(const Location& posted_from,
                                  TimeDelta delay,
                                  RepeatingClosure user_task) {
  user_task_ = std::move(user_task);
  StartInternal(posted_from, delay);
}

void RetainingOneShotTimer::OnStop() {}
void RetainingOneShotTimer::RunUserTask() {
  // Make a local copy of the task to run in case the task destroys the timer
  // instance.
  RepeatingClosure task = user_task_;
  Stop();
  task.Run();
  // No more member accesses here: |this| could be deleted at this point.
}

void RetainingOneShotTimer::EnsureNonNullUserTask() {
  DCHECK(user_task_);
}

DeadlineTimer::DeadlineTimer() = default;
DeadlineTimer::~DeadlineTimer() = default;

void DeadlineTimer::Start(const Location& posted_from,
                          TimeTicks deadline,
                          OnceClosure user_task,
                          ExactDeadline exact) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_task_ = std::move(user_task);
  posted_from_ = posted_from;
  subtle::DelayPolicy delay_policy =
      exact ? subtle::DelayPolicy::kPrecise
            : subtle::DelayPolicy::kFlexiblePreferEarly;

  ScheduleNewTask(deadline, delay_policy);
}

void DeadlineTimer::OnStop() {
  user_task_.Reset();
  // No more member accesses here: |this| could be deleted after freeing
  // |user_task_|.
}

void DeadlineTimer::RunUserTask() {
  // Make a local copy of the task to run. The Stop method will reset the
  // |user_task_| member.
  OnceClosure task = std::move(user_task_);
  Stop();
  std::move(task).Run();
  // No more member accesses here: |this| could be deleted at this point.
}

void DeadlineTimer::ScheduleNewTask(TimeTicks deadline,
                                    subtle::DelayPolicy delay_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!task_destruction_detector_);
  is_running_ = true;
  auto task_destruction_detector =
      std::make_unique<internal::TaskDestructionDetector>(this);
  task_destruction_detector_ = task_destruction_detector.get();

  delayed_task_handle_ = GetTaskRunner()->PostCancelableDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), posted_from_,
      BindOnce(&DeadlineTimer::OnScheduledTaskInvoked, Unretained(this),
               std::move(task_destruction_detector)),
      deadline, delay_policy);
}

void DeadlineTimer::OnScheduledTaskInvoked(
    std::unique_ptr<internal::TaskDestructionDetector>
        task_destruction_detector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delayed_task_handle_.IsValid());

  // The scheduled task is currently running so its destruction detector is no
  // longer needed.
  task_destruction_detector->Disable();
  task_destruction_detector_ = nullptr;
  task_destruction_detector.reset();

  RunUserTask();
  // No more member accesses here: |this| could be deleted at this point.
}

}  // namespace base
