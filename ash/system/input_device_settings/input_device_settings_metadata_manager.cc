// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata_manager.h"

#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/containers/contains.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace {

std::string GenerateImageRequestKey(const std::string& key,
                                    DeviceImageDestination destination) {
  return key + "_" + base::NumberToString(static_cast<int>(destination));
}

}  // namespace

InputDeviceSettingsMetadataManager::InputDeviceSettingsMetadataManager(
    std::unique_ptr<DeviceImageDownloader> image_downloader,
    std::unique_ptr<DeviceImageStorage> image_storage)
    : image_downloader_(std::move(image_downloader)),
      device_image_storage_(std::move(image_storage)) {}

InputDeviceSettingsMetadataManager::InputDeviceSettingsMetadataManager()
    : InputDeviceSettingsMetadataManager(
          std::make_unique<DeviceImageDownloader>(),
          std::make_unique<DeviceImageStorage>()) {}

InputDeviceSettingsMetadataManager::~InputDeviceSettingsMetadataManager() =
    default;

// static
void InputDeviceSettingsMetadataManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDeviceImagesDictPref);
}

void InputDeviceSettingsMetadataManager::GetDeviceImage(
    const std::string& device_key,
    const AccountId& account_id,
    DeviceImageDestination destination,
    ImageDownloadCallback callback) {
  GetDeviceImagePreferringCache(device_key, account_id, destination,
                                std::move(callback));
}

void InputDeviceSettingsMetadataManager::OnDeviceImageFetched(
    DeviceImageDestination destination,
    const DeviceImage& device_image) {
  const auto device_key = device_image.device_key();
  // Check if the device image is specifically for settings and is a valid
  // image.
  if (destination == DeviceImageDestination::kSettings &&
      device_image.IsValid()) {
    // Save the device image data (represented as a data URL) to persistent
    // storage, using the device_key as a unique identifier.
    device_image_storage_->PersistDeviceImage(device_key,
                                              device_image.data_url());
  }
  auto it = device_callback_map_.find(
      GenerateImageRequestKey(device_key, destination));

  if (it == device_callback_map_.end()) {
    return;
  }

  for (auto& callback : it->second) {
    std::move(callback).Run(device_image);
  }
  device_callback_map_.erase(it);
}

std::optional<std::string>
InputDeviceSettingsMetadataManager::GetCachedDeviceImageDataUri(
    const std::string& device_key) {
  return device_image_storage_->GetImage(device_key);
}

void InputDeviceSettingsMetadataManager::GetDeviceImagePreferringCache(
    const std::string& device_key,
    const AccountId& account_id,
    DeviceImageDestination destination,
    ImageDownloadCallback callback) {
  const auto device_image = device_image_storage_->GetImage(device_key);
  if (device_image.has_value()) {
    std::move(callback).Run(DeviceImage(device_key, device_image.value()));
    return;
  }
  device_callback_map_[GenerateImageRequestKey(device_key, destination)]
      .push_back(std::move(callback));
  image_downloader_->DownloadImage(
      device_key, account_id, destination,
      base::BindOnce(&InputDeviceSettingsMetadataManager::OnDeviceImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), destination));
}

}  // namespace ash
