// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_PREFS_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_PREFS_H_

#include <string>

#include "chrome/browser/contextual_cueing/cue_target.h"

class PrefRegistrySimple;
class PrefService;

namespace contextual_cueing {

struct TargetStats;

enum class ChromeSuggestionsSettingsValue {
  kEnabled = 0,
  kDisabled = 1,
};

namespace prefs {

// Per-target UCB interaction counters, persisted across restarts.
// Pref names are derived at runtime via the helpers below, keyed by the
// stable GetName() string for each target type (not the enum integer value).
// Each non-test CueTargetType gets three integer prefs: impressions, clicks,
// dismissals. E.g. for kGlic (GetName = "Glic"):
//   "contextual_cueing.ucb.glic.impressions"  -- impression count
//   "contextual_cueing.ucb.glic.clicks"       -- click count
//   "contextual_cueing.ucb.glic.dismissals"   -- dismissal count

// Returns the pref name for the impression count for |type|.
std::string GetImpressionsPrefName(CueTargetType type);

// Returns the pref name for the click count for |type|.
std::string GetClicksPrefName(CueTargetType type);

// Returns the pref name for the dismissal count for |type|.
std::string GetDismissalsPrefName(CueTargetType type);

// Registers all per-target UCB stat prefs. Called by the service factory and
// optionally by tests that need pref-backed persistence.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Retrieves per-target UCB stats from prefs.
TargetStats GetTargetStatsFromPrefs(PrefService* pref_service,
                                    CueTargetType type);

// Saves per-target UCB stats to prefs.
void SetTargetStatsInPrefs(PrefService* pref_service,
                           CueTargetType type,
                           const TargetStats& stats);

}  // namespace prefs
}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_PREFS_H_
