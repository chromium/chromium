// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_metrics.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
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
constexpr char kTimeOfFirstInteractionPrefPrefix[] =
    "ash.holding_space.wallpaper_nudge.interaction_time.";
constexpr char kUserEligibleForNudge[] =
    "ash.holding_space.wallpaper_nudge.user_eligible";
constexpr char kUserFirstEligibleSessionTime[] =
    "ash.holding_space.wallpaper_nudge.first_eligible_session_time";

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

// Gets the name of the pref that stores the first time `interaction` happened
// since the most recent nudge.
std::string GetTimeOfFirstInteractionPrefName(
    holding_space_wallpaper_nudge_metrics::Interaction interaction) {
  return base::StrCat(
      {kTimeOfFirstInteractionPrefPrefix,
       holding_space_wallpaper_nudge_metrics::ToString(interaction),
       ".first_time"});
}

}  // namespace

// Utilities -------------------------------------------------------------------

std::optional<base::Time> GetTimeOfFirstEligibleSession(PrefService* prefs) {
  auto* pref = prefs->FindPreference(kUserFirstEligibleSessionTime);
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

std::optional<base::Time> GetLastTimeNudgeWasShown(PrefService* prefs) {
  auto* pref = prefs->FindPreference(GetNudgeTimePrefName());
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

uint64_t GetNudgeShownCount(PrefService* prefs) {
  return prefs->GetUint64(GetNudgeCountPrefName());
}

std::optional<bool> GetUserEligibility(PrefService* prefs) {
  auto* pref = prefs->FindPreference(kUserEligibleForNudge);

  if (pref->IsDefaultValue()) {
    return std::nullopt;
  }

  return pref->GetValue()->GetBool();
}

void MarkNudgeShown(PrefService* prefs) {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());
  prefs->SetTime(GetNudgeTimePrefName(), base::Time::Now());
  prefs->SetUint64(GetNudgeCountPrefName(), GetNudgeShownCount(prefs) + 1u);
}

bool MarkTimeOfFirstEligibleSession(PrefService* prefs) {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());
  if (prefs->FindPreference(kUserFirstEligibleSessionTime)->IsDefaultValue()) {
    prefs->SetTime(kUserFirstEligibleSessionTime, base::Time::Now());
    return true;
  }
  return false;
}

bool MarkTimeOfFirstInteraction(
    PrefService* prefs,
    holding_space_wallpaper_nudge_metrics::Interaction interaction) {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());
  if (!GetTimeOfFirstEligibleSession(prefs).has_value()) {
    return false;
  }

  const auto pref_name = GetTimeOfFirstInteractionPrefName(interaction);
  if (prefs->FindPreference(pref_name)->IsDefaultValue()) {
    prefs->SetTime(pref_name, base::Time::Now());
    return true;
  }
  return false;
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kUserEligibleForNudge, false);
  registry->RegisterTimePref(kLastTimeNudgeShownCounterfactual, base::Time());
  registry->RegisterTimePref(kLastTimeNudgeShown, base::Time());
  registry->RegisterTimePref(kUserFirstEligibleSessionTime, base::Time());
  registry->RegisterUint64Pref(kNudgeShownCountCounterfactual, 0u);
  registry->RegisterUint64Pref(kNudgeShownCount, 0u);

  for (const auto interaction :
       holding_space_wallpaper_nudge_metrics::kAllInteractionsSet) {
    registry->RegisterTimePref(GetTimeOfFirstInteractionPrefName(interaction),
                               base::Time());
  }
}

bool SetUserEligibility(PrefService* prefs, bool eligible) {
  auto* pref = prefs->FindPreference(kUserEligibleForNudge);
  if (!pref->IsDefaultValue()) {
    return false;
  }

  prefs->SetBoolean(kUserEligibleForNudge, eligible);
  return true;
}

}  // namespace ash::holding_space_wallpaper_nudge_prefs
