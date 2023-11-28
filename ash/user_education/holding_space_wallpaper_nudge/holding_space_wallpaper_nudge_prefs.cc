// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/json/values_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::holding_space_wallpaper_nudge_prefs {
namespace {

// Constants -------------------------------------------------------------------

// Prefs about the nudge being shown are stored separately for the
// counterfactual branch to make sure users that are on the counterfactual
// branch still see it in the event the feature is turned on for them later.
constexpr char kLastTimeNudgeShownCounterfactual[] =
    "ash.holding_space.wallpaper_nudge.last_shown_time_counterfactual";
constexpr char kLastTimeNudgeShown[] =
    "ash.holding_space.wallpaper_nudge.last_shown_time";
constexpr char kNudgeShownCountCounterfactual[] =
    "ash.holding_space.wallpaper_nudge.shown_count_counterfactual";
constexpr char kNudgeShownCount[] =
    "ash.holding_space.wallpaper_nudge.shown_count";

// Helpers ---------------------------------------------------------------------

// Gets the name of the pref that store how many times the nudge has been shown.
std::string GetNudgeCountPrefName() {
  return features::IsHoldingSpaceWallpaperNudgeEnabledCounterfactually()
             ? kNudgeShownCountCounterfactual
             : kNudgeShownCount;
}

// Gets the name of the pref that store the last time the nudge was shown.
std::string GetNudgeTimePrefName() {
  return features::IsHoldingSpaceWallpaperNudgeEnabledCounterfactually()
             ? kLastTimeNudgeShownCounterfactual
             : kLastTimeNudgeShown;
}

}  // namespace

// Utilities -------------------------------------------------------------------

std::optional<base::Time> GetLastTimeNudgeWasShown(PrefService* prefs) {
  auto* pref = prefs->FindPreference(GetNudgeTimePrefName());
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

uint64_t GetNudgeShownCount(PrefService* prefs) {
  return prefs->GetUint64(GetNudgeCountPrefName());
}

void MarkNudgeShown(PrefService* prefs) {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());
  prefs->SetTime(GetNudgeTimePrefName(), base::Time::Now());
  prefs->SetUint64(GetNudgeCountPrefName(), GetNudgeShownCount(prefs) + 1u);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastTimeNudgeShownCounterfactual, base::Time());
  registry->RegisterTimePref(kLastTimeNudgeShown, base::Time());
  registry->RegisterUint64Pref(kNudgeShownCountCounterfactual, 0u);
  registry->RegisterUint64Pref(kNudgeShownCount, 0u);
}

}  // namespace ash::holding_space_wallpaper_nudge_prefs
