// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/queue.h"
#include "base/time/time.h"

namespace contextual_cueing {

// Keeps track of timestamps of recent nudges for the sake of capping nudge
// count over a period of time. (i.e. x nudges per y hours). This queue is
// maintained such that it only has timestamps necessary to enforce the limits.
// Old timestamps will be trimmed.
class NudgeCapTracker {
 public:
  NudgeCapTracker(size_t cap_count, base::TimeDelta duration);

  NudgeCapTracker(const NudgeCapTracker&) = delete;
  NudgeCapTracker& operator=(const NudgeCapTracker&) = delete;

  NudgeCapTracker(NudgeCapTracker&& source);
  ~NudgeCapTracker();

  // Returns whether the nudge is eligible to be shown.
  bool CanShowNudge() const;

  // Notifies that the nudge was shown to user. This tracks the shown timestamps
  // for nudge cap calculation.
  void CueingNudgeShown();

  // Returns the time when the most recent nudge was shown.
  std::optional<base::TimeTicks> GetMostRecentNudgeTime() const;

 private:
  base::queue<base::TimeTicks> recent_nudge_timestamps_;

  // The nudge can be shown `cap_count_` times over `duration_`.
  const size_t cap_count_;
  const base::TimeDelta duration_;
};

}  // namespace contextual_cueing
