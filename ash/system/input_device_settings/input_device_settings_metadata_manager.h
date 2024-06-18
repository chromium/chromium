// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_MANAGER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/device_image.h"
#include "ash/system/input_device_settings/device_image_downloader.h"
#include "ash/system/input_device_settings/device_image_storage.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"

class PrefRegistrySimple;

namespace ash {

enum class DeviceImageDestination;

// Handles input device metadata (images, app info) and updates components on
// changes.
class ASH_EXPORT InputDeviceSettingsMetadataManager {
 public:
  using ImageDownloadCallback =
      base::OnceCallback<void(const DeviceImage& image)>;

  InputDeviceSettingsMetadataManager();
  InputDeviceSettingsMetadataManager(
      std::unique_ptr<DeviceImageDownloader> image_downloader,
      std::unique_ptr<DeviceImageStorage> image_storage);
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
                      DeviceImageDestination destination,
                      ImageDownloadCallback callback);
  // Retrieves the image data URI for the input device if it exists.
  std::optional<std::string> GetCachedDeviceImageDataUri(
      const std::string& device_key);

  const base::flat_map<std::string, std::vector<ImageDownloadCallback>>&
  GetDeviceCallbackMapForTesting() {
    return device_callback_map_;
  }

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  // Callback function called when a device image is fetched by the downloader.
  // Handles storing the image and notifying any pending callbacks.
  void OnDeviceImageFetched(DeviceImageDestination destination,
                            const DeviceImage& device_image);
  // Attempts to load the device image from disk before making a network
  // request to download the device image.
  void GetDeviceImagePreferringCache(const std::string& device_key,
                                     const AccountId& account_id,
                                     DeviceImageDestination destination,
                                     ImageDownloadCallback callback);

  std::unique_ptr<DeviceImageDownloader> image_downloader_;
  std::unique_ptr<DeviceImageStorage> device_image_storage_;
  // Tracks image download requests for input devices. Maps a device's unique
  // key to the callback that should be executed when the image is downloaded.
  base::flat_map<std::string, std::vector<ImageDownloadCallback>>
      device_callback_map_;
  base::WeakPtrFactory<InputDeviceSettingsMetadataManager> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METADATA_MANAGER_H_
