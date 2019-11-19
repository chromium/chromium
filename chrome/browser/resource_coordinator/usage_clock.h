// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_USAGE_CLOCK_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_USAGE_CLOCK_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"

namespace resource_coordinator {

// A clock that advances when Chrome is in use.
//
// See metrics::DesktopSessionDurationTracker for how Chrome usage is tracked.
// If metrics::DesktopSessionDurationTracker isn't initialized before this, the
// clock will advance continuously, regardless of Chrome usage. This avoids
// forcing all tests that indirectly depend on this to initialize
// metrics::DesktopSessionDurationTracker.
class UsageClock
    : public metrics::DesktopSessionDurationTracker::Observer
{
 public:
  UsageClock();
  ~UsageClock() override;

  // Returns the amount of Chrome usage time since this was instantiated.
  base::TimeDelta GetTotalUsageTime() const;

  // Returns true if Chrome is currently considered to be in use.
  bool IsInUse() const;

 private:
  void OnSessionStarted(base::TimeTicks session_start) override;
  void OnSessionEnded(base::TimeDelta session_length,
                      base::TimeTicks session_end) override;

  // The total time elapsed in completed usage sessions. The duration of the
  // current usage session, if any, must be added to this to get the total usage
  // time of Chrome.
  base::TimeDelta usage_time_in_completed_sessions_;

  // The time at which the current session started, or a null TimeTicks if not
  // currently in a session.
  base::TimeTicks current_usage_session_start_time_;

  DISALLOW_COPY_AND_ASSIGN(UsageClock);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_USAGE_CLOCK_H_
