// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_STORAGE_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_STORAGE_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"

namespace ash {

// Provides persistent storage for device image data, allowing retrieval
// and saving of image data URLs based on device keys.
class ASH_EXPORT DeviceImageStorage {
 public:
  // This function retrieves the image data URL associated with a specific
  // device from the user's stored preferences. It returns an std::optional
  // to handle cases where the image may not be found
  std::optional<std::string> GetImage(const std::string& device_key) const;
  // Saves the image data URL (base64-encoded representation) into the local
  // state prefs.
  bool PersistDeviceImage(const std::string& device_key,
                          const std::string& data_url);
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_STORAGE_H_
