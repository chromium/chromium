// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/device_image_storage.h"

#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

std::optional<std::string> DeviceImageStorage::GetImage(
    const std::string& device_key) const {
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    LOG(WARNING) << "No shell local state available";
    return std::nullopt;
  }

  const std::string* device_image =
      local_state->GetDict(prefs::kDeviceImagesDictPref).FindString(device_key);

  if (!device_image) {
    return std::nullopt;
  }

  return *device_image;
}

bool DeviceImageStorage::PersistDeviceImage(const std::string& device_key,
                                            const std::string& data_url) {
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    LOG(WARNING) << "No shell local state available";
    return false;
  }
  ScopedDictPrefUpdate device_images(local_state, prefs::kDeviceImagesDictPref);
  if (!device_images->Set(device_key, data_url)) {
    LOG(WARNING) << "Failed to persist image to prefs for device_key: "
                 << device_key;
    return false;
  }
  return true;
}

}  // namespace ash
