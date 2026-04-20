// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_cueing/nudge_cap_tracker.h"

namespace contextual_cueing {

ContextualCueingService::ContextualCueingService()
    : recent_nudge_tracker_(kCueCapCount.Get(), kCueCapTime.Get()),
      recent_visited_origins_(kVisitedOriginsLimit.Get()) {}
ContextualCueingService::~ContextualCueingService() = default;

void ContextualCueingService::ReportPageLoad() {
  if (remaining_quiet_loads_) {
    remaining_quiet_loads_--;
  }
}

void ContextualCueingService::OnCueClicked(CueTargetType type) {
  // TODO(crbug.com/498985205): record the click
}

void ContextualCueingService::OnCueDismissed(CueTargetType type) {
  // TODO(crbug.com/498985205): record the dismissal
}

void ContextualCueingService::OnCueShown(const GURL& url) {
  if (kMinPageCountBetweenNudges.Get()) {
    // Let the cue logic be performed the next page after quiet count pages.
    remaining_quiet_loads_ = kMinPageCountBetweenNudges.Get() + 1;
  }
  shown_backoff_end_time_ =
      base::TimeTicks::Now() + kMinTimeBetweenNudges.Get();

  recent_nudge_tracker_.CueingNudgeShown();

  auto origin = url::Origin::Create(url);
  auto origin_iter = recent_visited_origins_.Get(origin);
  if (origin_iter == recent_visited_origins_.end()) {
    origin_iter = recent_visited_origins_.Put(
        origin, NudgeCapTracker(kCueCapCountPerOrigin.Get(),
                                kCueCapTimePerOrigin.Get()));
  }
  origin_iter->second.CueingNudgeShown();
}

contextual_cueing::ContextualCueingDecision ContextualCueingService::CanShowCue(
    const GURL& url) const {
  if (kDisableCueBackoff.Get()) {
    return ContextualCueingDecision::kSuccess;
  }

  if (remaining_quiet_loads_ > 0) {
    return ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue;
  }
  if (shown_backoff_end_time_ &&
      base::TimeTicks::Now() < shown_backoff_end_time_) {
    return ContextualCueingDecision::kNotEnoughTimeSinceLastCue;
  }

  if (!recent_nudge_tracker_.CanShowNudge()) {
    return ContextualCueingDecision::kTooManyCuesShownToTheUser;
  }

  auto origin_iter = recent_visited_origins_.Peek(url::Origin::Create(url));
  if (origin_iter != recent_visited_origins_.end() &&
      !origin_iter->second.CanShowNudge()) {
    return ContextualCueingDecision::kTooManyCuesShownToTheUserForOrigin;
  }

  return ContextualCueingDecision::kSuccess;
}

}  // namespace contextual_cueing
