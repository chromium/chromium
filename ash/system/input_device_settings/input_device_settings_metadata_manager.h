// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_MANAGER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/device_image.h"
#include "ash/system/input_device_settings/device_image_downloader.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"

namespace ash {

// Handles input device metadata (images, app info) and updates components on
// changes.
class ASH_EXPORT InputDeviceSettingsMetadataManager {
 public:
  using ImageDownloadCallback =
      base::OnceCallback<void(const DeviceImage& image)>;

  InputDeviceSettingsMetadataManager();
  explicit InputDeviceSettingsMetadataManager(
      std::unique_ptr<DeviceImageDownloader> image_downloader);
  InputDeviceSettingsMetadataManager(
      const InputDeviceSettingsMetadataManager&) = delete;
  InputDeviceSettingsMetadataManager& operator=(
      const InputDeviceSettingsMetadataManager&) = delete;
  ~InputDeviceSettingsMetadataManager();

  // Gets the image associated with the input device specified by
  // `device_key`. Initiates a download for the image using the
  // ImageDownloader.
  void GetDeviceImage(const std::string& device_key,
                      const AccountId& account_id,
                      ImageDownloadCallback callback);

 private:
  std::unique_ptr<DeviceImageDownloader> image_downloader_;
  base::WeakPtrFactory<InputDeviceSettingsMetadataManager> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_MANAGER_H_
