// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_

#include <optional>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/contextual_cueing/nudge_cap_tracker.h"
#include "components/contextual_cueing/ucb_scorer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_cueing/internals/contextual_cueing_internals.mojom.h"
#endif

class PrefService;

namespace contextual_cueing {

enum class ContextualCueingDecision;

class ContextualCueingService : public KeyedService {
 public:
  static constexpr size_t kMaxShownCues = 20;

  explicit ContextualCueingService(PrefService* pref_service);
  ~ContextualCueingService() override;

  // Reports a page load occurred. This is used to keep track of quiet
  // page loads requirement after a cueing UI is shown.
  void ReportPageLoad();

  // Called when the user clicks the cue action button.
  void OnCueClicked(CueTargetType type);

  // Called when the user dismisses the cue.
  void OnCueDismissed(CueTargetType type);

  // Called when the cue is shown to the user.
  void OnCueShown(const GURL& url, CueTargetType type);

  // Returns true if a nudge can be shown.
  ContextualCueingDecision CanShowCue(const GURL& url) const;

  // Returns the UCB score for the given target, incorporating per-target
  // interaction stats and UCB hyperparameters from Finch.
  double GetUcbScore(CueTargetType type) const;

  // Returns the per-target interaction stats for the given target.
  const TargetStats& GetStatsForTarget(CueTargetType type) const;

  // Returns the total number of impressions across all targets.
  int GetTotalImpressions() const;

#if !BUILDFLAG(IS_ANDROID)
  using CueLogPtr = contextual_cueing_internals::mojom::CueLogPtr;

  // Logs metadata for a cue shown to the user for WebUI debugging.
  void LogCueShownMetadata(CueLogPtr cue_log);

  // Returns the list of shown cues for WebUI debugging.
  const base::circular_deque<CueLogPtr>& shown_cues() const {
    return shown_cues_;
  }
#endif

 private:
  // A counter for how many subsequent page load events will be prevented from
  // showing a nudge. This is to limit the frequency at which consecutive page
  // loads can trigger nudges.
  size_t remaining_quiet_loads_ = 0;

  // The end of the backoff period triggered by the last shown nudge.
  std::optional<base::TimeTicks> shown_backoff_end_time_;

  // Number of times the cueing nudge has been dismissed (i.e. closed by the
  // user). This count resets to 0 if nudge is clicked on by the user.
  int dismiss_count_ = 0;

  // The end of the backoff period triggered by the last dismissed nudge.
  std::optional<base::TimeTicks> dismiss_backoff_end_time_;

  // The end of the backoff period triggered by the last clicked nudge.
  std::optional<base::TimeTicks> click_backoff_end_time_;

  // Tracker to limit the number of nudges shown over a certain duration.
  NudgeCapTracker recent_nudge_tracker_;

  // Maintains the recently visited origins along with their nudge cap tracking.
  base::LRUCache<url::Origin, NudgeCapTracker> recent_visited_origins_;

  // Per-target interaction stats used by the UCB scorer.
  absl::flat_hash_map<CueTargetType, TargetStats> target_stats_;

  // Writes the stats for `type` to the profile prefs.
  void WriteStatsToPref(CueTargetType type);

  // Not owned. Guaranteed to outlive this service (profile lifetime).
  const raw_ptr<PrefService> profile_prefs_;

  SEQUENCE_CHECKER(sequence_checker_);

#if !BUILDFLAG(IS_ANDROID)
  base::circular_deque<CueLogPtr> shown_cues_;
#endif
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
