// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"

#include "ash/constants/ash_features.h"
#include "base/json/values_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::holding_space_wallpaper_nudge_prefs {
namespace {

// Constants -------------------------------------------------------------------

constexpr char kLastTimeNudgeShown[] =
    "ash.holding_space.wallpaper_nudge.last_shown_time";
constexpr char kNudgeShownCount[] =
    "ash.holding_space.wallpaper_nudge.shown_count";

}  // namespace

// Utilities -------------------------------------------------------------------

absl::optional<base::Time> GetLastTimeNudgeWasShown(PrefService* prefs) {
  auto* pref = prefs->FindPreference(kLastTimeNudgeShown);
  return pref->IsDefaultValue() ? absl::nullopt
                                : base::ValueToTime(pref->GetValue());
}

uint64_t GetNudgeShownCount(PrefService* prefs) {
  return prefs->GetUint64(kNudgeShownCount);
}

void MarkNudgeShown(PrefService* prefs) {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());
  prefs->SetTime(kLastTimeNudgeShown, base::Time::Now());
  prefs->SetUint64(kNudgeShownCount, GetNudgeShownCount(prefs) + 1u);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastTimeNudgeShown, base::Time());
  registry->RegisterUint64Pref(kNudgeShownCount, 0u);
}

}  // namespace ash::holding_space_wallpaper_nudge_prefs
