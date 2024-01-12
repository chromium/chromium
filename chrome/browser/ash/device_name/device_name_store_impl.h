// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_STORE_IMPL_H_
#define CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_STORE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/device_name/device_name_store.h"

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

namespace ash {

class DeviceNameApplier;

// DeviceNameStore implementation which uses a PrefService to store the device
// name.
class DeviceNameStoreImpl : public DeviceNameStore,
                            public policy::DeviceNamePolicyHandler::Observer,
                            public ProfileManagerObserver {
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

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  std::string GetDeviceName() const;

  // Computes the new device name according to the device name policy.
  std::string ComputeDeviceName() const;

  // Computes the new device name state according to any active policies and
  // whether user is device owner.
  DeviceNameStore::DeviceNameState ComputeDeviceNameState() const;

  // Returns whether the device name policy in place prohits name update.
  bool IsConfiguringDeviceNameProhibitedByPolicy() const;

  // Returns whether the user cannot modify the name because
  bool CannotModifyBecauseNotDeviceOwner() const;

  // Sets the device name and notify observers of DeviceNameStore class.
  void ChangeDeviceName(const std::string& device_name);

  // Called from OnHostnamePolicyChanged() and AttemptDeviceNameStateUpdate().
  // Computes the new device name and attempts to update it.
  void AttemptUpdateToComputedDeviceName();

  // Called from AttemptUpdateToComputedDeviceName() and SetDeviceName() to set
  // the new device name if it is different from the one set previously in
  // |prefs_|. Observers are notified if the device name and/or state changes.
  void AttemptDeviceNameUpdate(const std::string& new_device_name);

  // Callback function for OwnerSettingsService::IsOwnerAsync() that attempts to
  // update the device name state and notify observers if the state has changed.
  void AttemptDeviceNameStateUpdate(bool is_user_owner);

  // Provides access and persistence for the device name value.
  raw_ptr<PrefService> prefs_;

  // Stores the device name state that was last set.
  DeviceNameStore::DeviceNameState device_name_state_;

  // Stores whether the active profile is owner or not. This takes an initial
  // value of false and gets updated in AttemptDeviceNameStateUpdate().
  bool is_user_owner_ = false;

  raw_ptr<policy::DeviceNamePolicyHandler> handler_;
  std::unique_ptr<DeviceNameApplier> device_name_applier_;

  base::ScopedObservation<policy::DeviceNamePolicyHandler,
                          policy::DeviceNamePolicyHandler::Observer>
      policy_handler_observation_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};

  base::WeakPtrFactory<DeviceNameStoreImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_STORE_IMPL_H_
