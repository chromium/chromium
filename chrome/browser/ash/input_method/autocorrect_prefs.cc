// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_prefs.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/ash/input_method/input_method_settings.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::input_method {
namespace {

constexpr char kPkAutocorrectLevelPrefName[] =
    "physicalKeyboardAutoCorrectionLevel";
constexpr char kVkAutocorrectLevelPrefName[] =
    "virtualKeyboardAutoCorrectionLevel";
constexpr char kPkEnabledByDefaultPrefName[] =
    "physicalKeyboardAutoCorrectionEnabledByDefault";

AutocorrectPreference GetAutocorrectPrefFor(
    const std::string& autocorrect_pref_path,
    const PrefService& pref_service,
    const std::string& engine_id) {
  const base::Value::Dict& input_method_settings =
      pref_service.GetDict(prefs::kLanguageInputMethodSpecificSettings);
  const base::Value* autocorrect_level = input_method_settings.FindByDottedPath(
      base::StrCat({engine_id, ".", autocorrect_pref_path}));

  if (!autocorrect_level) {
    return AutocorrectPreference::kDefault;
  }
  if (!autocorrect_level->GetIfInt().has_value()) {
    return AutocorrectPreference::kDefault;
  }
  if (autocorrect_level->GetIfInt().value() > 0) {
    return AutocorrectPreference::kEnabled;
  }
  return AutocorrectPreference::kDisabled;
}

bool IsPkAutocorrectEnabledByDefault(const PrefService& pref_service,
                                     const std::string& engine_id) {
  if (!base::FeatureList::IsEnabled(features::kAutocorrectByDefault)) {
    return false;
  }

  const base::Value::Dict& settings =
      pref_service.GetDict(prefs::kLanguageInputMethodSpecificSettings);
  const base::Value* enabled_by_default = settings.FindByDottedPath(
      base::StrCat({engine_id, ".", kPkEnabledByDefaultPrefName}));

  return (enabled_by_default && enabled_by_default->GetIfBool().has_value() &&
          enabled_by_default->GetIfBool().value());
}

}  // namespace

AutocorrectPreference GetPhysicalKeyboardAutocorrectPref(
    const PrefService& pref_service,
    const std::string& engine_id) {
  if (!IsPhysicalKeyboardAutocorrectAllowed(pref_service)) {
    return AutocorrectPreference::kDisabled;
  }
  auto preference = GetAutocorrectPrefFor(kPkAutocorrectLevelPrefName,
                                          pref_service, engine_id);
  if (!base::FeatureList::IsEnabled(features::kAutocorrectByDefault)) {
    return preference;
  }
  return (preference == AutocorrectPreference::kDefault &&
          IsPkAutocorrectEnabledByDefault(pref_service, engine_id))
             ? AutocorrectPreference::kEnabledByDefault
             : preference;
}

AutocorrectPreference GetVirtualKeyboardAutocorrectPref(
    const PrefService& pref_service,
    const std::string& engine_id) {
  return GetAutocorrectPrefFor(kVkAutocorrectLevelPrefName, pref_service,
                               engine_id);
}

bool SetPhysicalKeyboardAutocorrectAsEnabledByDefault(
    PrefService* pref_service,
    const std::string& engine_id) {
  if (!base::FeatureList::IsEnabled(features::kAutocorrectByDefault)) {
    return false;
  }
  base::Value* result =
      ScopedDictPrefUpdate(pref_service,
                           prefs::kLanguageInputMethodSpecificSettings)
          ->SetByDottedPath(
              base::StrCat({engine_id, ".", kPkEnabledByDefaultPrefName}),
              base::Value(true));
  return result != nullptr;
}

}  // namespace ash::input_method
