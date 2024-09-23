// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_KEY_ALIAS_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_KEY_ALIAS_MANAGER_H_

#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "ui/events/devices/input_device.h"

namespace ash {

using PrimaryDeviceKeyToAliasesMap =
    base::flat_map<std::string, base::flat_set<std::string>>;
using AliasToPrimaryDeviceKeyMap = base::flat_map<std::string, std::string>;

class ASH_EXPORT InputDeviceKeyAliasManager {
 public:
  InputDeviceKeyAliasManager();
  InputDeviceKeyAliasManager(const InputDeviceKeyAliasManager&) = delete;
  InputDeviceKeyAliasManager& operator=(const InputDeviceKeyAliasManager&) =
      delete;
  ~InputDeviceKeyAliasManager();
  // Builds the device key for `device` and checks to see if it's an alias
  // to a device's primary key or not before returning.
  std::string GetAliasedDeviceKey(const ui::InputDevice& device);
  // Builds the device key based on vendor and product IDs and checks to see
  // if it's an alias to a device's primary key or not before returning.
  std::string GetAliasedDeviceKey(uint16_t vendor_id, uint16_t product_id);
  // Uses `primary_device_key` to retrieve all aliased device keys for a given
  // device.
  const base::flat_set<std::string>* GetAliasesForPrimaryDeviceKey(
      std::string_view primary_device_key) const;

  void AddDeviceKeyPair(const std::string& primary_key,
                        const std::string& aliased_key);

 private:
  PrimaryDeviceKeyToAliasesMap primary_key_to_aliases_map_;
  AliasToPrimaryDeviceKeyMap alias_to_primary_key_map_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_KEY_ALIAS_MANAGER_H_
