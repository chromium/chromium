// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_PREFS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_PREFS_H_

#include <string>

#include "chrome/browser/ash/input_method/autocorrect_enums.h"
#include "components/prefs/pref_service.h"

namespace ash::input_method {

// Returns the current user's preference for the autocorrect feature on the
// physical keyboard.
AutocorrectPreference GetPhysicalKeyboardAutocorrectPref(
    const PrefService& pref_service,
    const std::string& engine_id);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_PREFS_H_
