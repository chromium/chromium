// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_LEGACY_CONFIG_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_LEGACY_CONFIG_H_

#include "base/containers/fixed_flat_map.h"
#include "base/values.h"
#include "chromeos/ash/services/ime/public/mojom/user_data_japanese_legacy_config.mojom.h"

namespace ash::input_method {

// Fills a dictionary with all the prefs that are set in the
// JapaneseLegacyConfig using the prefs constants expected by the settings app.
base::Value::Dict CreatePrefsDictFromJapaneseLegacyConfig(
    ash::ime::mojom::JapaneseLegacyConfigPtr legacy_config);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_LEGACY_CONFIG_H_
