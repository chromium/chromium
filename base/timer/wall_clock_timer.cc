// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/wall_clock_timer.h"

#include <utility>

#include "base/power_monitor/power_monitor.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"

namespace base {

WallClockTimer::WallClockTimer() : WallClockTimer(nullptr, nullptr) {}

WallClockTimer::WallClockTimer(const Clock* clock, const TickClock* tick_clock)
    : timer_(tick_clock), clock_(clock ? clock : DefaultClock::GetInstance()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WallClockTimer::~WallClockTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveObserver();
}

void WallClockTimer::Start(const Location& posted_from,
                           Time desired_run_time,
                           OnceClosure user_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_task_ = std::move(user_task);
  posted_from_ = posted_from;
  desired_run_time_ = desired_run_time;
  AddObserver();
  timer_.Start(posted_from_, desired_run_time_ - Now(), this,
               &WallClockTimer::RunUserTask);
}

void WallClockTimer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
  user_task_.Reset();
  RemoveObserver();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

bool WallClockTimer::IsRunning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return timer_.IsRunning();
}

void WallClockTimer::OnResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This will actually restart timer with smaller delay
  timer_.Start(posted_from_, desired_run_time_ - Now(), this,
               &WallClockTimer::RunUserTask);
}

void WallClockTimer::AddObserver() {
  if (!observer_added_) {
    PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
    observer_added_ = true;
  }
}

void WallClockTimer::RemoveObserver() {
  if (observer_added_) {
    PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
    observer_added_ = false;
  }
}

void WallClockTimer::RunUserTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(user_task_);
  RemoveObserver();
  // Detach the sequence checker before running the task, just in case someone
  // sets a new task (= new sequence) while executing the task.
  auto task = std::move(user_task_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  std::move(task).Run();
}

Time WallClockTimer::Now() const {
  return clock_->Now();
}

}  // namespace base
