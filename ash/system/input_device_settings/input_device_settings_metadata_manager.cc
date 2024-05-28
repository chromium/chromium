// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata_manager.h"

#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

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
    ImageDownloadCallback callback) {
  GetDeviceImagePreferringCache(device_key, account_id, std::move(callback));
}

// TODO(b/329686601): Notify any observers that a new image is available.
// TODO(b/329686601): Implement error handling for cases where the image
// download fails.
void InputDeviceSettingsMetadataManager::OnDeviceImageFetched(
    const DeviceImage& device_image) {
  const auto device_key = device_image.device_key();
  device_image_storage_->PersistDeviceImage(device_key,
                                            device_image.data_url());
  std::move(device_callback_map_[device_key]).Run(device_image);
  device_callback_map_.erase(device_key);
}

std::optional<std::string>
InputDeviceSettingsMetadataManager::GetCachedDeviceImageDataUri(
    const std::string& device_key) {
  return device_image_storage_->GetImage(device_key);
}

void InputDeviceSettingsMetadataManager::GetDeviceImagePreferringCache(
    const std::string& device_key,
    const AccountId& account_id,
    ImageDownloadCallback callback) {
  const auto device_image = device_image_storage_->GetImage(device_key);
  if (device_image.has_value()) {
    std::move(callback).Run(DeviceImage(device_key, device_image.value()));
    return;
  }
  device_callback_map_[device_key] = std::move(callback);
  image_downloader_->DownloadImage(
      device_key, account_id,
      base::BindOnce(&InputDeviceSettingsMetadataManager::OnDeviceImageFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash
