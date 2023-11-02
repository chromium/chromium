// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_PREFS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_PREFS_H_

#include <string>

#include "components/prefs/pref_service.h"

namespace ash {
namespace input_method {

bool IsPredictiveWritingPrefEnabled(PrefService* pref_service,
                                    const std::string& engine_id);

bool IsDiacriticsOnLongpressPrefEnabled(PrefService* pref_service,
                                        const std::string& engine_id);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_ASSISTIVE_SUGGESTER_PREFS_H_
