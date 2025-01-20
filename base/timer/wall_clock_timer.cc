// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/wall_clock_timer.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {

WallClockTimer::WallClockTimer() : WallClockTimer(nullptr, nullptr) {}

WallClockTimer::WallClockTimer(const Clock* clock, const TickClock* tick_clock)
    : timer_(tick_clock), clock_(clock ? clock : DefaultClock::GetInstance()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WallClockTimer::~WallClockTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

void WallClockTimer::Start(const Location& posted_from,
                           Time desired_run_time,
                           OnceClosure user_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(user_task);
  posted_from_ = posted_from;
  desired_run_time_ = desired_run_time;
  if (!user_task_) {
    PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
  }
  user_task_ = std::move(user_task);
  OnResume();
}

void WallClockTimer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
  user_task_.Reset();
  PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

bool WallClockTimer::IsRunning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return timer_.IsRunning();
}

void WallClockTimer::OnResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Start(posted_from_, desired_run_time_ - clock_->Now(), this,
               &WallClockTimer::RunUserTask);
}

void WallClockTimer::RunUserTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(user_task_);
  PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  // Detach the sequence checker before running the task, just in case someone
  // sets a new task (= new sequence) while executing the task.
  auto task = std::move(user_task_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  std::move(task).Run();
}

}  // namespace base
