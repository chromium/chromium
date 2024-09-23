// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_prefs.h"

#include "ash/constants/ash_features.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::holding_space_prefs {

namespace {

// Boolean preference storing if holding space previews are enabled.
constexpr char kPreviewsEnabled[] = "ash.holding_space.previews_enabled";

// Boolean preference storing if holding space suggestions is expanded.
constexpr char kSuggestionsExpanded[] =
    "ash.holding_space.suggestions_expanded";

// Time preference storing when an item was first added to holding space.
constexpr char kTimeOfFirstAdd[] = "ash.holding_space.time_of_first_add";

// Time preference storing when holding space first became available.
constexpr char kTimeOfFirstAvailability[] =
    "ash.holding_space.time_of_first_availability";

// Time preference storing when holding space was first entered.
constexpr char kTimeOfFirstEntry[] = "ash.holding_space.time_of_first_entry";

// Time preference storing when the Files app chip in the holding space pinned
// files section placeholder was first pressed.
constexpr char kTimeOfFirstFilesAppChipPress[] =
    "ash.holding_space.time_of_first_files_app_chip_press";

// Time preference storing when the first pin to holding space occurred.
constexpr char kTimeOfFirstPin[] = "ash.holding_space.time_of_first_pin";

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Boolean prefs.
  registry->RegisterBooleanPref(kPreviewsEnabled, true);
  registry->RegisterBooleanPref(kSuggestionsExpanded, true);

  // Time prefs.
  const base::Time unix_epoch = base::Time::UnixEpoch();
  registry->RegisterTimePref(kTimeOfFirstAdd, unix_epoch);
  registry->RegisterTimePref(kTimeOfFirstAvailability, unix_epoch);
  registry->RegisterTimePref(kTimeOfFirstEntry, unix_epoch);
  registry->RegisterTimePref(kTimeOfFirstFilesAppChipPress, unix_epoch);
  registry->RegisterTimePref(kTimeOfFirstPin, unix_epoch);
}

void ResetProfilePrefsForTesting(PrefService* prefs) {
  prefs->ClearPref(kPreviewsEnabled);
  prefs->ClearPref(kTimeOfFirstAdd);
  prefs->ClearPref(kTimeOfFirstAvailability);
  prefs->ClearPref(kTimeOfFirstEntry);
  prefs->ClearPref(kTimeOfFirstFilesAppChipPress);
  prefs->ClearPref(kTimeOfFirstPin);
}

void AddPreviewsEnabledChangedCallback(PrefChangeRegistrar* registrar,
                                       base::RepeatingClosure callback) {
  registrar->Add(kPreviewsEnabled, std::move(callback));
}

void AddSuggestionsExpandedChangedCallback(PrefChangeRegistrar* registrar,
                                           base::RepeatingClosure callback) {
  registrar->Add(kSuggestionsExpanded, std::move(callback));
}

void AddTimeOfFirstAddChangedCallback(PrefChangeRegistrar* registrar,
                                      base::RepeatingClosure callback) {
  registrar->Add(kTimeOfFirstAdd, std::move(callback));
}

void AddTimeOfFirstPinChangedCallback(PrefChangeRegistrar* registrar,
                                      base::RepeatingClosure callback) {
  registrar->Add(kTimeOfFirstPin, std::move(callback));
}

bool IsPreviewsEnabled(PrefService* prefs) {
  return prefs->GetBoolean(kPreviewsEnabled);
}

void SetPreviewsEnabled(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kPreviewsEnabled, enabled);
}

bool IsSuggestionsExpanded(PrefService* prefs) {
  return prefs->GetBoolean(kSuggestionsExpanded);
}

void SetSuggestionsExpanded(PrefService* prefs, bool expanded) {
  prefs->SetBoolean(kSuggestionsExpanded, expanded);
}

std::optional<base::Time> GetTimeOfFirstAdd(PrefService* prefs) {
  // The `kTimeOfFirstAdd` preference was added after the `kTimeOfFirstPin`
  // preference, so if the `kTimeOfFirstAdd` has not yet been marked it's
  // possible that the user may still have pinned a file at an earlier time.
  auto* pref = prefs->FindPreference(kTimeOfFirstAdd);
  return pref->IsDefaultValue() ? GetTimeOfFirstPin(prefs)
                                : base::ValueToTime(pref->GetValue());
}

bool MarkTimeOfFirstAdd(PrefService* prefs) {
  if (prefs->FindPreference(kTimeOfFirstAdd)->IsDefaultValue()) {
    // The `kTimeOfFirstAdd` preference was added after the `kTimeOfFirstPin`
    // preference, so it's possible that this is not actually the first time an
    // item has been added to holding space. If `kTimeOfFirstPin` was previously
    // recorded, that will be more accurate than using `base::Time::Now()`.
    prefs->SetTime(kTimeOfFirstAdd,
                   GetTimeOfFirstPin(prefs).value_or(base::Time::Now()));
    return true;
  }
  return false;
}

std::optional<base::Time> GetTimeOfFirstAvailability(PrefService* prefs) {
  auto* pref = prefs->FindPreference(kTimeOfFirstAvailability);
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

bool MarkTimeOfFirstAvailability(PrefService* prefs) {
  if (prefs->FindPreference(kTimeOfFirstAvailability)->IsDefaultValue()) {
    prefs->SetTime(kTimeOfFirstAvailability, base::Time::Now());
    return true;
  }
  return false;
}

std::optional<base::Time> GetTimeOfFirstEntry(PrefService* prefs) {
  auto* pref = prefs->FindPreference(kTimeOfFirstEntry);
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

bool MarkTimeOfFirstEntry(PrefService* prefs) {
  if (prefs->FindPreference(kTimeOfFirstEntry)->IsDefaultValue()) {
    prefs->SetTime(kTimeOfFirstEntry, base::Time::Now());
    return true;
  }
  return false;
}

std::optional<base::Time> GetTimeOfFirstPin(PrefService* prefs) {
  auto* pref = prefs->FindPreference(kTimeOfFirstPin);
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

bool MarkTimeOfFirstPin(PrefService* prefs) {
  if (prefs->FindPreference(kTimeOfFirstPin)->IsDefaultValue()) {
    prefs->SetTime(kTimeOfFirstPin, base::Time::Now());
    return true;
  }
  return false;
}

std::optional<base::Time> GetTimeOfFirstFilesAppChipPress(PrefService* prefs) {
  auto* pref = prefs->FindPreference(kTimeOfFirstFilesAppChipPress);
  return pref->IsDefaultValue() ? std::nullopt
                                : base::ValueToTime(pref->GetValue());
}

bool MarkTimeOfFirstFilesAppChipPress(PrefService* prefs) {
  if (prefs->FindPreference(kTimeOfFirstFilesAppChipPress)->IsDefaultValue()) {
    prefs->SetTime(kTimeOfFirstFilesAppChipPress, base::Time::Now());
    return true;
  }
  return false;
}

}  // namespace ash::holding_space_prefs
