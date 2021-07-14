// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_NAME_STORE_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_NAME_STORE_H_

#include <string>

#include "base/gtest_prod_util.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// DeviceNameStore is a device-persistent model of the device name which
// clients use to display the device name to the user and broadcast the device
// name to local networks (e.g., the DHCP hostname). Clients also have the
// ability to set the device name (e.g., OS Settings).
//
// An initial device name is created on first boot and is set to the default
// name 'ChromeOS'.
//
// DeviceNameStore is responsible for updating the system state to reflect
// changes to the device name, e.g., setting the hostname via Shill.
//
// Must only be used on the UI thread.
class DeviceNameStore {
 public:
  // Returns a pointer to the singleton instance for the current process.
  // Should only be called after Initialize().
  static DeviceNameStore* GetInstance();

  // Register the pref used to store the device name in the local state.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Checks the local state for a device name to store if it exists; otherwise,
  // creates a new device name and persists it. Must be called before any other
  // non-static method on DeviceNameStore.
  // |prefs| is the PrefService used to persist and read the device name value.
  static void Initialize(PrefService* prefs);

  // Shutdown() should be called to destroy the instance once its clients no
  // longer need it.
  static void Shutdown();

  std::string GetDeviceName() const;

 private:
  friend class DeviceNameStoreTest;

  explicit DeviceNameStore(PrefService* prefs);
  virtual ~DeviceNameStore();
  DeviceNameStore(const DeviceNameStore&) = delete;
  DeviceNameStore& operator=(const DeviceNameStore&) = delete;

  // Provides access and persistence for the device name value.
  PrefService* prefs_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_NAME_STORE_H_
