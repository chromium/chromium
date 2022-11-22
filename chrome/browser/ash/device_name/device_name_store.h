// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_STORE_H_
#define CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_STORE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

class PrefRegistrySimple;
class PrefService;

namespace policy {
class DeviceNamePolicyHandler;
}  // namespace policy

namespace ash {

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
  // Types of results for setting the device name. Numerical values from this
  // enum must stay in sync with the JS enum in device_name_util.js.
  enum class SetDeviceNameResult {
    // Device name was updated successfully.
    kSuccess = 0,

    // Device name change is prohibited by policy. An administrator can choose
    // the device name directly and/or prevent managed users from changing it.
    kProhibitedByPolicy = 1,

    // Non-managed users who are not device owners cannot update the name.
    kNotDeviceOwner = 2,

    // Name must be >0 characters and <=15 characters long and contain only
    // letters, numbers or hyphens. Examples of invalid names include "Chrome
    // OS" (uses a space), "ChromeOS!" (uses an exclamation point),
    // "0123456789012345" (too long), "" (empty string).
    kInvalidName = 3,
  };

  // Types of states for the current device name. Numerical values from this
  // enum must stay in sync with the JS enum in device_name_util.js.
  enum class DeviceNameState {
    // Device name can be modified.
    kCanBeModified = 0,

    // Device name change is prohibited by policy. An administrator can choose
    // the device name directly and/or prevent managed users from changing it.
    kCannotBeModifiedBecauseOfPolicy = 1,

    // Non-managed users who are not device owners cannot modify the name.
    kCannotBeModifiedBecauseNotDeviceOwner = 2,
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the device name and/or state changes. Use
    // GetDeviceNameMetadata() to get the new device name and state.
    virtual void OnDeviceNameMetadataChanged() = 0;
  };

  // Contains the device name and whether device name change is possible.
  struct DeviceNameMetadata {
    std::string device_name;
    DeviceNameState device_name_state;
  };

  // Returns a pointer to the singleton instance for the current process.
  // Should only be called after Initialize().
  static DeviceNameStore* GetInstance();

  // Register the pref used to store the device name in the local state.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Checks the local state for a device name to store if it exists; otherwise,
  // creates a new device name and persists it. Must be called before any other
  // non-static method on DeviceNameStore.
  // |prefs| is the PrefService used to persist and read the device name value.
  static void Initialize(PrefService* prefs,
                         policy::DeviceNamePolicyHandler* handler);

  // Shutdown() should be called to destroy the instance once its clients no
  // longer need it.
  static void Shutdown();

  virtual DeviceNameMetadata GetDeviceNameMetadata() const = 0;

  // Attempts to update the device name and returns the result of the update.
  virtual SetDeviceNameResult SetDeviceName(
      const std::string& new_device_name) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  DeviceNameStore();
  virtual ~DeviceNameStore();

  void NotifyDeviceNameMetadataChanged();

 private:
  DeviceNameStore(const DeviceNameStore&) = delete;
  DeviceNameStore& operator=(const DeviceNameStore&) = delete;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_STORE_H_
