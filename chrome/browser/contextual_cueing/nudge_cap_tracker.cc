// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/nudge_cap_tracker.h"

namespace contextual_cueing {
NudgeCapTracker::NudgeCapTracker(size_t cap_count, base::TimeDelta duration)
    : cap_count_(cap_count), duration_(duration) {}

NudgeCapTracker::NudgeCapTracker(NudgeCapTracker&& source)
    : cap_count_(source.cap_count_), duration_(source.duration_) {}

NudgeCapTracker::~NudgeCapTracker() = default;

void NudgeCapTracker::CueingNudgeShown() {
  CHECK(cap_count_ == 0 || recent_nudge_timestamps_.size() <= cap_count_);

  if (cap_count_ > 0 && recent_nudge_timestamps_.size() == cap_count_) {
    recent_nudge_timestamps_.pop();
  }
  recent_nudge_timestamps_.push(base::TimeTicks::Now());
}

bool NudgeCapTracker::CanShowNudge() const {
  if (!cap_count_) {  // No restriction.
    return true;
  }
  if (recent_nudge_timestamps_.size() < cap_count_) {
    return true;
  }

  base::TimeDelta time_diff =
      base::TimeTicks::Now() - recent_nudge_timestamps_.front();
  CHECK(time_diff.is_positive());
  return time_diff > duration_;
}

std::optional<base::TimeTicks> NudgeCapTracker::GetMostRecentNudgeTime() const {
  if (recent_nudge_timestamps_.empty()) {
    return std::nullopt;
  }
  return recent_nudge_timestamps_.back();
}

}  // namespace contextual_cueing
