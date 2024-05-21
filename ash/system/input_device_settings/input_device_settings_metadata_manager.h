// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_MANAGER_H_

#include "ash/ash_export.h"

namespace ash {

// Handles input device metadata (images, app info) and updates components on
// changes.
class ASH_EXPORT InputDeviceSettingsMetadataManager {
 public:
  InputDeviceSettingsMetadataManager();
  InputDeviceSettingsMetadataManager(
      const InputDeviceSettingsMetadataManager&) = delete;
  InputDeviceSettingsMetadataManager& operator=(
      const InputDeviceSettingsMetadataManager&) = delete;
  ~InputDeviceSettingsMetadataManager();
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_MANAGER_H_
