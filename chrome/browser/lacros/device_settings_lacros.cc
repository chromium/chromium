// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/device_settings_lacros.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"

DeviceSettingsLacros::DeviceSettingsLacros() {
  device_settings_ =
      chromeos::BrowserParamsProxy::Get()->DeviceSettings().Clone();

  // DeviceSettingsService is not available yet at the time when this is
  // constructed. So, we post it as a task to be executed later.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DeviceSettingsLacros::Init,
                                weak_ptr_factory_.GetWeakPtr()));
}

DeviceSettingsLacros::~DeviceSettingsLacros() = default;

void DeviceSettingsLacros::Init() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::DeviceSettingsService>()) {
    LOG(ERROR) << "DeviceSettingsService not available.";
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::DeviceSettingsService>()
      ->AddDeviceSettingsObserver(
          receiver_.BindNewPipeAndPassRemoteWithVersion());
}

crosapi::mojom::DeviceSettings* DeviceSettingsLacros::GetDeviceSettings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return device_settings_.get();
}

void DeviceSettingsLacros::UpdateDeviceSettings(
    crosapi::mojom::DeviceSettingsPtr device_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_settings_ = std::move(device_settings);

  // Also notify observers.
  for (auto& observer : observers_) {
    observer.OnDeviceSettingsUpdated();
  }
}

void DeviceSettingsLacros::AddObserver(
    DeviceSettingsLacros::Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceSettingsLacros::RemoveObserver(
    DeviceSettingsLacros::Observer* observer) {
  observers_.RemoveObserver(observer);
}
