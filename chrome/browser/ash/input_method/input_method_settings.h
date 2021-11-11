// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_H_

#include "ash/services/ime/public/mojom/input_method.mojom.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace input_method {

ime::mojom::InputMethodSettingsPtr CreateSettingsFromPrefs(
    const PrefService& prefs,
    const std::string& engine_id);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_H_
