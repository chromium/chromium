// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_FAKE_CHROME_USER_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_FAKE_CHROME_USER_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager_impl.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH), "For ChromeOS ash-chrome only");

namespace ash {

// Fake chrome user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class FakeChromeUserManager : public user_manager::UserManagerImpl {
 public:
  FakeChromeUserManager();

  FakeChromeUserManager(const FakeChromeUserManager&) = delete;
  FakeChromeUserManager& operator=(const FakeChromeUserManager&) = delete;

  ~FakeChromeUserManager() override;

  // Create and add various types of users.
  user_manager::User* AddGuestUser();
  user_manager::User* AddKioskAppUser(const AccountId& account_id);
  user_manager::User* AddWebKioskAppUser(const AccountId& account_id);
  user_manager::User* AddKioskIwaUser(const AccountId& account_id);
  user_manager::User* AddPublicAccountUser(const AccountId& account_id);

  // Calculates the user name hash and calls UserLoggedIn to login a user.
  // Sets the user as having its profile created if `set_profile_created_flag`
  // is true, but does not create a profile.
  // NOTE: This does not match production, which first logs in the user, then
  // creates the profile and updates the user later.
  void LoginUser(const AccountId& account_id,
                 bool set_profile_created_flag = true);

  user_manager::User* AddUser(const AccountId& account_id);
  user_manager::User* AddChildUser(const AccountId& account_id);
  user_manager::User* AddUserWithAffiliation(const AccountId& account_id,
                                             bool is_affiliated);
  user_manager::User* AddSamlUser(const AccountId& account_id);

  // Creates and adds user with specified `account_id` and `user_type`. Sets
  // user affiliation. If `profile` is valid, maps it to the created user.
  user_manager::User* AddUserWithAffiliationAndTypeAndProfile(
      const AccountId& account_id,
      bool is_affiliated,
      user_manager::UserType user_type,
      TestingProfile* profile);

  // Sets the user profile created flag to simulate finishing user
  // profile loading. Note this does not create a profile.
  void SimulateUserProfileLoad(const AccountId& account_id);

  // user_manager::UserManager override.
  const user_manager::UserList& GetUsers() const override;
  user_manager::UserList GetUsersAllowedForMultiProfile() const override;
  const user_manager::UserList& GetLoggedInUsers() const override;
  const user_manager::UserList& GetLRULoggedInUsers() const override;
  user_manager::UserList GetUnlockUsers() const override;
  const AccountId& GetLastSessionActiveAccountId() const override;
  void UserLoggedIn(const AccountId& account_id,
                    const std::string& user_id_hash,
                    bool browser_restart,
                    bool is_child) override;
  void SwitchActiveUser(const AccountId& account_id) override;
  void SwitchToLastActiveUser() override;
  void OnSessionStarted() override;
  bool IsKnownUser(const AccountId& account_id) const override;
  const user_manager::User* FindUser(
      const AccountId& account_id) const override;
  user_manager::User* FindUserAndModify(const AccountId& account_id) override;
  const user_manager::User* GetActiveUser() const override;
  user_manager::User* GetActiveUser() override;
  const user_manager::User* GetPrimaryUser() const override;
  void SaveUserOAuthStatus(
      const AccountId& account_id,
      user_manager::User::OAuthTokenStatus oauth_token_status) override;
  void SaveForceOnlineSignin(const AccountId& account_id,
                             bool force_online_signin) override;
  void SaveUserDisplayName(const AccountId& account_id,
                           const std::u16string& display_name) override;
  void SaveUserDisplayEmail(const AccountId& account_id,
                            const std::string& display_email) override;
  void SaveUserType(const user_manager::User* user) override;
  std::optional<std::string> GetOwnerEmail() override;
  bool IsCurrentUserCryptohomeDataEphemeral() const override;
  bool IsCurrentUserNonCryptohomeDataEphemeral() const override;
  bool IsUserLoggedIn() const override;
  bool IsLoggedInAsUserWithGaiaAccount() const override;
  bool IsLoggedInAsChildUser() const override;
  bool IsLoggedInAsManagedGuestSession() const override;
  bool IsLoggedInAsGuest() const override;
  bool IsLoggedInAsKioskApp() const override;
  bool IsLoggedInAsWebKioskApp() const override;
  bool IsLoggedInAsAnyKioskApp() const override;
  bool IsLoggedInAsStub() const override;
  bool IsUserNonCryptohomeDataEphemeral(
      const AccountId& account_id) const override;
  bool IsGuestSessionAllowed() const override;
  bool IsGaiaUserAllowed(const user_manager::User& user) const override;
  bool IsUserAllowed(const user_manager::User& user) const override;
  bool IsDeprecatedSupervisedAccountId(
      const AccountId& account_id) const override;

  // user_manager::UserManagerImpl override.
  bool IsDeviceLocalAccountMarkedForRemoval(
      const AccountId& account_id) const override;
  // Just make it public for tests.
  using UserManagerImpl::SetOwnerId;

  // UserManager:
  void SetUserAffiliated(const AccountId& account_id,
                         bool is_affiliated) override;
  // TODO(b/278643115): merged into SetUserAffiliated.
  void SetUserAffiliationForTesting(const AccountId& account_id,
                                    bool is_affliated);

  // Just make it public for tests.
  using UserManagerImpl::SetEphemeralModeConfig;

  void set_current_user_ephemeral(bool user_ephemeral) {
    current_user_ephemeral_ = user_ephemeral;
  }
  void set_current_user_child(bool child_user) {
    current_user_child_ = child_user;
  }

  void set_last_session_active_account_id(
      const AccountId& last_session_active_account_id) {
    last_session_active_account_id_ = last_session_active_account_id;
  }

 private:
  // Returns the active user.
  user_manager::User* GetActiveUserInternal() const;

  bool current_user_ephemeral_ = false;
  bool current_user_child_ = false;

  // If set this is the active user. If empty, the first created user is the
  // active user.
  AccountId active_account_id_ = EmptyAccountId();

  AccountId last_session_active_account_id_ = EmptyAccountId();

  // Whether the device is enterprise managed.
  bool is_enterprise_managed_ = false;
};

}  // namespace ash

namespace chromeos {
using ::ash::FakeChromeUserManager;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_FAKE_CHROME_USER_MANAGER_H_
