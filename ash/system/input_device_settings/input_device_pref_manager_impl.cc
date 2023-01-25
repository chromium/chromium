// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_pref_manager_impl.h"

#include <cstdint>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

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

InputDevicePrefManagerImpl::InputDevicePrefManagerImpl() = default;
InputDevicePrefManagerImpl::~InputDevicePrefManagerImpl() = default;

// static
std::string InputDevicePrefManagerImpl::BuildDeviceKey(
    const ui::InputDevice& device) {
  return base::StrCat(
      {HexEncode(device.vendor_id), ":", HexEncode(device.product_id)});
}

// TODO(dpad): Implement retrieval of settings when keyboard is initially
// connected.
void InputDevicePrefManagerImpl::InitializeKeyboardSettings(
    mojom::Keyboard* keyboard) {
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    return;
  }
  NOTIMPLEMENTED();
}

}  // namespace ash
