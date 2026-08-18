// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/prefs.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/contextual_cueing/ucb_scorer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace contextual_cueing::prefs {

namespace {

// Returns a stable lowercase pref-name prefix for a given target type.
// Format: "contextual_cueing.ucb.<target_name>."
// Uses GetName() so the key is stable regardless of enum integer values.
std::string PrefPrefix(CueTargetType type) {
  return base::StrCat(
      {"contextual_cueing.ucb.", base::ToLowerASCII(GetName(type)), "."});
}

}  // namespace

std::string GetImpressionsPrefName(CueTargetType type) {
  return PrefPrefix(type) + "impressions";
}

std::string GetClicksPrefName(CueTargetType type) {
  return PrefPrefix(type) + "clicks";
}

std::string GetDismissalsPrefName(CueTargetType type) {
  return PrefPrefix(type) + "dismissals";
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Register prefs for all CueTargetTypes. kTestSource is included to prevent
  // fatal DCHECKs when tests using a real Profile trigger a test cue.
  for (int i = 0; i <= static_cast<int>(CueTargetType::kMaxValue); ++i) {
    CueTargetType type = static_cast<CueTargetType>(i);
    registry->RegisterIntegerPref(GetImpressionsPrefName(type), 0);
    registry->RegisterIntegerPref(GetClicksPrefName(type), 0);
    registry->RegisterIntegerPref(GetDismissalsPrefName(type), 0);
  }
}

TargetStats GetTargetStatsFromPrefs(PrefService* pref_service,
                                    CueTargetType type) {
  TargetStats stats;
  if (!pref_service) {
    return stats;
  }
  stats.impressions = pref_service->GetInteger(GetImpressionsPrefName(type));
  stats.clicks = pref_service->GetInteger(GetClicksPrefName(type));
  stats.dismissals = pref_service->GetInteger(GetDismissalsPrefName(type));
  return stats;
}

void SetTargetStatsInPrefs(PrefService* pref_service,
                           CueTargetType type,
                           const TargetStats& stats) {
  if (!pref_service) {
    return;
  }
  pref_service->SetInteger(GetImpressionsPrefName(type), stats.impressions);
  pref_service->SetInteger(GetClicksPrefName(type), stats.clicks);
  pref_service->SetInteger(GetDismissalsPrefName(type), stats.dismissals);
}

}  // namespace contextual_cueing::prefs
