// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_key_alias_manager.h"

#include <string_view>

#include "ash/system/input_device_settings/input_device_settings_utils.h"

namespace ash {
namespace {

// Device Key - Built using the VID/PID for the device "<vid>:<pid>"
// Contains entries in the form of <aliased_key, primary_key> for devices that
// report different PID/VIDs depending on the connection method used
// (Bluetooth/USB). The device key reported when connected via USB will be
// used as the primary key for a set of keys belonging to a device.
static constexpr std::pair<const char*, const char*>
    kAliasToPrimaryDeviceKeyMap[] = {
        // Apple Magic Keyboard with Numeric Pad {Bluetooth, USB}
        {"004c:026c", "05ac:026c"},
        {"046d:b35b", "046d:408a"}  // Logitech MX Keys {Bluetooth, Receiver}
};

}  // namespace

InputDeviceKeyAliasManager::InputDeviceKeyAliasManager() {
  for (const auto& [aliased_key, primary_key] : kAliasToPrimaryDeviceKeyMap) {
    AddDeviceKeyPair(primary_key, aliased_key);
  }
}

InputDeviceKeyAliasManager::~InputDeviceKeyAliasManager() = default;

std::string InputDeviceKeyAliasManager::GetAliasedDeviceKey(
    const ui::InputDevice& device) {
  return GetAliasedDeviceKey(device.vendor_id, device.product_id);
}

std::string InputDeviceKeyAliasManager::GetAliasedDeviceKey(
    uint16_t vendor_id,
    uint16_t product_id) {
  const std::string key = BuildDeviceKey(vendor_id, product_id);
  const auto it = alias_to_primary_key_map_.find(key);
  return it == alias_to_primary_key_map_.end() ? key : it->second;
}

const base::flat_set<std::string>*
InputDeviceKeyAliasManager::GetAliasesForPrimaryDeviceKey(
    std::string_view primary_device_key) const {
  const auto it = primary_key_to_aliases_map_.find(primary_device_key);
  return it == primary_key_to_aliases_map_.end() ? nullptr : &it->second;
}

void InputDeviceKeyAliasManager::AddDeviceKeyPair(
    const std::string& primary_key,
    const std::string& aliased_key) {
  primary_key_to_aliases_map_[primary_key].insert(aliased_key);
  alias_to_primary_key_map_.emplace(aliased_key, primary_key);
}

}  // namespace ash
