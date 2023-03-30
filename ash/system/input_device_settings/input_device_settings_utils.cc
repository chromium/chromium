// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_utils.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/chromeos/events/mojom/modifier_key.mojom.h"

namespace ash {

namespace {
std::string HexEncode(uint16_t v) {
  // Load the bytes into the bytes array in reverse order as hex number should
  // be read from left to right.
  uint8_t bytes[sizeof(uint16_t)];
  bytes[1] = v & 0xFF;
  bytes[0] = v >> 8;
  return base::ToLowerASCII(base::HexEncode(bytes));
}
}  // namespace

// `kIsoLevel5ShiftMod3` is not a valid modifier value.
bool IsValidModifier(int val) {
  return val >= static_cast<int>(ui::mojom::ModifierKey::kMinValue) &&
         val <= static_cast<int>(ui::mojom::ModifierKey::kMaxValue) &&
         val != static_cast<int>(ui::mojom::ModifierKey::kIsoLevel5ShiftMod3);
}

std::string BuildDeviceKey(const ui::InputDevice& device) {
  return base::StrCat(
      {HexEncode(device.vendor_id), ":", HexEncode(device.product_id)});
}

}  // namespace ash
