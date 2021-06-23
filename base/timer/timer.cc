// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/timer.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

// The reason for which the timer's scheduled task was invoked.
enum ScheduledTaskInvokedReason {
  kStopped,      // The timer fired for a stopped timer so nothing was done.
  kRescheduled,  // The timer fired before the desired run time so the user task
                 // was rescheduled for later. This can happens when the timer
                 // is restarted while it is already running.
  kReady,        // The timer fired at the desired run time so the task is ready
                 // to be invoked.
  kMaxValue
};

void RecordScheduledTaskInvokedReason(ScheduledTaskInvokedReason reason) {
  // Recording this histogram breaks a fuchsia test.
#if !defined(OS_FUCHSIA)
  UMA_HISTOGRAM_ENUMERATION("Scheduler.TimerBase.ScheduledTaskInvokedReason",
                            reason);
#endif
}

}  // namespace

// TaskDestructionDetector's role is to detect when the scheduled task is
// deleted without being executed. It can be disabled when the timer no longer
// wants to be notified.
class TaskDestructionDetector {
 public:
  explicit TaskDestructionDetector(TimerBase* timer) : timer_(timer) {}

  ~TaskDestructionDetector() {
    // If this instance is getting destroyed before it was disabled, notify the
    // timer.
    if (timer_)
      timer_->AbandonAndStop();
  }

  // Disables this instance so that the timer is no longer notified in the
  // destructor.
  void Disable() { timer_ = nullptr; }

 private:
  TimerBase* timer_;

  DISALLOW_COPY_AND_ASSIGN(TaskDestructionDetector);
};

TimerBase::TimerBase() : TimerBase(nullptr) {}

TimerBase::TimerBase(const TickClock* tick_clock)
    : task_destruction_detector_(nullptr),
      tick_clock_(tick_clock),
      is_running_(false) {
  // It is safe for the timer to be created on a different thread/sequence than
  // the one from which the timer APIs are called. The first call to the
  // checker's CalledOnValidSequence() method will re-bind the checker, and
  // later calls will verify that the same task runner is used.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

TimerBase::TimerBase(const Location& posted_from, TimeDelta delay)
    : TimerBase(posted_from, delay, nullptr) {}

TimerBase::TimerBase(const Location& posted_from,
                     TimeDelta delay,
                     const TickClock* tick_clock)
    : task_destruction_detector_(nullptr),
      posted_from_(posted_from),
      delay_(delay),
      tick_clock_(tick_clock),
      is_running_(false) {
  // See comment in other constructor.
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

TimeDelta TimerBase::GetCurrentDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delay_;
}

void TimerBase::SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(!IsRunning());
  task_runner_.swap(task_runner);
}

void TimerBase::StartInternal(const Location& posted_from, TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  posted_from_ = posted_from;
  delay_ = delay;

  Reset();
}

void TimerBase::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_running_ = false;

  // It's safe to destroy or restart Timer on another sequence after Stop().
  DETACH_FROM_SEQUENCE(sequence_checker_);

  OnStop();
  // No more member accesses here: |this| could be deleted after Stop() call.
}

void TimerBase::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there's no pending task, start one up and return.
  if (!task_destruction_detector_) {
    ScheduleNewTask(delay_);
    return;
  }

  // Set the new |desired_run_time_|.
  if (delay_ > TimeDelta::FromMicroseconds(0))
    desired_run_time_ = Now() + delay_;
  else
    desired_run_time_ = TimeTicks();

  // We can use the existing scheduled task if it arrives before the new
  // |desired_run_time_|.
  if (desired_run_time_ >= scheduled_run_time_) {
    is_running_ = true;
    return;
  }

  // We can't reuse the |scheduled_task_|, so abandon it and post a new one.
  AbandonScheduledTask();
  ScheduleNewTask(delay_);
}

