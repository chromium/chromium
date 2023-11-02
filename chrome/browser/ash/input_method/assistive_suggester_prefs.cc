// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester_prefs.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
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

}  // namespace input_method
}  // namespace ash
