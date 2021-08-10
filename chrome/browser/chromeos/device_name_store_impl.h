// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_NAME_STORE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_NAME_STORE_IMPL_H_

#include "chrome/browser/chromeos/device_name_store.h"

#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

class DeviceNameApplier;

// DeviceNameStore implementation which uses a PrefService to store the device
// name.
class DeviceNameStoreImpl
    : public DeviceNameStore,
      public policy::DeviceNamePolicyHandler::Observer,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  DeviceNameStoreImpl(PrefService* prefs,
                      policy::DeviceNamePolicyHandler* handler);

  ~DeviceNameStoreImpl() override;

  // DeviceNameStore:
  DeviceNameMetadata GetDeviceNameMetadata() const override;
  DeviceNameStore::SetDeviceNameResult SetDeviceName(
      const std::string& new_device_name) override;

 private:
  friend class DeviceNameStoreImplTest;

  DeviceNameStoreImpl(PrefService* prefs,
                      policy::DeviceNamePolicyHandler* handler,
                      std::unique_ptr<DeviceNameApplier> device_name_applier);

  // policy::DeviceNamePolicyHandler::Observer:
  void OnHostnamePolicyChanged() override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  std::string GetDeviceName() const;

  // Computes the new device name according to the device name policy.
  std::string ComputeDeviceName() const;

  // Computes the new device name state according to any active policies and
  // whether user is device owner.
  DeviceNameStore::DeviceNameState ComputeDeviceNameState() const;

  // Returns whether the device name policy in place prohits name update.
  bool IsConfiguringDeviceNameProhibitedByPolicy() const;

  // Sets the device name and notify observers of DeviceNameStore class.
  void ChangeDeviceName(const std::string& device_name);

  // Called from OnHostnamePolicyChanged() and SetDeviceName() to set the device
  // name and notify observers of DeviceNameStore class. The new device name
  // must be different from the one set previously in |prefs_|.
  void AttemptDeviceNameUpdate(const std::string& new_device_name);

  // Provides access and persistence for the device name value.
  PrefService* prefs_;

  // Stores the device name state that was last set.
  DeviceNameStore::DeviceNameState device_name_state_;

  policy::DeviceNamePolicyHandler* handler_;
  std::unique_ptr<DeviceNameApplier> device_name_applier_;

  base::ScopedObservation<policy::DeviceNamePolicyHandler,
                          policy::DeviceNamePolicyHandler::Observer>
      policy_handler_observation_{this};
  base::ScopedObservation<
      user_manager::UserManager,
      user_manager::UserManager::UserSessionStateObserver,
      &user_manager::UserManager::AddSessionStateObserver,
      &user_manager::UserManager::RemoveSessionStateObserver>
      user_manager_observation_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_NAME_STORE_IMPL_H_
