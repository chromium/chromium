// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_prefs.h"

#include <string>

#include "base/strings/strcat.h"
#include "chrome/common/pref_names.h"

namespace ash::input_method {
namespace {

constexpr char kPkAutocorrectLevelPrefName[] =
    "physicalKeyboardAutoCorrectionLevel";
constexpr char kVkAutocorrectLevelPrefName[] =
    "virtualKeyboardAutoCorrectionLevel";

AutocorrectPreference GetAutocorrectPrefFor(
    const std::string& autocorrect_pref_path,
    const PrefService& pref_service,
    const std::string& engine_id) {
  const base::Value::Dict& input_method_settings =
      pref_service.GetDict(prefs::kLanguageInputMethodSpecificSettings);
  const base::Value* autocorrect_level = input_method_settings.FindByDottedPath(
      base::StrCat({engine_id, ".", autocorrect_pref_path}));

  if (!autocorrect_level)
    return AutocorrectPreference::kDefault;
  if (!autocorrect_level->GetIfInt().has_value())
    return AutocorrectPreference::kDefault;
  if (autocorrect_level->GetIfInt().value() > 0)
    return AutocorrectPreference::kEnabled;
  return AutocorrectPreference::kDisabled;
}

}  // namespace

AutocorrectPreference GetPhysicalKeyboardAutocorrectPref(
    const PrefService& pref_service,
    const std::string& engine_id) {
  return GetAutocorrectPrefFor(kPkAutocorrectLevelPrefName, pref_service,
                               engine_id);
}

AutocorrectPreference GetVirtualKeyboardAutocorrectPref(
    const PrefService& pref_service,
    const std::string& engine_id) {
  return GetAutocorrectPrefFor(kVkAutocorrectLevelPrefName, pref_service,
                               engine_id);
}

}  // namespace ash::input_method
