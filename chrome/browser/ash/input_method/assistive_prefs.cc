// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace input_method {

bool IsPredictiveWritingPrefEnabled(PrefService* pref_service,
                                    const std::string& engine_id) {
  const base::Value::Dict& input_method_settings =
      pref_service->GetDict(::prefs::kLanguageInputMethodSpecificSettings);
  absl::optional<bool> predictive_writing_setting =
      input_method_settings.FindBoolByDottedPath(
          engine_id + ".physicalKeyboardEnablePredictiveWriting");
  // If no preference has been set yet by the user then we can assume the
  // default preference as enabled.
  return predictive_writing_setting.value_or(true);
}

bool IsDiacriticsOnLongpressPrefEnabled(PrefService* pref_service,
                                        const std::string& engine_id) {
  const base::Value::Dict& input_method_settings =
      pref_service->GetDict(::prefs::kLanguageInputMethodSpecificSettings);
  absl::optional<bool> diacritics_on_longpress_setting =
      input_method_settings.FindBoolByDottedPath(
          engine_id + ".physicalKeyboardEnableDiacriticsOnLongpress");
  // If no preference has been set yet by the user then we can assume the
  // default preference as enabled.
  return diacritics_on_longpress_setting.value_or(true);
}

int GetPrefValue(const std::string& pref_name, Profile& profile) {
  ScopedDictPrefUpdate update(profile.GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  auto value = update->FindInt(pref_name);
  if (!value.has_value()) {
    update->Set(pref_name, 0);
    return 0;
  }
  return *value;
}

void IncrementPrefValueUntilCapped(const std::string& pref_name,
                                   int max_value,
                                   Profile& profile) {
  int value = GetPrefValue(pref_name, profile);
  if (value < max_value) {
    ScopedDictPrefUpdate update(profile.GetPrefs(),
                                prefs::kAssistiveInputFeatureSettings);
    update->Set(pref_name, value + 1);
  }
}

}  // namespace input_method
}  // namespace ash
