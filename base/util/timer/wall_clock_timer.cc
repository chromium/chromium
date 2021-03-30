// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/timer/wall_clock_timer.h"

#include <utility>

#include "base/power_monitor/power_monitor.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"

namespace util {

WallClockTimer::WallClockTimer() = default;
WallClockTimer::WallClockTimer(const base::Clock* clock,
                               const base::TickClock* tick_clock)
    : timer_(tick_clock),
      clock_(clock ? clock : base::DefaultClock::GetInstance()) {}

WallClockTimer::~WallClockTimer() {
  RemoveObserver();
}

void WallClockTimer::Start(const base::Location& posted_from,
                           base::Time desired_run_time,
                           base::OnceClosure user_task) {
  user_task_ = std::move(user_task);
  posted_from_ = posted_from;
  desired_run_time_ = desired_run_time;
  AddObserver();
  timer_.Start(posted_from_, desired_run_time_ - Now(), this,
               &WallClockTimer::RunUserTask);
}

void WallClockTimer::Stop() {
  timer_.Stop();
  user_task_.Reset();
  RemoveObserver();
}

bool WallClockTimer::IsRunning() const {
  return timer_.IsRunning();
}

void WallClockTimer::OnResume() {
  // This will actually restart timer with smaller delay
  timer_.Start(posted_from_, desired_run_time_ - Now(), this,
               &WallClockTimer::RunUserTask);
}

void WallClockTimer::AddObserver() {
  if (!observer_added_) {
    base::PowerMonitor::AddPowerSuspendObserver(this);
    observer_added_ = true;
  }
}

void WallClockTimer::RemoveObserver() {
  if (observer_added_) {
    base::PowerMonitor::RemovePowerSuspendObserver(this);
    observer_added_ = false;
  }
}

void WallClockTimer::RunUserTask() {
  DCHECK(user_task_);
  RemoveObserver();
  std::exchange(user_task_, {}).Run();
}

base::Time WallClockTimer::Now() const {
  return clock_->Now();
}

}  // namespace util
