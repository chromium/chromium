// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata_manager.h"

#include "ash/system/input_device_settings/device_image_downloader.h"

namespace ash {

InputDeviceSettingsMetadataManager::InputDeviceSettingsMetadataManager()
    : InputDeviceSettingsMetadataManager(
          std::make_unique<DeviceImageDownloader>()) {}

InputDeviceSettingsMetadataManager::InputDeviceSettingsMetadataManager(
    std::unique_ptr<DeviceImageDownloader> image_downloader)
    : image_downloader_(std::move(image_downloader)) {}

InputDeviceSettingsMetadataManager::~InputDeviceSettingsMetadataManager() =
    default;

void InputDeviceSettingsMetadataManager::GetDeviceImage(
    const std::string& device_key,
    ImageDownloadCallback callback) {
  image_downloader_->DownloadImage(device_key, std::move(callback));
}

}  // namespace ash
