// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_KEY_ALIAS_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_KEY_ALIAS_MANAGER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT InputDeviceKeyAliasManager {
 public:
  InputDeviceKeyAliasManager();
  InputDeviceKeyAliasManager(const InputDeviceKeyAliasManager&) = delete;
  InputDeviceKeyAliasManager& operator=(const InputDeviceKeyAliasManager&) =
      delete;
  ~InputDeviceKeyAliasManager();
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_KEY_ALIAS_MANAGER_H_
