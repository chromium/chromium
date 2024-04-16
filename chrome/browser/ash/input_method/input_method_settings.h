// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_H_

#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace input_method {

ime::mojom::InputMethodSettingsPtr CreateSettingsFromPrefs(
    const PrefService& prefs,
    const std::string& engine_id);

// Adds or replaces the settings in the values map while notifying the
// PrefService of changes.
void SetLanguageInputMethodSpecificSetting(PrefService& prefs,
                                           const std::string& engine_id,
                                           const base::Value::Dict& values);

// Gets a specific settings value that is held under a key for an engine id if
// it exists.
// Will return nullptr if it does not exist.
const base::Value* GetLanguageInputMethodSpecificSetting(
    PrefService& prefs,
    const std::string& engine_id,
    const std::string& preference_name);

// Returns true if Autocorrect is supported for a given engine id.
bool IsAutocorrectSupported(const std::string& engine_id);

// Is the physical keyboard autocorrect feature allowed for this device (if the
// device is managed).
bool IsPhysicalKeyboardAutocorrectAllowed(const PrefService& prefs);

// Is the physical keyboard predictive writing feature allowed for this device
// (if the device is managed).
bool IsPhysicalKeyboardPredictiveWritingAllowed(const PrefService& prefs);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_H_
