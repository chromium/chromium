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
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_base.h"

namespace policy {
class CloudExternalDataPolicyHandler;
}  // namespace policy

namespace ash {

// Feature that removes deprecated ARC kiosk users.
BASE_DECLARE_FEATURE(kRemoveDeprecatedArcKioskUsersOnStartup);

// Domain that is used for ARC kiosk users.
extern const char kArcKioskDomain[];

// Chrome specific implementation of the UserManager.
class ChromeUserManagerImpl
    : public user_manager::UserManagerBase,
      public DeviceSettingsService::Observer,
      public policy::DeviceLocalAccountPolicyService::Observer,
      public policy::MinimumVersionPolicyHandler::Observer,
      public ProfileObserver,
      public ProfileManagerObserver {
 public:
  // These enum values represent a deprecated ARC kiosk user's status on the
  // sign in screen.
  // TODO(b/355590943): Remove once all ARC kiosk users are deleted in the wild.
  // ARC Kiosk has been deprecated and removed in m126. However, the accounts
  // still exist on the devices if configured prior to m126, but hidden. These
  // values are logged to UMA. Entries should not be renumbered and numeric
  // values should never be reused. Please keep in sync with
  // "DeprecatedArcKioskUserStatus" in src/tools/metrics/histograms/enums.xml.
  enum class DeprecatedArcKioskUserStatus {
    // ARC kiosk hidden on login screen. Expect this count to decline to zero
    // over
    // time.
    kHidden = 0,
    // Attempted to delete cryptohome. Expect this count to decline to zero
    // over time.
    kDeleted = 1,
    kMaxValue = kDeleted
  };

  ChromeUserManagerImpl(const ChromeUserManagerImpl&) = delete;
  ChromeUserManagerImpl& operator=(const ChromeUserManagerImpl&) = delete;

  ~ChromeUserManagerImpl() override;

  // Histogram for tracking the number of deprecated ARC kiosk user
  // cryptohomes remaining in the wild.
  // TODO(b/355590943): clean up once there is no ARC kiosk records.
  static const char kDeprecatedArcKioskUsersHistogramName[];

  // Creates ChromeUserManagerImpl instance.
  static std::unique_ptr<ChromeUserManagerImpl> CreateChromeUserManager();

  // UserManager implementation:
  void Shutdown() override;

  // DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override;

  // policy::DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

  void StopPolicyObserverForTesting();
  void SetUsingSamlForTesting(const AccountId& account_id, bool using_saml);

  // policy::MinimumVersionPolicyHandler::Observer:
  void OnMinimumVersionStateChanged() override;

  // ProfileManagerObserver:
  void OnProfileCreationStarted(Profile* profile) override;
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 protected:
  void LoadDeviceLocalAccounts(std::set<AccountId>* users_set) override;
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
  void CleanUpDeviceLocalAccountNonCryptohomeDataPendingRemoval();

  // Removes data belonging to device local accounts that are no longer found on
  // the user list. If the user is currently logged into one of these accounts,
  // the data for that account is not removed immediately but marked as pending
  // removal after logout.
  void CleanUpDeviceLocalAccountNonCryptohomeData(
      const std::vector<std::string>& old_device_local_accounts);

  // Replaces the list of device local accounts with those found in
  // `device_local_accounts`. Ensures that data belonging to accounts no longer
  // on the list is removed. Returns `true` if the list has changed.
  // Device local accounts are defined by policy. This method is called whenever
  // an updated list of device local accounts is received from policy.
  bool UpdateAndCleanUpDeviceLocalAccounts(
      const std::vector<policy::DeviceLocalAccount>& device_local_accounts);

  // Updates the display name for public account `username` from policy settings
  // associated with that username.
  void UpdatePublicAccountDisplayName(const std::string& user_id);

  // Update the number of users.
  void UpdateNumberOfUsers();

  // Creates a user for the given device local account.
  std::unique_ptr<user_manager::User> CreateUserFromDeviceLocalAccount(
      const AccountId& account_id,
      const policy::DeviceLocalAccountType type) const;

  void UpdateOwnerId();

  // Remove non cryptohome data associated with the given `account_id` after
  // having removed all external data (such as wallpapers and avatars)
  // associated with that `account_id`, this function is guarded by a latch
  // `remove_non_cryptohome_data_latch_` that ensures that all external data is
  // removed prior to clearing prefs for `account_id`, as the removal of certain
  // external data depends on prefs.
  void RemoveNonCryptohomeDataPostExternalDataRemoval(
      const AccountId& account_id);

  // Returns true if |account_id| is a deprecated ARC kiosk account.
  // TODO(b/355590943): Check if it is not used anymore and remove it.
  bool IsDeprecatedArcKioskAccountId(const AccountId& account_id) const;
  void RemoveDeprecatedArcKioskUser(const AccountId& account_id);

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

  std::vector<std::unique_ptr<policy::CloudExternalDataPolicyHandler>>
      cloud_external_data_policy_handlers_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  std::vector<
      std::unique_ptr<base::ScopedObservation<Profile, ProfileObserver>>>
      profile_observations_;

  base::RepeatingClosure remove_non_cryptohome_data_barrier_;

  base::WeakPtrFactory<ChromeUserManagerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_
