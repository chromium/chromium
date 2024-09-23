// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_PREFS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_PREFS_H_

#include <string>

#include "chrome/browser/ash/input_method/suggester.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace input_method {

bool IsPredictiveWritingPrefEnabled(const PrefService& pref_service,
                                    const std::string& engine_id);

bool IsDiacriticsOnLongpressPrefEnabled(PrefService* pref_service,
                                        const std::string& engine_id);

int GetPrefValue(const std::string& pref_name, Profile& profile);

// Increment int value for the given pref_name by 1 every time the function is
// called. The function has no effect after the int value becomes equal to the
// max_value.
void IncrementPrefValueUntilCapped(const std::string& pref_name,
                                   int max_value,
                                   Profile& profile);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_PREFS_H_