void TimerBase::ScheduleNewTask(TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!task_destruction_detector_);
  is_running_ = true;
  auto task_destruction_detector =
      std::make_unique<TaskDestructionDetector>(this);
  task_destruction_detector_ = task_destruction_detector.get();
  if (delay > TimeDelta::FromMicroseconds(0)) {
    GetTaskRunner()->PostDelayedTask(
        posted_from_,
        BindOnce(&TimerBase::OnScheduledTaskInvoked,
                 weak_ptr_factory_.GetWeakPtr(),
                 std::move(task_destruction_detector)),
        delay);
    scheduled_run_time_ = desired_run_time_ = Now() + delay;
  } else {
    GetTaskRunner()->PostTask(posted_from_,
                              BindOnce(&TimerBase::OnScheduledTaskInvoked,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(task_destruction_detector)));
    scheduled_run_time_ = desired_run_time_ = TimeTicks();
  }
}

scoped_refptr<SequencedTaskRunner> TimerBase::GetTaskRunner() {
  return task_runner_.get() ? task_runner_ : SequencedTaskRunnerHandle::Get();
}

TimeTicks TimerBase::Now() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tick_clock_ ? tick_clock_->NowTicks() : TimeTicks::Now();
}

void TimerBase::AbandonScheduledTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task_destruction_detector_) {
    task_destruction_detector_->Disable();
    task_destruction_detector_ = nullptr;
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
}

void TimerBase::OnScheduledTaskInvoked(
    std::unique_ptr<TaskDestructionDetector> task_destruction_detector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The scheduled task is currently running so its destruction detector is no
  // longer needed.
  task_destruction_detector->Disable();
  task_destruction_detector_ = nullptr;
  task_destruction_detector.reset();

  // The timer may have been stopped.
  if (!is_running_) {
    RecordScheduledTaskInvokedReason(ScheduledTaskInvokedReason::kStopped);
    return;
  }

  // First check if we need to delay the task because of a new target time.
  if (desired_run_time_ > scheduled_run_time_) {
    // Now() can be expensive, so only call it if we know the user has changed
    // the |desired_run_time_|.
    TimeTicks now = Now();
    // Task runner may have called us late anyway, so only post a continuation
    // task if the |desired_run_time_| is in the future.
    if (desired_run_time_ > now) {
      RecordScheduledTaskInvokedReason(
          ScheduledTaskInvokedReason::kRescheduled);
      // Post a new task to span the remaining time.
      ScheduleNewTask(desired_run_time_ - now);
      return;
    }
  }

  RecordScheduledTaskInvokedReason(ScheduledTaskInvokedReason::kReady);
  RunUserTask();
  // No more member accesses here: |this| could be deleted at this point.
}

}  // namespace internal

OneShotTimer::OneShotTimer() = default;
OneShotTimer::OneShotTimer(const TickClock* tick_clock)
    : internal::TimerBase(tick_clock) {}
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

RepeatingTimer::RepeatingTimer() = default;
RepeatingTimer::RepeatingTimer(const TickClock* tick_clock)
    : internal::TimerBase(tick_clock) {}
RepeatingTimer::~RepeatingTimer() = default;

RepeatingTimer::RepeatingTimer(const Location& posted_from,
                               TimeDelta delay,
                               RepeatingClosure user_task)
    : internal::TimerBase(posted_from, delay),
      user_task_(std::move(user_task)) {}
RepeatingTimer::RepeatingTimer(const Location& posted_from,
                               TimeDelta delay,
                               RepeatingClosure user_task,
                               const TickClock* tick_clock)
    : internal::TimerBase(posted_from, delay, tick_clock),
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

RetainingOneShotTimer::RetainingOneShotTimer() = default;
RetainingOneShotTimer::RetainingOneShotTimer(const TickClock* tick_clock)
    : internal::TimerBase(tick_clock) {}
RetainingOneShotTimer::~RetainingOneShotTimer() = default;

RetainingOneShotTimer::RetainingOneShotTimer(const Location& posted_from,
                                             TimeDelta delay,
                                             RepeatingClosure user_task)
    : internal::TimerBase(posted_from, delay),
      user_task_(std::move(user_task)) {}
RetainingOneShotTimer::RetainingOneShotTimer(const Location& posted_from,
                                             TimeDelta delay,
                                             RepeatingClosure user_task,
                                             const TickClock* tick_clock)
    : internal::TimerBase(posted_from, delay, tick_clock),
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

}  // namespace base
