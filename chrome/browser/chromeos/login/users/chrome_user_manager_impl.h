// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller_delegate.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace gfx {
class ImageSkia;
}

namespace user_manager {
class RemoveUserDelegate;
}

namespace policy {
class CloudExternalDataPolicyHandler;
}  // namespace policy

namespace chromeos {

class MultiProfileUserController;
class SupervisedUserManagerImpl;
class SessionLengthLimiter;

// Chrome specific implementation of the UserManager.
class ChromeUserManagerImpl
    : public ChromeUserManager,
      public content::NotificationObserver,
      public DeviceSettingsService::Observer,
      public policy::DeviceLocalAccountPolicyService::Observer,
      public policy::MinimumVersionPolicyHandler::Observer,
      public ProfileManagerObserver,
      public MultiProfileUserControllerDelegate {
 public:
  ~ChromeUserManagerImpl() override;

  // Creates ChromeUserManagerImpl instance.
  static std::unique_ptr<ChromeUserManager> CreateChromeUserManager();

  // Registers user manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Resets platform specific delegates that were set for public accounts.
  static void ResetPublicAccountDelegatesForTesting();

  // UserManagerInterface implementation:
  MultiProfileUserController* GetMultiProfileUserController() override;
  UserImageManager* GetUserImageManager(const AccountId& account_id) override;
  SupervisedUserManager* GetSupervisedUserManager() override;
  UserFlow* GetCurrentUserFlow() const override;
  UserFlow* GetUserFlow(const AccountId& account_id) const override;
  void SetUserFlow(const AccountId& account_id, UserFlow* flow) override;
  void ResetUserFlow(const AccountId& account_id) override;

  // UserManager implementation:
  void Shutdown() override;
  user_manager::UserList GetUsersAllowedForMultiProfile() const override;
  user_manager::UserList GetUnlockUsers() const override;
  void SaveUserOAuthStatus(
      const AccountId& account_id,
      user_manager::User::OAuthTokenStatus oauth_token_status) override;
  void SaveUserDisplayName(const AccountId& account_id,
                           const base::string16& display_name) override;
  bool CanCurrentUserLock() const override;
  bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  bool AreSupervisedUsersAllowed() const override;
  bool IsGuestSessionAllowed() const override;
  bool IsGaiaUserAllowed(const user_manager::User& user) const override;
  bool IsUserAllowed(const user_manager::User& user) const override;
  const AccountId& GetGuestAccountId() const override;
  bool IsFirstExecAfterBoot() const override;
  void AsyncRemoveCryptohome(const AccountId& account_id) const override;
  bool IsGuestAccountId(const AccountId& account_id) const override;
  bool IsStubAccountId(const AccountId& account_id) const override;
  bool IsSupervisedAccountId(const AccountId& account_id) const override;
  bool HasBrowserRestarted() const override;
  const gfx::ImageSkia& GetResourceImagekiaNamed(int id) const override;
  base::string16 GetResourceStringUTF16(int string_id) const override;
  void ScheduleResolveLocale(const std::string& locale,
                             base::OnceClosure on_resolved_callback,
                             std::string* out_resolved_locale) const override;
  bool IsValidDefaultUserImageId(int image_index) const override;

  // content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override;

  // policy::DeviceLocalAccountPolicyService::Observer:
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

  void StopPolicyObserverForTesting();

  // policy::MinimumVersionPolicyHandler::Observer:
  void OnMinimumVersionStateChanged() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // UserManagerBase:
  bool AreEphemeralUsersEnabled() const override;
  void OnUserRemoved(const AccountId& account_id) override;

  // ChromeUserManager:
  bool IsEnterpriseManaged() const override;
  void SetUserAffiliation(
      const AccountId& account_id,
      const AffiliationIDSet& user_affiliation_ids) override;
  bool ShouldReportUser(const std::string& user_id) const override;
  bool IsManagedSessionEnabledForUser(
      const user_manager::User& active_user) const override;
  bool IsFullManagementDisclosureNeeded(
      policy::DeviceLocalAccountPolicyBroker* broker) const override;

 protected:
  const std::string& GetApplicationLocale() const override;
  PrefService* GetLocalState() const override;
  void HandleUserOAuthTokenStatusChange(
      const AccountId& account_id,
      user_manager::User::OAuthTokenStatus status) const override;
  void LoadDeviceLocalAccounts(std::set<AccountId>* users_set) override;
  void NotifyOnLogin() override;
  void NotifyUserAddedToSession(const user_manager::User* added_user,
                                bool user_switch_pending) override;
  void PerformPreUserListLoadingActions() override;
  void PerformPostUserListLoadingActions() override;
  void PerformPostUserLoggedInActions(bool browser_restart) override;
  void RemoveNonCryptohomeData(const AccountId& account_id) override;
  void RemoveUserInternal(const AccountId& account_id,
                          user_manager::RemoveUserDelegate* delegate) override;
  bool IsDemoApp(const AccountId& account_id) const override;
  bool IsDeviceLocalAccountMarkedForRemoval(
      const AccountId& account_id) const override;
  void DemoAccountLoggedIn() override;
  void GuestUserLoggedIn() override;
  void KioskAppLoggedIn(user_manager::User* user) override;
  void ArcKioskAppLoggedIn(user_manager::User* user) override;
  void WebKioskAppLoggedIn(user_manager::User* user) override;
  void PublicAccountUserLoggedIn(user_manager::User* user) override;
  void RegularUserLoggedIn(const AccountId& account_id,
                           const user_manager::UserType user_type) override;
  void RegularUserLoggedInAsEphemeral(
      const AccountId& account_id,
      const user_manager::UserType user_type) override;
  void SupervisedUserLoggedIn(const AccountId& account_id) override;

 private:
  friend class SupervisedUserManagerImpl;
  friend class UserManagerTest;
  friend class WallpaperManager;
  friend class WallpaperManagerTest;

  using UserImageManagerMap =
      std::map<AccountId, std::unique_ptr<UserImageManager>>;

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
  // |device_local_accounts|. Ensures that data belonging to accounts no longer
  // on the list is removed. Returns |true| if the list has changed.
  // Device local accounts are defined by policy. This method is called whenever
  // an updated list of device local accounts is received from policy.
  bool UpdateAndCleanUpDeviceLocalAccounts(
      const std::vector<policy::DeviceLocalAccount>& device_local_accounts);

  // Updates the display name for public account |username| from policy settings
  // associated with that username.
  void UpdatePublicAccountDisplayName(const std::string& user_id);

  // Lazily creates default user flow.
  UserFlow* GetDefaultUserFlow() const;

  // MultiProfileUserControllerDelegate implementation:
  void OnUserNotAllowed(const std::string& user_email) override;

  // Update the number of users.
  void UpdateNumberOfUsers();

  // Starts (or stops) automatic timezone refresh on geolocation,
  // depending on user preferences.
  void UpdateUserTimeZoneRefresher(Profile* profile);

  // Adds user to the list of the users who should be reported.
  void AddReportingUser(const AccountId& account_id);

  // Removes user from the list of the users who should be reported.
  void RemoveReportingUser(const AccountId& account_id);

  // Creates a user for the given device local account.
  std::unique_ptr<user_manager::User> CreateUserFromDeviceLocalAccount(
      const AccountId& account_id,
      const policy::DeviceLocalAccount::Type type) const;

  // Interface to the signed settings store.
  CrosSettings* cros_settings_;

  // Interface to device-local account definitions and associated policy.
  policy::DeviceLocalAccountPolicyService* device_local_account_policy_service_;

  content::NotificationRegistrar registrar_;

  // User avatar managers.
  UserImageManagerMap user_image_managers_;

  // Supervised user manager.
  std::unique_ptr<SupervisedUserManagerImpl> supervised_user_manager_;

  // Session length limiter.
  std::unique_ptr<SessionLengthLimiter> session_length_limiter_;

  using FlowMap = std::map<AccountId, UserFlow*>;

  // Lazy-initialized default flow.
  mutable std::unique_ptr<UserFlow> default_flow_;

  // Specific flows by user e-mail. Keys should be canonicalized before
  // access.
  FlowMap specific_flows_;

  // Cros settings change subscriptions.
  std::unique_ptr<CrosSettings::ObserverSubscription> allow_guest_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription>
      allow_supervised_user_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription> users_subscription_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      local_accounts_subscription_;

  std::unique_ptr<MultiProfileUserController> multi_profile_user_controller_;

  std::vector<std::unique_ptr<policy::CloudExternalDataPolicyHandler>>
      cloud_external_data_policy_handlers_;

  base::WeakPtrFactory<ChromeUserManagerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeUserManagerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_CHROME_USER_MANAGER_IMPL_H_
