// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/pref_change_recorder.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/ash/input_method/autocorrect_prefs.h"
#include "chrome/common/pref_names.h"

namespace ash::input_method {
namespace {

constexpr char kUsEnglish[] = "xkb:us::eng";

using AutocorrectPrefDetails = PrefChangeRecorder::AutocorrectPrefDetails;
using AutocorrectPrefs = PrefChangeRecorder::AutocorrectPrefs;
using KeyboardType = PrefChangeRecorder::KeyboardType;

struct AutocorrectPrefChange {
  AutocorrectPrefDetails details;
  AutocorrectPreference previous_value;
  AutocorrectPreference new_value;
};

// Extracts all autocorrect preference values from the input method options
// dictionary blob. Each preference will be keyed to a single engine id, with
// exactly two preferences per engine id for VK and PK.
AutocorrectPrefs ExtractAutocorrectPrefs(PrefService* pref_service) {
  AutocorrectPrefs autocorrect_prefs;

  for (const auto [engine_id, _] :
       pref_service->GetDict(prefs::kLanguageInputMethodSpecificSettings)) {
    autocorrect_prefs.insert(
        {base::StrCat({engine_id, ".VirtualKeyboard"}),
         AutocorrectPrefDetails{
             /*engine_id=*/engine_id,
             /*keyboard_type=*/KeyboardType::kVirtualKeyboard,
             /*preference=*/
             GetVirtualKeyboardAutocorrectPref(*(pref_service), engine_id)}});

    autocorrect_prefs.insert(
        {base::StrCat({engine_id, ".PhysicalKeyboard"}),
         AutocorrectPrefDetails{
             /*engine_id=*/engine_id,
             /*keyboard_type=*/KeyboardType::kPhysicalKeyboard,
             /*preference=*/
             GetPhysicalKeyboardAutocorrectPref(*(pref_service), engine_id)}});
  }

  return autocorrect_prefs;
}

// Iterate over two sets of autocorrect preferences and find the first
// preference where its respective value differs between the two sets. This
// function will be called once per setting change, so we can assume there
// would be at most one preference with different values in the two sets.
std::optional<AutocorrectPrefChange> FindPrefChange(
    const AutocorrectPrefs& previous,
    const AutocorrectPrefs& current) {
  for (const auto& [key, details] : current) {
    const auto find_it = previous.find(key);

    if (find_it == previous.end() &&
        details.preference != AutocorrectPreference::kDefault) {
      return AutocorrectPrefChange{
          /*details=*/details,
          /*previous_value=*/AutocorrectPreference::kDefault,
          /*new_value=*/details.preference};
    }

    if (find_it != previous.end()) {
      const AutocorrectPrefDetails& previous_details = find_it->second;
      if (previous_details.preference != details.preference) {
        return AutocorrectPrefChange{
            /*details=*/details,
            /*previous_value=*/previous_details.preference,
            /*new_value=*/details.preference};
      }
    }
  }

  return std::nullopt;
}

AutocorrectPrefStateTransition MapToAutocorrectPrefStateTransition(
    AutocorrectPreference previous_value,
    AutocorrectPreference new_value) {
  if (previous_value == AutocorrectPreference::kDefault &&
      new_value == AutocorrectPreference::kDisabled) {
    return AutocorrectPrefStateTransition::kDefaultToDisabled;
  }

  if (previous_value == AutocorrectPreference::kDefault &&
      new_value == AutocorrectPreference::kEnabled) {
    return AutocorrectPrefStateTransition::kDefaultToEnabled;
  }

  if (previous_value == AutocorrectPreference::kDisabled &&
      new_value == AutocorrectPreference::kEnabled) {
    return AutocorrectPrefStateTransition::kDisabledToEnabled;
  }

  if (previous_value == AutocorrectPreference::kEnabled &&
      new_value == AutocorrectPreference::kDisabled) {
    return AutocorrectPrefStateTransition::kEnabledToDisabled;
  }

  if (previous_value == AutocorrectPreference::kDefault &&
      new_value == AutocorrectPreference::kEnabledByDefault) {
    return AutocorrectPrefStateTransition::kDefaultToForceEnabled;
  }

  if (previous_value == AutocorrectPreference::kEnabledByDefault &&
      new_value == AutocorrectPreference::kDisabled) {
    return AutocorrectPrefStateTransition::kForceEnabledToDisabled;
  }

  if (previous_value == AutocorrectPreference::kEnabledByDefault &&
      new_value == AutocorrectPreference::kEnabled) {
    return AutocorrectPrefStateTransition::kForceEnabledToEnabled;
  }

  // Note that we do not record kEnabledByDefault to kDefault (this transition
  // would occur in a rampdown of the enabled by default experiment). Recording
  // this transition would require code to run outside of the enabled by default
  // flag (ie remove the enabledByDefault bool from prefs when the experiment
  // flag is disabled), so on the safe side we don't do that.

  return AutocorrectPrefStateTransition::kUnchanged;
}

void RecordAutocorrectPrefChangeMetric(
    const AutocorrectPrefChange& pref_change) {
  AutocorrectPrefStateTransition state_transition =
      MapToAutocorrectPrefStateTransition(pref_change.previous_value,
                                          pref_change.new_value);

  if (pref_change.details.engine_id == kUsEnglish) {
    base::UmaHistogramEnumeration(
        pref_change.details.keyboard_type == KeyboardType::kPhysicalKeyboard
            ? "InputMethod.Assistive.AutocorrectV2.UserPrefChange.English.PK"
            : "InputMethod.Assistive.AutocorrectV2.UserPrefChange.English.VK",
        state_transition);
  }

  base::UmaHistogramEnumeration(
      pref_change.details.keyboard_type == KeyboardType::kPhysicalKeyboard
          ? "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.PK"
          : "InputMethod.Assistive.AutocorrectV2.UserPrefChange.All.VK",
      state_transition);
}

}  // namespace

PrefChangeRecorder::PrefChangeRecorder(PrefService* pref_service)
    : input_method_options_observer_(pref_service),
      autocorrect_prefs_(ExtractAutocorrectPrefs(pref_service)),
      pref_service_(pref_service) {
  input_method_options_observer_.Observe(
      base::BindRepeating(&PrefChangeRecorder::OnInputMethodOptionsChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

PrefChangeRecorder::~PrefChangeRecorder() = default;

void PrefChangeRecorder::OnInputMethodOptionsChanged(
    const std::string& pref_path_changed) {
  AutocorrectPrefs new_autocorrect_prefs =
      ExtractAutocorrectPrefs(pref_service_);

  auto pref_change = FindPrefChange(autocorrect_prefs_, new_autocorrect_prefs);
  if (pref_change) {
    RecordAutocorrectPrefChangeMetric(*pref_change);
  }

  autocorrect_prefs_ = std::move(new_autocorrect_prefs);
}

}  // namespace ash::input_method
