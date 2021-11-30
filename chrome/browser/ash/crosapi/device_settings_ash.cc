// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_settings_ash.h"

#include <utility>

#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

DeviceSettingsAsh::DeviceSettingsAsh() {
  if (ash::DeviceSettingsService::IsInitialized())
    ash::DeviceSettingsService::Get()->AddObserver(this);
}

DeviceSettingsAsh::~DeviceSettingsAsh() {
  if (ash::DeviceSettingsService::IsInitialized())
    ash::DeviceSettingsService::Get()->RemoveObserver(this);
}

void DeviceSettingsAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeviceSettingsService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceSettingsAsh::DeviceSettingsUpdated() {
  for (auto& observer : observers_)
    observer->UpdateDeviceSettings(browser_util::GetDeviceSettings());
}

void DeviceSettingsAsh::AddDeviceSettingsObserver(
    mojo::PendingRemote<mojom::DeviceSettingsObserver> observer) {
  mojo::Remote<mojom::DeviceSettingsObserver> remote(std::move(observer));
  observers_.Add(std::move(remote));
}

}  // namespace crosapi
