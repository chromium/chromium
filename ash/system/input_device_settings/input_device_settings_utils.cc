// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_utils.h"

#include "ash/public/mojom/input_device_settings.mojom.h"

namespace ash {

bool IsValidModifier(int val) {
  return val >= static_cast<int>(mojom::ModifierKey::kMinValue) &&
         val <= static_cast<int>(mojom::ModifierKey::kMaxValue);
}

}  // namespace ash
