// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "url/gurl.h"

namespace {

void LogNudgeInteraction(contextual_cueing::NudgeInteraction interaction) {
  base::UmaHistogramEnumeration("ContextualCueing.NudgeInteraction",
                                interaction);
}

}  // namespace

namespace contextual_cueing {

ContextualCueingService::ContextualCueingService()
    : recent_nudge_tracker_(kNudgeCapCount.Get(), kNudgeCapTime.Get()),
      recent_visited_origins_(kVisitedDomainsLimit.Get()) {}

ContextualCueingService::~ContextualCueingService() = default;

void ContextualCueingService::ReportPageLoad() {
  if (remaining_quiet_loads_) {
    remaining_quiet_loads_--;
  }
}

void ContextualCueingService::CueingNudgeShown(const GURL& url) {
  recent_nudge_tracker_.CueingNudgeShown();
  LogNudgeInteraction(NudgeInteraction::kShown);

  if (kMinPageCountBetweenNudges.Get()) {
    // Let the cue logic be performed the next page after quiet count pages.
    remaining_quiet_loads_ = kMinPageCountBetweenNudges.Get() + 1;
  }

  auto origin = url::Origin::Create(url);
  auto iter = recent_visited_origins_.Get(origin);
  if (iter == recent_visited_origins_.end()) {
    iter = recent_visited_origins_.Put(
        origin, NudgeCapTracker(kNudgeCapCountPerDomain.Get(),
                                kNudgeCapTimePerDomain.Get()));
  }
  iter->second.CueingNudgeShown();
}

void ContextualCueingService::CueingNudgeDismissed() {
  LogNudgeInteraction(NudgeInteraction::kDismissed);

  base::TimeDelta backoff_duration =
      kBackoffTime.Get() * pow(kBackoffMultiplierBase.Get(), dismiss_count_);

  backoff_end_time_ = base::Time::Now() + backoff_duration;
  ++dismiss_count_;
}

void ContextualCueingService::CueingNudgeClicked() {
  LogNudgeInteraction(NudgeInteraction::kClicked);

  dismiss_count_ = 0;
}

NudgeDecision ContextualCueingService::CanShowNudge(const GURL& url) {
  if (remaining_quiet_loads_ > 0) {
    return NudgeDecision::kNotEnoughPageLoadsSinceLastNudge;
  }
  if (IsNudgeBlockedByBackoffRule()) {
    return NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge;
  }
  if (!recent_nudge_tracker_.CanShowNudge()) {
    return NudgeDecision::kTooManyNudgesShownToTheUser;
  }
  auto iter = recent_visited_origins_.Peek(url::Origin::Create(url));
  if (iter != recent_visited_origins_.end() && !iter->second.CanShowNudge()) {
    return NudgeDecision::kTooManyNudgesShownToTheUserForDomain;
  }
  return NudgeDecision::kSuccess;
}

bool ContextualCueingService::IsNudgeBlockedByBackoffRule() const {
  return backoff_end_time_ && (base::Time::Now() < backoff_end_time_);
}

void ContextualCueingService::OnNudgeActivity(
    const GURL& url,
    ukm::SourceId source_id,
    tabs::GlicNudgeActivity activity) {
  switch (activity) {
    case tabs::GlicNudgeActivity::kNudgeShown:
      CueingNudgeShown(url);
      break;
    case tabs::GlicNudgeActivity::kNudgeClicked:
      CueingNudgeClicked();
      break;
    case tabs::GlicNudgeActivity::kNudgeDismissed:
      CueingNudgeDismissed();
      break;
    case tabs::GlicNudgeActivity::kNudgeNotShownWebContents:
      LogNudgeInteraction(NudgeInteraction::kNudgeNotShownWebContents);
      break;
  }
}
}  // namespace contextual_cueing
