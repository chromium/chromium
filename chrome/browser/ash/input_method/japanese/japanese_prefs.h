// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_PREFS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_PREFS_H_

#include "base/containers/fixed_flat_map.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"

namespace ash::input_method {

void SetJpOptionsSourceAsPrefService(PrefService& prefs);

void SetJpOptionsSourceAsLegacyConfig(PrefService& prefs);

bool ShouldInitializeJpPrefsFromLegacyConfig(PrefService& prefs);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_PREFS_H_
