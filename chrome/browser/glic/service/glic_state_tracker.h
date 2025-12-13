// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_STATE_TRACKER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_STATE_TRACKER_H_

#include "base/time/time.h"

namespace glic {

// Tracks a single boolean state, measures the total time it remains true, and
// logs the duration of each uninterrupted true period to a UMA histogram.
class GlicStateTracker {
 public:
  // |histogram_name| can be null if no histogram should be recorded for
  // uninterrupted durations.
  GlicStateTracker(bool initial_state, const char* histogram_name);
  ~GlicStateTracker();

  GlicStateTracker(const GlicStateTracker&) = delete;
  GlicStateTracker& operator=(const GlicStateTracker&) = delete;

  // Updates the state. If transitioning from true to false, records the
  // duration.
  void OnStateChanged(bool new_state);

  // Forces a state change to 'false' at the given time to record any final
  // duration.
  void Finalize();

  bool state() const { return state_; }
  base::TimeDelta total_duration() const { return total_duration_; }

 private:
  bool state_;
  base::TimeTicks last_change_time_;
  base::TimeDelta total_duration_;
  const char* histogram_name_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_STATE_TRACKER_H_
