// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DEVICE_SETTINGS_LACROS_H_
#define CHROME_BROWSER_LACROS_DEVICE_SETTINGS_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// The keeper of device settings needed for Lacros. Initializes with current
// value at startup and receives the updates from ash when the settings are
// changed. Lacros should use the device settings provided by this class when
// needs to use any device settings.
class DeviceSettingsLacros : public crosapi::mojom::DeviceSettingsObserver {
 public:
  DeviceSettingsLacros();
  DeviceSettingsLacros(const DeviceSettingsLacros&) = delete;
  DeviceSettingsLacros& operator=(const DeviceSettingsLacros&) = delete;
  ~DeviceSettingsLacros() override;

  crosapi::mojom::DeviceSettings* GetDeviceSettings();

  // crosapi::mojom::DeviceSettingsObserver:
  void UpdateDeviceSettings(
      crosapi::mojom::DeviceSettingsPtr device_settings) override;

 private:
  void Init();

  crosapi::mojom::DeviceSettingsPtr device_settings_;
  mojo::Receiver<crosapi::mojom::DeviceSettingsObserver> receiver_{this};
  base::WeakPtrFactory<DeviceSettingsLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_DEVICE_SETTINGS_LACROS_H_
