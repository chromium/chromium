// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/device_wallpaper_image_external_data_handler.h"

#include <utility>

#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

DeviceWallpaperImageExternalDataHandler::
    DeviceWallpaperImageExternalDataHandler(PrefService* local_state,
                                            PolicyService* policy_service)
    : local_state_(local_state),
      device_wallpaper_image_observer_(
          std::make_unique<DeviceCloudExternalDataPolicyObserver>(
              policy_service,
              key::kDeviceWallpaperImage,
              this)) {}

DeviceWallpaperImageExternalDataHandler::
    ~DeviceWallpaperImageExternalDataHandler() = default;

// static
void DeviceWallpaperImageExternalDataHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceWallpaperImageFilePath,
                               std::string());
}

void DeviceWallpaperImageExternalDataHandler::OnDeviceExternalDataCleared(
    const std::string& policy) {
  local_state_->SetString(prefs::kDeviceWallpaperImageFilePath, std::string());
}

void DeviceWallpaperImageExternalDataHandler::OnDeviceExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  local_state_->SetString(prefs::kDeviceWallpaperImageFilePath,
                          file_path.value());
}

void DeviceWallpaperImageExternalDataHandler::Shutdown() {
  device_wallpaper_image_observer_.reset();
}

}  // namespace policy
