// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_prefs.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::welcome_tour_prefs {
namespace {

// Constants -------------------------------------------------------------------

// TODO: crbug.com/353568276 - Clean up the deprecated prefs.
// static constexpr char kTimeOfFirstInteractionPrefPrefix[] =
//     "ash.welcome_tour.interaction_time.";
// static constexpr char kTimeOfFirstTourCompletion[] =
//     "ash.welcome_tour.completed.first_time";
// static constexpr char kTimeOfFirstTourPrevention[] =
//     "ash.welcome_tour.prevented.first_time";
// static constexpr char kReasonForFirstTourPrevention[] =
//     "ash.welcome_tour.prevented.first_reason";

static constexpr char kFirstExperimentalArm[] =
    "ash.welcome_tour.v2.experimental_arm.first";
static constexpr char kTimeOfFirstInteractionPrefPrefix[] =
    "ash.welcome_tour.v2.interaction_time.";
static constexpr char kTimeOfFirstTourAborted[] =
    "ash.welcome_tour.v2.aborted.first_time";
static constexpr char kTimeOfFirstTourCompletion[] =
    "ash.welcome_tour.v2.completed.first_time";
static constexpr char kTimeOfFirstTourPrevention[] =
    "ash.welcome_tour.v2.prevented.first_time";
static constexpr char kReasonForFirstTourPrevention[] =
    "ash.welcome_tour.v2.prevented.first_reason";

// Helpers ---------------------------------------------------------------------

std::string GetTimeOfFirstInteractionPrefName(
    welcome_tour_metrics::Interaction interaction) {
  return base::StrCat({kTimeOfFirstInteractionPrefPrefix,
                       welcome_tour_metrics::ToString(interaction),
                       ".first_time"});
}

std::optional<base::Time> GetTimePrefIfSet(PrefService* prefs,
                                           const char* pref_name) {
  auto* pref = prefs->FindPreference(pref_name);
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

bool TourWasPreventedAsHoldback(PrefService* prefs) {
  return GetReasonForFirstTourPrevention(prefs) ==
         welcome_tour_metrics::PreventedReason::kHoldbackExperimentArm;
}

}  // namespace

// Utilities -------------------------------------------------------------------

std::optional<welcome_tour_metrics::ExperimentalArm> GetFirstExperimentalArm(
    PrefService* prefs) {
  CHECK(features::IsWelcomeTourEnabled());

  auto* pref = prefs->FindPreference(kFirstExperimentalArm);
  if (!pref || pref->IsDefaultValue() || !pref->GetValue()->is_int()) {
    return std::nullopt;
  }

  auto arm = static_cast<welcome_tour_metrics::ExperimentalArm>(
      pref->GetValue()->GetInt());
  if (welcome_tour_metrics::kAllExperimentalArmsSet.Has(arm)) {
    return arm;
  }

  return std::nullopt;
}

std::optional<base::Time> GetTimeOfFirstInteraction(
    PrefService* prefs,
    welcome_tour_metrics::Interaction interaction) {
  auto pref_name = GetTimeOfFirstInteractionPrefName(interaction);
  auto* pref = prefs->FindPreference(pref_name);
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

std::optional<base::Time> GetTimeOfFirstTourAborted(PrefService* prefs) {
  CHECK(features::IsWelcomeTourEnabled());
  return GetTimePrefIfSet(prefs, kTimeOfFirstTourAborted);
}

std::optional<base::Time> GetTimeOfFirstTourAttempt(PrefService* prefs) {
  std::optional<base::Time> first_aborted = GetTimeOfFirstTourAborted(prefs);
  std::optional<base::Time> first_completion =
      GetTimeOfFirstTourCompletion(prefs);
  std::optional<base::Time> first_prevention =
      TourWasPreventedAsHoldback(prefs) ? GetTimeOfFirstTourPrevention(prefs)
                                        : std::nullopt;

  if (!first_aborted && !first_completion && !first_prevention) {
    return std::nullopt;
  }

  return std::min(first_aborted.value_or(base::Time::Max()),
                  std::min(first_completion.value_or(base::Time::Max()),
                           first_prevention.value_or(base::Time::Max())));
}

std::optional<base::Time> GetTimeOfFirstTourCompletion(PrefService* prefs) {
  CHECK(features::IsWelcomeTourEnabled());
  return GetTimePrefIfSet(prefs, kTimeOfFirstTourCompletion);
}

std::optional<base::Time> GetTimeOfFirstTourPrevention(PrefService* prefs) {
  CHECK(features::IsWelcomeTourEnabled());
  return GetTimePrefIfSet(prefs, kTimeOfFirstTourPrevention);
}

std::optional<welcome_tour_metrics::PreventedReason>
GetReasonForFirstTourPrevention(PrefService* prefs) {
  using welcome_tour_metrics::PreventedReason;

  CHECK(features::IsWelcomeTourEnabled());

  auto* pref = prefs->FindPreference(kReasonForFirstTourPrevention);
  if (!pref || pref->IsDefaultValue() || !pref->GetValue()->is_int()) {
    return std::nullopt;
  }

  auto reason = static_cast<PreventedReason>(pref->GetValue()->GetInt());
  if (welcome_tour_metrics::kAllPreventedReasonsSet.Has(reason)) {
    return reason;
  }

  return PreventedReason::kUnknown;
}

bool MarkFirstExperimentalArm(PrefService* prefs,
                              welcome_tour_metrics::ExperimentalArm arm) {
  CHECK(features::IsWelcomeTourEnabled());

  if (prefs->FindPreference(kFirstExperimentalArm)->IsDefaultValue()) {
    prefs->SetInteger(kFirstExperimentalArm, static_cast<int>(arm));
    return true;
  }

  return false;
}

bool MarkFirstTourPrevention(PrefService* prefs,
                             welcome_tour_metrics::PreventedReason reason) {
  CHECK(features::IsWelcomeTourEnabled());

  if (prefs->FindPreference(kTimeOfFirstTourPrevention)->IsDefaultValue()) {
    prefs->SetTime(kTimeOfFirstTourPrevention, base::Time::Now());
    prefs->SetInteger(kReasonForFirstTourPrevention, static_cast<int>(reason));
    return true;
  }

  return false;
}

bool MarkTimeOfFirstInteraction(PrefService* prefs,
                                welcome_tour_metrics::Interaction interaction) {
  CHECK(features::IsWelcomeTourEnabled());

  const auto now = base::Time::Now();
  const auto time_to_measure_from = GetTimeOfFirstTourAttempt(prefs);

  // This function should only be called if the tour has been attempted, so that
  // we always have a time to measure the delta from.
  CHECK(time_to_measure_from.has_value());

  // Set the continuous time pref.
  const auto time_pref_name = GetTimeOfFirstInteractionPrefName(interaction);
  if (prefs->FindPreference(time_pref_name)->IsDefaultValue()) {
    prefs->SetTime(time_pref_name, now);
    return true;
  }

  return false;
}

bool MarkTimeOfFirstTourAborted(PrefService* prefs) {
  CHECK(features::IsWelcomeTourEnabled());
  if (prefs->FindPreference(kTimeOfFirstTourAborted)->IsDefaultValue()) {
    prefs->SetTime(kTimeOfFirstTourAborted, base::Time::Now());
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

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kTimeOfFirstTourAborted, base::Time());
  registry->RegisterTimePref(kTimeOfFirstTourCompletion, base::Time());
  registry->RegisterTimePref(kTimeOfFirstTourPrevention, base::Time());
  registry->RegisterIntegerPref(kFirstExperimentalArm, -1);
  registry->RegisterIntegerPref(kReasonForFirstTourPrevention, -1);

  for (const auto interaction : welcome_tour_metrics::kAllInteractionsSet) {
    registry->RegisterTimePref(GetTimeOfFirstInteractionPrefName(interaction),
                               base::Time());
  }
}

}  // namespace ash::welcome_tour_prefs
