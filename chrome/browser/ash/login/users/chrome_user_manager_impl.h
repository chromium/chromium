// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/ash/login/users/affiliation.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_base.h"

namespace ash {

// Chrome specific implementation of the UserManager.
class ChromeUserManagerImpl
    : public user_manager::UserManagerBase,
      public DeviceSettingsService::Observer,
      public policy::DeviceLocalAccountPolicyService::Observer,
      public policy::MinimumVersionPolicyHandler::Observer {
 public:
  ChromeUserManagerImpl(const ChromeUserManagerImpl&) = delete;
  ChromeUserManagerImpl& operator=(const ChromeUserManagerImpl&) = delete;

  ~ChromeUserManagerImpl() override;

  // Creates ChromeUserManagerImpl instance.
  static std::unique_ptr<ChromeUserManagerImpl> CreateChromeUserManager();

  // UserManager implementation:
  void Shutdown() override;

  // DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override;

  // policy::DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

  // policy::MinimumVersionPolicyHandler::Observer:
  void OnMinimumVersionStateChanged() override;

 protected:
  void RemoveNonCryptohomeData(const AccountId& account_id) override;

 private:
  friend class UserManagerTest;
  friend class WallpaperManager;
  friend class WallpaperManagerTest;
  friend class MockRemoveUserManager;

  ChromeUserManagerImpl();

  // Retrieves trusted device policies and removes users from the persistent
  // list if ephemeral users are enabled. Schedules a callback to itself if
  // trusted device policies are not yet available.
  void RetrieveTrustedDevicePolicies();

  // If data for a device local account is marked as pending removal and the
  // user is no longer logged into that account, removes the data.
  void RemovePendingDeviceLocalAccount();

  // Replaces the list of device local accounts with those found in
  // `device_local_accounts`. Ensures that data belonging to accounts no longer
  // on the list is removed. Returns `true` if the list has changed.
  // Device local accounts are defined by policy. This method is called whenever
  // an updated list of device local accounts is received from policy.
  bool UpdateAndCleanUpDeviceLocalAccounts(
      const std::vector<DeviceLocalAccountInfo>& device_local_accounts);

  // Update the number of users.
  void UpdateNumberOfUsers();

  void UpdateOwnerId();

  // Returns the display name taken from policy, expected to be used for
  // public accounts.
  std::optional<std::u16string> GetDisplayName(std::string_view user_id);

  // Interface to device-local account definitions and associated policy.
  raw_ptr<policy::DeviceLocalAccountPolicyService>
      device_local_account_policy_service_;

  // Cros settings change subscriptions.
  base::CallbackListSubscription allow_guest_subscription_;
  base::CallbackListSubscription users_subscription_;
  base::CallbackListSubscription family_link_accounts_subscription_;
  base::CallbackListSubscription owner_subscription_;

  base::CallbackListSubscription ephemeral_users_enabled_subscription_;
  base::CallbackListSubscription local_accounts_subscription_;

  base::RepeatingClosure remove_non_cryptohome_data_barrier_;

  base::WeakPtrFactory<ChromeUserManagerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_
