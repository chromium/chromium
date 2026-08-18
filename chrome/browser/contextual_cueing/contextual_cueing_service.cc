// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_cueing/prefs.h"
#include "components/contextual_cueing/contextual_cueing_enums.h"
#include "components/contextual_cueing/nudge_cap_tracker.h"
#include "components/contextual_cueing/ucb_scorer.h"
#include "components/prefs/pref_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_cueing/internals/contextual_cueing_internals.mojom.h"
#endif

namespace {
const contextual_cueing::TargetStats kEmptyStats;
}  // namespace

namespace contextual_cueing {

ContextualCueingService::ContextualCueingService(PrefService* profile_prefs)
    : recent_nudge_tracker_(kCueCapCount.Get(), kCueCapTime.Get()),
      recent_visited_origins_(kVisitedOriginsLimit.Get()),
      profile_prefs_(profile_prefs) {
  CHECK(profile_prefs_);
  // Restore per-target UCB stats from the persisted prefs so UCB scoring
  // carries historical signal across browser restarts.
  for (int i = 0; i <= static_cast<int>(CueTargetType::kMaxValue); ++i) {
    CueTargetType type = static_cast<CueTargetType>(i);
    TargetStats stats = prefs::GetTargetStatsFromPrefs(profile_prefs_, type);
    // Only store stats if there has been some activity, to save memory.
    if (stats.impressions > 0 || stats.clicks > 0 || stats.dismissals > 0) {
      target_stats_[type] = stats;
    }
  }
}
ContextualCueingService::~ContextualCueingService() = default;

void ContextualCueingService::WriteStatsToPref(CueTargetType type) {
  auto it = target_stats_.find(type);
  DCHECK(it != target_stats_.end());
  if (it == target_stats_.end()) {
    return;
  }
  prefs::SetTargetStatsInPrefs(profile_prefs_, type, it->second);
}

void ContextualCueingService::ReportPageLoad() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remaining_quiet_loads_) {
    remaining_quiet_loads_--;
  }
}

void ContextualCueingService::OnCueClicked(CueTargetType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  click_backoff_end_time_ = base::TimeTicks::Now() + kClickBackoffTime.Get();
  dismiss_count_ = 0;
  target_stats_[type].clicks++;
  WriteStatsToPref(type);
}

void ContextualCueingService::OnCueDismissed(CueTargetType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeDelta backoff_duration =
      kDismissBackoffTime.Get() *
      pow(kDismissBackoffMultiplierBase.Get(), dismiss_count_);
  dismiss_backoff_end_time_ = base::TimeTicks::Now() + backoff_duration;
  ++dismiss_count_;
  target_stats_[type].dismissals++;
  WriteStatsToPref(type);
}

void ContextualCueingService::OnCueShown(const GURL& url, CueTargetType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  target_stats_[type].impressions++;
  WriteStatsToPref(type);
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualCueingService::LogCueShownMetadata(CueLogPtr cue_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cue_log) {
    if (shown_cues_.size() >= kMaxShownCues) {
      shown_cues_.pop_front();
    }
    shown_cues_.push_back(std::move(cue_log));
  }
}
#endif

contextual_cueing::ContextualCueingDecision ContextualCueingService::CanShowCue(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (kDisableCueBackoff.Get()) {
    return ContextualCueingDecision::kSuccess;
  }

  if (remaining_quiet_loads_ > 0) {
    return ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue;
  }
  if (shown_backoff_end_time_ &&
      (base::TimeTicks::Now() < *shown_backoff_end_time_)) {
    return ContextualCueingDecision::kNotEnoughTimeSinceLastCue;
  }
  if (dismiss_backoff_end_time_ &&
      (base::TimeTicks::Now() < *dismiss_backoff_end_time_)) {
    return ContextualCueingDecision::kNotEnoughTimeSinceLastDismissal;
  }
  if (click_backoff_end_time_ &&
      (base::TimeTicks::Now() < *click_backoff_end_time_)) {
    return ContextualCueingDecision::kNotEnoughTimeSinceLastClick;
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

const TargetStats& ContextualCueingService::GetStatsForTarget(
    CueTargetType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = target_stats_.find(type);
  if (it == target_stats_.end()) {
    return kEmptyStats;
  }
  return it->second;
}

int ContextualCueingService::GetTotalImpressions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int total = 0;
  for (const auto& [type, stats] : target_stats_) {
    total += stats.impressions;
  }
  return total;
}

double ContextualCueingService::GetUcbScore(CueTargetType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UCBHyperparameters params{
      .c = kUcbExplorationWeight.Get(),
      .alpha = kUcbAlpha.Get(),
      .beta = kUcbBeta.Get(),
      .gamma = kUcbGamma.Get(),
      .max_explore_impressions = kUcbMaxExploreImpressions.Get(),
  };
  return CalculateUCBScore(GetStatsForTarget(type), GetTotalImpressions(),
                           params);
}

}  // namespace contextual_cueing
