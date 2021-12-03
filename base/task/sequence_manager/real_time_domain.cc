// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/real_time_domain.h"

#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace sequence_manager {
namespace internal {

RealTimeDomain::RealTimeDomain(const base::TickClock* clock)
    : tick_clock_(clock) {}

TimeTicks RealTimeDomain::NowTicks() const {
  return tick_clock_->NowTicks();
}

base::TimeTicks RealTimeDomain::GetNextDelayedTaskTime(
    WakeUp next_wake_up,
    sequence_manager::LazyNow* lazy_now) const {
  TimeTicks now = lazy_now->Now();
  if (now >= next_wake_up.time) {
    // Overdue work needs to be run immediately.
    return TimeTicks();
  }

  TimeDelta delay = next_wake_up.time - now;
  TRACE_EVENT1("sequence_manager", "RealTimeDomain::DelayTillNextTask",
               "delay_ms", delay.InMillisecondsF());
  return next_wake_up.time;
}

bool RealTimeDomain::MaybeFastForwardToWakeUp(
    absl::optional<WakeUp> next_wake_up,
    bool quit_when_idle_requested) {
  return false;
}

const char* RealTimeDomain::GetName() const {
  return "RealTimeDomain";
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
