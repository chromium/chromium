// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DEVICE_SETTINGS_LACROS_H_
#define CHROME_BROWSER_LACROS_DEVICE_SETTINGS_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// The keeper of device settings needed for Lacros. Initializes with current
// value at startup and receives the updates from ash when the settings are
// changed. Lacros should use the device settings provided by this class when
// needs to use any device settings.
class DeviceSettingsLacros : public crosapi::mojom::DeviceSettingsObserver {
 public:
  // Observer that is notified on certain events like device settings updates in
  // Ash.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Triggered when device settings are updated,
    virtual void OnDeviceSettingsUpdated() {}
  };

  DeviceSettingsLacros();
  DeviceSettingsLacros(const DeviceSettingsLacros&) = delete;
  DeviceSettingsLacros& operator=(const DeviceSettingsLacros&) = delete;
  ~DeviceSettingsLacros() override;

  // Returns device settings that were retrieved from Ash via crosapi. Needs to
  // be accessed in a valid sequence for thread safety.
  crosapi::mojom::DeviceSettings* GetDeviceSettings();

  // crosapi::mojom::DeviceSettingsObserver:
  // Updated device settings as they are recorded in Ash. Needs to run in a
  // valid sequence for thread safety.
  void UpdateDeviceSettings(
      crosapi::mojom::DeviceSettingsPtr device_settings) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void Init();

  SEQUENCE_CHECKER(sequence_checker_);

  crosapi::mojom::DeviceSettingsPtr device_settings_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<DeviceSettingsLacros::Observer> observers_;

  mojo::Receiver<crosapi::mojom::DeviceSettingsObserver> receiver_{this};
  base::WeakPtrFactory<DeviceSettingsLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_DEVICE_SETTINGS_LACROS_H_
