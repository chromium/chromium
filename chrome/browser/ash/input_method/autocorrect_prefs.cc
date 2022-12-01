// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/autocorrect_prefs.h"

#include "base/strings/strcat.h"
#include "chrome/common/pref_names.h"

namespace ash::input_method {
namespace {

constexpr char kAutocorrectLevelPreferenceName[] =
    "physicalKeyboardAutoCorrectionLevel";

}  // namespace

AutocorrectPreference GetPhysicalKeyboardAutocorrectPref(
    const PrefService& pref_service,
    const std::string& engine_id) {
  const base::Value::Dict& input_method_settings =
      pref_service.GetDict(prefs::kLanguageInputMethodSpecificSettings);
  const base::Value* autocorrect_level = input_method_settings.FindByDottedPath(
      base::StrCat({engine_id, ".", kAutocorrectLevelPreferenceName}));

  if (!autocorrect_level)
    return AutocorrectPreference::kDefault;
  if (!autocorrect_level->GetIfInt().has_value())
    return AutocorrectPreference::kDefault;
  if (autocorrect_level->GetIfInt().value() > 0)
    return AutocorrectPreference::kEnabled;
  return AutocorrectPreference::kDisabled;
}

}  // namespace ash::input_method
