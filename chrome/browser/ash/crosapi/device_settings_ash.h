// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DEVICE_SETTINGS_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DEVICE_SETTINGS_ASH_H_

#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implements the crosapi device settings interface. Lives in ash-chrome.
// Allows lacros-chrome to access device settings that live in ash.
class DeviceSettingsAsh : public mojom::DeviceSettingsService,
                          public ash::DeviceSettingsService::Observer {
 public:
  DeviceSettingsAsh();
  DeviceSettingsAsh(const DeviceSettingsAsh&) = delete;
  DeviceSettingsAsh& operator=(const DeviceSettingsAsh&) = delete;
  ~DeviceSettingsAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::DeviceSettingsService> receiver);

  // ash::DeviceSettingsService::Observer
  void DeviceSettingsUpdated() override;

  // crosapi::mojom::DeviceSettingsService:
  void AddDeviceSettingsObserver(
      mojo::PendingRemote<mojom::DeviceSettingsObserver> observer) override;
  void GetDevicePolicy(GetDevicePolicyCallback callback) override;
  void GetDevicePolicyDeprecated(
      GetDevicePolicyDeprecatedCallback callback) override;
  void GetDeviceReportSources(GetDeviceReportSourcesCallback callback) override;
  void IsDeviceDeprovisioned(IsDeviceDeprovisionedCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::DeviceSettingsService> receivers_;

  // Support any number of device settings observers.
  mojo::RemoteSet<mojom::DeviceSettingsObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DEVICE_SETTINGS_ASH_H_
