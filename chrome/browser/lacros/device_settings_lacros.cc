// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/device_settings_lacros.h"

#include <utility>

#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"

DeviceSettingsLacros::DeviceSettingsLacros() {
  device_settings_ =
      chromeos::BrowserInitParams::Get()->device_settings.Clone();

  // DeviceSettingsService is not available yet at the time when this is
  // constructed. So, we post it as a task to be executed later.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DeviceSettingsLacros::Init,
                                weak_ptr_factory_.GetWeakPtr()));
}

DeviceSettingsLacros::~DeviceSettingsLacros() = default;

void DeviceSettingsLacros::Init() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::DeviceSettingsService>()) {
    LOG(ERROR) << "DeviceSettingsService not available.";
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::DeviceSettingsService>()
      ->AddDeviceSettingsObserver(
          receiver_.BindNewPipeAndPassRemoteWithVersion());
}

crosapi::mojom::DeviceSettings* DeviceSettingsLacros::GetDeviceSettings() {
  return device_settings_.get();
}

void DeviceSettingsLacros::UpdateDeviceSettings(
    crosapi::mojom::DeviceSettingsPtr device_settings) {
  device_settings_ = std::move(device_settings);
}
