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
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/multi_profile_user_controller.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user.h"

class PrefRegistrySimple;

namespace gfx {
class ImageSkia;
}

namespace policy {
class CloudExternalDataPolicyHandler;
}  // namespace policy

namespace ash {

class MultiProfileUserController;
class SessionLengthLimiter;

// Chrome specific implementation of the UserManager.
class ChromeUserManagerImpl
    : public ChromeUserManager,
      public session_manager::SessionManagerObserver,
      public DeviceSettingsService::Observer,
      public policy::DeviceLocalAccountPolicyService::Observer,
      public policy::MinimumVersionPolicyHandler::Observer,
      public ProfileObserver,
      public ProfileManagerObserver {
 public:
  ChromeUserManagerImpl(const ChromeUserManagerImpl&) = delete;
  ChromeUserManagerImpl& operator=(const ChromeUserManagerImpl&) = delete;

  ~ChromeUserManagerImpl() override;

  // Creates ChromeUserManagerImpl instance.
  static std::unique_ptr<ChromeUserManager> CreateChromeUserManager();

  // Registers user manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // UserManagerInterface implementation:
  MultiProfileUserController* GetMultiProfileUserController() override;
  UserImageManager* GetUserImageManager(const AccountId& account_id) override;

  // UserManager implementation:
  void Shutdown() override;
  user_manager::UserList GetUsersAllowedForMultiProfile() const override;
  user_manager::UserList GetUnlockUsers() const override;
  void SaveUserOAuthStatus(
      const AccountId& account_id,
      user_manager::User::OAuthTokenStatus oauth_token_status) override;
  void SaveUserDisplayName(const AccountId& account_id,
                           const std::u16string& display_name) override;
  bool CanCurrentUserLock() const override;
  bool IsGuestSessionAllowed() const override;
  bool IsGaiaUserAllowed(const user_manager::User& user) const override;
  bool IsUserAllowed(const user_manager::User& user) const override;
  void AsyncRemoveCryptohome(const AccountId& account_id) const override;
  bool IsDeprecatedSupervisedAccountId(
      const AccountId& account_id) const override;
  const gfx::ImageSkia& GetResourceImagekiaNamed(int id) const override;
  std::u16string GetResourceStringUTF16(int string_id) const override;
  void ScheduleResolveLocale(const std::string& locale,
                             base::OnceClosure on_resolved_callback,
                             std::string* out_resolved_locale) const override;
  bool IsValidDefaultUserImageId(int image_index) const override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override;

  // policy::DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

  void StopPolicyObserverForTesting();
  SessionLengthLimiter* GetSessionLengthLimiterForTesting() {
    return session_length_limiter_.get();
  }

  // policy::MinimumVersionPolicyHandler::Observer:
  void OnMinimumVersionStateChanged() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // ChromeUserManager:
  bool IsEnterpriseManaged() const override;
  void SetUserAffiliation(
      const AccountId& account_id,
      const base::flat_set<std::string>& user_affiliation_ids) override;

 protected:
  const std::string& GetApplicationLocale() const override;
  void LoadDeviceLocalAccounts(std::set<AccountId>* users_set) override;
  void NotifyOnLogin() override;
  void NotifyUserAddedToSession(const user_manager::User* added_user,
                                bool user_switch_pending) override;
  void PerformPostUserLoggedInActions(bool browser_restart) override;
  void RemoveNonCryptohomeData(const AccountId& account_id) override;
  void RemoveUserInternal(const AccountId& account_id,
                          user_manager::UserRemovalReason reason) override;
  bool IsDeviceLocalAccountMarkedForRemoval(
      const AccountId& account_id) const override;
  void GuestUserLoggedIn() override;
  void KioskAppLoggedIn(user_manager::User* user) override;
  void PublicAccountUserLoggedIn(user_manager::User* user) override;
  void RegularUserLoggedIn(const AccountId& account_id,
                           const user_manager::UserType user_type) override;
  void RegularUserLoggedInAsEphemeral(
      const AccountId& account_id,
      const user_manager::UserType user_type) override;
  bool IsEphemeralAccountIdByPolicy(const AccountId& account_id) const override;

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

  // Starts (or stops) automatic timezone refresh on geolocation,
  // depending on user preferences.
  void UpdateUserTimeZoneRefresher(Profile* profile);

  // Creates a user for the given device local account.
  std::unique_ptr<user_manager::User> CreateUserFromDeviceLocalAccount(
      const AccountId& account_id,
      const policy::DeviceLocalAccount::Type type) const;

  void UpdateOwnerId();

  // Remove non cryptohome data associated with the given `account_id` after
  // having removed all external data (such as wallpapers and avatars)
  // associated with that `account_id`, this function is guarded by a latch
  // `remove_non_cryptohome_data_latch_` that ensures that all external data is
  // removed prior to clearing prefs for `account_id`, as the removal of certain
  // external data depends on prefs.
  void RemoveNonCryptohomeDataPostExternalDataRemoval(
      const AccountId& account_id);

  // Interface to the signed settings store.
  raw_ptr<CrosSettings, ExperimentalAsh> cros_settings_;

  // Interface to device-local account definitions and associated policy.
  raw_ptr<policy::DeviceLocalAccountPolicyService, ExperimentalAsh>
      device_local_account_policy_service_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  // TODO(b/278643115): Move this out from ChromeUserManagerImpl.
  UserImageManagerRegistry user_image_manager_registry_;

  // Session length limiter.
  std::unique_ptr<SessionLengthLimiter> session_length_limiter_;

  // Cros settings change subscriptions.
  base::CallbackListSubscription allow_guest_subscription_;
  base::CallbackListSubscription users_subscription_;
  base::CallbackListSubscription family_link_accounts_subscription_;
  base::CallbackListSubscription owner_subscription_;

  base::CallbackListSubscription ephemeral_users_enabled_subscription_;
  base::CallbackListSubscription local_accounts_subscription_;

  MultiProfileUserController multi_profile_user_controller_;

  std::vector<std::unique_ptr<policy::CloudExternalDataPolicyHandler>>
      cloud_external_data_policy_handlers_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  std::vector<
      std::unique_ptr<base::ScopedObservation<Profile, ProfileObserver>>>
      profile_observations_;

  base::RepeatingClosure remove_non_cryptohome_data_barrier_;

  std::unique_ptr<MountPerformer> mount_performer_;

  base::WeakPtrFactory<ChromeUserManagerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_
