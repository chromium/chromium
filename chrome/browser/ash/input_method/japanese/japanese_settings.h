// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_SETTINGS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_SETTINGS_H_
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"

namespace ash::input_method {

ash::ime::mojom::JapaneseSettingsPtr ToMojomInputMethodSettings(
    const base::Value::Dict& prefs_dict);

void RecordJapaneseSettingsMetrics(
    const ash::ime::mojom::JapaneseSettings& settings);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_SETTINGS_H_
