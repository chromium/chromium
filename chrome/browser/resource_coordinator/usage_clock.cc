// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/usage_clock.h"

#include "chrome/browser/resource_coordinator/time.h"

namespace resource_coordinator {

UsageClock::UsageClock() : current_usage_session_start_time_(NowTicks()) {
  if (metrics::DesktopSessionDurationTracker::IsInitialized()) {
    auto* tracker = metrics::DesktopSessionDurationTracker::Get();
    tracker->AddObserver(this);
    if (!tracker->in_session())
      current_usage_session_start_time_ = base::TimeTicks();
  }
}

UsageClock::~UsageClock() {
  if (metrics::DesktopSessionDurationTracker::IsInitialized())
    metrics::DesktopSessionDurationTracker::Get()->RemoveObserver(this);
}

base::TimeDelta UsageClock::GetTotalUsageTime() const {
  base::TimeDelta elapsed_time_in_session = usage_time_in_completed_sessions_;
  if (IsInUse())
    elapsed_time_in_session += NowTicks() - current_usage_session_start_time_;
  return elapsed_time_in_session;
}

bool UsageClock::IsInUse() const {
  return !current_usage_session_start_time_.is_null();
}

void UsageClock::OnSessionStarted(base::TimeTicks session_start) {
  // Ignore |session_start| because it doesn't come from the resource
  // coordinator clock.
  DCHECK(!IsInUse());
  current_usage_session_start_time_ = NowTicks();
}

void UsageClock::OnSessionEnded(base::TimeDelta session_length,
                                base::TimeTicks session_end) {
  // Ignore |session_length| because it wasn't measured using the resource
  // coordinator clock.
  DCHECK(IsInUse());
  usage_time_in_completed_sessions_ +=
      NowTicks() - current_usage_session_start_time_;
  current_usage_session_start_time_ = base::TimeTicks();
}

}  // namespace resource_coordinator
