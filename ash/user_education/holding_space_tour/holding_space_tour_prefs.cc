// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_tour/holding_space_tour_prefs.h"

#include "ash/constants/ash_features.h"
#include "base/json/values_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::holding_space_tour_prefs {
namespace {

// Constants -------------------------------------------------------------------

constexpr char kLastTimeTourShown[] =
    "ash.holding_space.wallpaper_nudge.last_shown_time";
constexpr char kTourShownCount[] =
    "ash.holding_space.wallpaper_nudge.shown_count";

}  // namespace

// Utilities -------------------------------------------------------------------

absl::optional<base::Time> GetLastTimeTourWasShown(PrefService* prefs) {
  auto* pref = prefs->FindPreference(kLastTimeTourShown);
  return pref->IsDefaultValue() ? absl::nullopt
                                : base::ValueToTime(pref->GetValue());
}

uint64_t GetTourShownCount(PrefService* prefs) {
  return prefs->GetUint64(kTourShownCount);
}

void MarkTourShown(PrefService* prefs) {
  CHECK(features::IsHoldingSpaceTourEnabled());
  prefs->SetTime(kLastTimeTourShown, base::Time::Now());
  prefs->SetUint64(kTourShownCount, GetTourShownCount(prefs) + 1u);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastTimeTourShown, base::Time());
  registry->RegisterUint64Pref(kTourShownCount, 0u);
}

}  // namespace ash::holding_space_tour_prefs
