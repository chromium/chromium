// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/timer.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_features.h"
#include "base/threading/platform_thread.h"
#include "base/time/tick_clock.h"

namespace base {
namespace internal {

TimerBase::TimerBase(const Location& posted_from) : posted_from_(posted_from) {
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
  return delayed_task_handle_.IsValid();
}

void TimerBase::SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(!IsRunning());
  task_runner_.swap(task_runner);
}

scoped_refptr<SequencedTaskRunner> TimerBase::GetTaskRunner() {
  return task_runner_ ? task_runner_ : SequencedTaskRunner::GetCurrentDefault();
}

void TimerBase::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AbandonScheduledTask();

  OnStop();
  // No more member accesses here: |this| could be deleted after Stop() call.
}

void TimerBase::AbandonScheduledTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (delayed_task_handle_.IsValid())
    delayed_task_handle_.CancelTask();

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
  Stop();
}

void DelayTimerBase::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EnsureNonNullUserTask();

  // We can't reuse the |scheduled_task_|, so abandon it and post a new one.
  AbandonScheduledTask();
  ScheduleNewTask(delay_);
}

void DelayTimerBase::ScheduleNewTask(TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delayed_task_handle_.IsValid());

  // Ignore negative deltas.
  // TODO(pmonette): Fix callers providing negative deltas and ban passing them.
  if (delay < TimeDelta())
    delay = TimeDelta();

  if (!timer_callback_) {
    timer_callback_ = BindRepeating(&DelayTimerBase::OnScheduledTaskInvoked,
                                    Unretained(this));
  }
  delayed_task_handle_ = GetTaskRunner()->PostCancelableDelayedTask(
      base::subtle::PostDelayedTaskPassKey(), posted_from_, timer_callback_,
      delay);
  desired_run_time_ = Now() + delay;
}

TimeTicks DelayTimerBase::Now() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tick_clock_ ? tick_clock_->NowTicks() : TimeTicks::Now();
}

void DelayTimerBase::OnScheduledTaskInvoked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delayed_task_handle_.IsValid()) << posted_from_.ToString();

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
  CHECK(user_task_);
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
                          subtle::DelayPolicy delay_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AbandonScheduledTask();
  user_task_ = std::move(user_task);
  posted_from_ = posted_from;
  ScheduleNewTask(deadline, delay_policy);
}

void DeadlineTimer::OnStop() {
  user_task_.Reset();
  // No more member accesses here: |this| could be deleted after freeing
  // |user_task_|.
}

void DeadlineTimer::ScheduleNewTask(TimeTicks deadline,
                                    subtle::DelayPolicy delay_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!timer_callback_) {
    timer_callback_ =
        BindRepeating(&DeadlineTimer::OnScheduledTaskInvoked, Unretained(this));
  }
  delayed_task_handle_ = GetTaskRunner()->PostCancelableDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), posted_from_, timer_callback_,
      deadline, delay_policy);
}

void DeadlineTimer::OnScheduledTaskInvoked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delayed_task_handle_.IsValid());

  // Make a local copy of the task to run. The Stop method will reset the
  // |user_task_| member.
  OnceClosure task = std::move(user_task_);
  Stop();
  std::move(task).Run();
  // No more member accesses here: |this| could be deleted at this point.
}

MetronomeTimer::MetronomeTimer() = default;
MetronomeTimer::~MetronomeTimer() = default;

MetronomeTimer::MetronomeTimer(const Location& posted_from,
                               TimeDelta interval,
                               RepeatingClosure user_task,
                               TimeTicks phase)
    : TimerBase(posted_from),
      interval_(interval),
      user_task_(user_task),
      phase_(phase) {}

void MetronomeTimer::Start(const Location& posted_from,
                           TimeDelta interval,
                           RepeatingClosure user_task,
                           TimeTicks phase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_task_ = std::move(user_task);
  posted_from_ = posted_from;
  interval_ = interval;
  phase_ = phase;

  Reset();
}

void MetronomeTimer::OnStop() {
  user_task_.Reset();
  // No more member accesses here: |this| could be deleted after freeing
  // |user_task_|.
}

void MetronomeTimer::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(user_task_);
  // We can't reuse the |scheduled_task_|, so abandon it and post a new one.
  AbandonScheduledTask();
  ScheduleNewTask();
}

void MetronomeTimer::ScheduleNewTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The next wake up is scheduled at the next aligned time which is at least
  // `interval_ / 2` after now. `interval_ / 2` is added to avoid playing
  // "catch-up" if wake ups are late.
  TimeTicks deadline =
      (TimeTicks::Now() + interval_ / 2).SnappedToNextTick(phase_, interval_);

  if (!timer_callback_) {
    timer_callback_ = BindRepeating(&MetronomeTimer::OnScheduledTaskInvoked,
                                    Unretained(this));
  }
  delayed_task_handle_ = GetTaskRunner()->PostCancelableDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), posted_from_, timer_callback_,
      deadline, subtle::DelayPolicy::kPrecise);
}

void MetronomeTimer::OnScheduledTaskInvoked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delayed_task_handle_.IsValid());

  // Make a local copy of the task to run in case the task destroy the timer
  // instance.
  RepeatingClosure task = user_task_;
  ScheduleNewTask();
  std::move(task).Run();
  // No more member accesses here: |this| could be deleted at this point.
}

}  // namespace base
