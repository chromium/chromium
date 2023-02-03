// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/check.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

namespace ash {

PointingStickPrefHandlerImpl::PointingStickPrefHandlerImpl() = default;
PointingStickPrefHandlerImpl::~PointingStickPrefHandlerImpl() = default;

// TODO(michaelcheco): Implement pointing stick settings initialization.
void PointingStickPrefHandlerImpl::InitializePointingStickSettings(
    PrefService* pref_service,
    mojom::PointingStick* pointing_stick) {
  NOTIMPLEMENTED();
}

void PointingStickPrefHandlerImpl::UpdatePointingStickSettings(
    PrefService* pref_service,
    const mojom::PointingStick& pointing_stick) {
  // Populate `settings_dict` with all settings in `settings`.
  const mojom::PointingStickSettings& settings = *pointing_stick.settings;

  DCHECK(pointing_stick.settings);
  base::Value::Dict settings_dict;
  settings_dict.Set(prefs::kPointingStickSettingSwapRight, settings.swap_right);
  settings_dict.Set(prefs::kPointingStickSettingSensitivity,
                    settings.sensitivity);
  settings_dict.Set(prefs::kPointingStickSettingAcceleration,
                    settings.acceleration_enabled);

  // Retrieve old settings and merge with the new ones.
  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kPointingStickDeviceSettingsDictPref)
          .Clone();

  // If an old settings dict already exists for the device, merge the updated
  // settings into the old settings. Otherwise, insert the dict at
  // `pointing_stick.device_key`.
  base::Value::Dict* old_settings_dict =
      devices_dict.FindDict(pointing_stick.device_key);
  if (old_settings_dict) {
    old_settings_dict->Merge(std::move(settings_dict));
  } else {
    devices_dict.Set(pointing_stick.device_key, std::move(settings_dict));
  }

  pref_service->SetDict(
      std::string(prefs::kPointingStickDeviceSettingsDictPref),
      std::move(devices_dict));
}

}  // namespace ash
