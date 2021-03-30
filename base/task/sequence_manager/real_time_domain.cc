// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/real_time_domain.h"

#include "base/record_replay.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace sequence_manager {
namespace internal {

void RealTimeDomain::OnRegisterWithSequenceManager(
    SequenceManagerImpl* sequence_manager) {
  TimeDomain::OnRegisterWithSequenceManager(sequence_manager);
  tick_clock_ = sequence_manager->GetTickClock();
}

LazyNow RealTimeDomain::CreateLazyNow() const {
  return LazyNow(tick_clock_);
}

TimeTicks RealTimeDomain::Now() const {
  return tick_clock_->NowTicks();
}

Optional<TimeDelta> RealTimeDomain::DelayTillNextTask(LazyNow* lazy_now) {
  recordreplay::Assert("RealTimeDomain::DelayTillNextTask Start");

  Optional<TimeTicks> next_run_time = NextScheduledRunTime();
  if (!next_run_time) {
    recordreplay::Assert("RealTimeDomain::DelayTillNextTask #1");
    return nullopt;
  }

  TimeTicks now = lazy_now->Now();
  if (now >= next_run_time) {
    // Overdue work needs to be run immediately.
    recordreplay::Assert("RealTimeDomain::DelayTillNextTask #2");
    return TimeDelta();
  }

  TimeDelta delay = *next_run_time - now;
  TRACE_EVENT1("sequence_manager", "RealTimeDomain::DelayTillNextTask",
               "delay_ms", delay.InMillisecondsF());
  recordreplay::Assert("RealTimeDomain::DelayTillNextTask Done %.2f", delay.InSecondsF());
  return delay;
}

bool RealTimeDomain::MaybeFastForwardToNextTask(bool quit_when_idle_requested) {
  return false;
}

const char* RealTimeDomain::GetName() const {
  return "RealTimeDomain";
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
