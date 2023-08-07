// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_prefs.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::welcome_tour_prefs {
namespace {

// Constants -------------------------------------------------------------------

constexpr char kTimeOfFirstInteractionPrefPrefix[] =
    "ash.welcome_tour.interaction_time.";
constexpr char kTimeOfFirstTourCompletion[] =
    "ash.welcome_tour.completed.first_time";
constexpr char kTimeOfFirstTourPrevention[] =
    "ash.welcome_tour.prevented.first_time";

// Helpers ---------------------------------------------------------------------

std::string GetTimeOfFirstInteractionPrefName(
    welcome_tour_metrics::Interaction interaction) {
  return base::StrCat({kTimeOfFirstInteractionPrefPrefix,
                       welcome_tour_metrics::ToString(interaction),
                       ".first_time"});
}

absl::optional<base::Time> GetTimePrefIfSet(PrefService* prefs,
                                            const char* pref_name) {
  auto* pref = prefs->FindPreference(pref_name);
  return pref->IsDefaultValue() ? absl::nullopt
                                : base::ValueToTime(pref->GetValue());
}

}  // namespace

// Utilities -------------------------------------------------------------------

absl::optional<base::Time> GetTimeOfFirstTourCompletion(PrefService* prefs) {
  CHECK(features::IsWelcomeTourEnabled());
  return GetTimePrefIfSet(prefs, kTimeOfFirstTourCompletion);
}

absl::optional<base::Time> GetTimeOfFirstTourPrevention(PrefService* prefs) {
  CHECK(features::IsWelcomeTourEnabled());
  return GetTimePrefIfSet(prefs, kTimeOfFirstTourPrevention);
}

bool MarkTimeOfFirstInteraction(PrefService* prefs,
                                welcome_tour_metrics::Interaction interaction) {
  CHECK(features::IsWelcomeTourEnabled());
  const auto pref_name = GetTimeOfFirstInteractionPrefName(interaction);
  if (prefs->FindPreference(pref_name)->IsDefaultValue()) {
    prefs->SetTime(pref_name, base::Time::Now());
    return true;
  }
  return false;
}

bool MarkTimeOfFirstTourCompletion(PrefService* prefs) {
  CHECK(features::IsWelcomeTourEnabled());
  if (prefs->FindPreference(kTimeOfFirstTourCompletion)->IsDefaultValue()) {
    prefs->SetTime(kTimeOfFirstTourCompletion, base::Time::Now());
    return true;
  }
  return false;
}

bool MarkTimeOfFirstTourPrevention(PrefService* prefs) {
  CHECK(features::IsWelcomeTourEnabled());
  if (prefs->FindPreference(kTimeOfFirstTourPrevention)->IsDefaultValue()) {
    prefs->SetTime(kTimeOfFirstTourPrevention, base::Time::Now());
    return true;
  }
  return false;
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kTimeOfFirstTourCompletion, base::Time());
  registry->RegisterTimePref(kTimeOfFirstTourPrevention, base::Time());

  for (const auto interaction : welcome_tour_metrics::AllInteractionsSet()) {
    registry->RegisterTimePref(GetTimeOfFirstInteractionPrefName(interaction),
                               base::Time());
  }
}

}  // namespace ash::welcome_tour_prefs
