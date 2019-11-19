// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_MOCK_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_MOCK_USER_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/affiliation.h"
#include "chrome/browser/chromeos/login/users/avatar/mock_user_image_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class FakeSupervisedUserManager;

class MockUserManager : public ChromeUserManager {
 public:
  MockUserManager();
  virtual ~MockUserManager();

  MOCK_METHOD0(Shutdown, void(void));
  MOCK_CONST_METHOD0(GetUsersAllowedForMultiProfile,
                     user_manager::UserList(void));
  MOCK_CONST_METHOD0(GetLoggedInUsers, const user_manager::UserList&(void));
  MOCK_CONST_METHOD0(GetLRULoggedInUsers, const user_manager::UserList&(void));
  MOCK_METHOD4(UserLoggedIn,
               void(const AccountId&, const std::string&, bool, bool));
  MOCK_METHOD0(SessionStarted, void(void));
  MOCK_METHOD2(RemoveUser,
               void(const AccountId&, user_manager::RemoveUserDelegate*));
  MOCK_METHOD1(RemoveUserFromList, void(const AccountId&));
  MOCK_CONST_METHOD1(IsKnownUser, bool(const AccountId&));
  MOCK_CONST_METHOD1(FindUser, const user_manager::User*(const AccountId&));
  MOCK_METHOD1(FindUserAndModify, user_manager::User*(const AccountId&));
  MOCK_METHOD2(SaveUserOAuthStatus,
               void(const AccountId&, user_manager::User::OAuthTokenStatus));
  MOCK_METHOD2(SaveForceOnlineSignin, void(const AccountId&, bool));
  MOCK_METHOD2(SaveUserDisplayName,
               void(const AccountId&, const base::string16&));
  MOCK_METHOD2(UpdateUserAccountData,
               void(const AccountId&, const UserAccountData&));
  MOCK_CONST_METHOD1(GetUserDisplayName, base::string16(const AccountId&));
  MOCK_METHOD2(SaveUserDisplayEmail,
               void(const AccountId&, const std::string&));
  MOCK_CONST_METHOD0(IsCurrentUserOwner, bool(void));
  MOCK_CONST_METHOD0(IsCurrentUserNew, bool(void));
  MOCK_CONST_METHOD0(IsCurrentUserNonCryptohomeDataEphemeral, bool(void));
  MOCK_CONST_METHOD0(CanCurrentUserLock, bool(void));
  MOCK_CONST_METHOD0(IsUserLoggedIn, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsUserWithGaiaAccount, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsChildUser, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsPublicAccount, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsGuest, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsSupervisedUser, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsKioskApp, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsArcKioskApp, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsWebKioskApp, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsStub, bool(void));
  MOCK_CONST_METHOD0(IsSessionStarted, bool(void));
  MOCK_CONST_METHOD1(IsUserNonCryptohomeDataEphemeral, bool(const AccountId&));
  MOCK_METHOD1(AddObserver, void(UserManager::Observer*));
  MOCK_METHOD1(RemoveObserver, void(UserManager::Observer*));
  MOCK_METHOD1(AddSessionStateObserver,
               void(UserManager::UserSessionStateObserver*));
  MOCK_METHOD1(RemoveSessionStateObserver,
               void(UserManager::UserSessionStateObserver*));
  MOCK_METHOD0(NotifyLocalStateChanged, void(void));
  MOCK_CONST_METHOD0(AreSupervisedUsersAllowed, bool(void));
  MOCK_CONST_METHOD0(IsGuestSessionAllowed, bool(void));
  MOCK_CONST_METHOD1(IsGaiaUserAllowed, bool(const user_manager::User& user));
  MOCK_CONST_METHOD1(IsUserAllowed, bool(const user_manager::User& user));
  MOCK_CONST_METHOD3(UpdateLoginState,
                     void(const user_manager::User*,
                          const user_manager::User*,
                          bool));
  MOCK_CONST_METHOD1(AsyncRemoveCryptohome, void(const AccountId&));
  MOCK_CONST_METHOD3(GetPlatformKnownUserId,
                     bool(const std::string&, const std::string&, AccountId*));
  MOCK_CONST_METHOD0(GetGuestAccountId, const AccountId&());
  MOCK_CONST_METHOD0(IsFirstExecAfterBoot, bool(void));
  MOCK_CONST_METHOD1(IsGuestAccountId, bool(const AccountId&));
  MOCK_CONST_METHOD1(IsStubAccountId, bool(const AccountId&));
  MOCK_CONST_METHOD1(IsSupervisedAccountId, bool(const AccountId&));
  MOCK_CONST_METHOD0(HasBrowserRestarted, bool(void));

  // UserManagerBase overrides:
  MOCK_CONST_METHOD0(AreEphemeralUsersEnabled, bool(void));
  MOCK_CONST_METHOD0(GetApplicationLocale, const std::string&(void));
  MOCK_CONST_METHOD0(GetLocalState, PrefService*(void));
  MOCK_CONST_METHOD2(HandleUserOAuthTokenStatusChange,
                     void(const AccountId&,
                          user_manager::User::OAuthTokenStatus status));
  MOCK_CONST_METHOD0(IsEnterpriseManaged, bool(void));
  MOCK_METHOD1(LoadDeviceLocalAccounts, void(std::set<AccountId>*));
  MOCK_METHOD0(PerformPreUserListLoadingActions, void(void));
  MOCK_METHOD0(PerformPostUserListLoadingActions, void(void));
  MOCK_METHOD1(PerformPostUserLoggedInActions, void(bool));
  MOCK_CONST_METHOD1(IsDemoApp, bool(const AccountId&));
  MOCK_CONST_METHOD1(IsKioskApp, bool(const AccountId&));
  MOCK_CONST_METHOD1(IsDeviceLocalAccountMarkedForRemoval,
                     bool(const AccountId&));
  MOCK_METHOD0(DemoAccountLoggedIn, void(void));
  MOCK_METHOD1(KioskAppLoggedIn, void(user_manager::User*));
  MOCK_METHOD1(ArcKioskAppLoggedIn, void(user_manager::User*));
  MOCK_METHOD1(WebKioskAppLoggedIn, void(user_manager::User*));
  MOCK_METHOD1(PublicAccountUserLoggedIn, void(user_manager::User*));
  MOCK_METHOD1(SupervisedUserLoggedIn, void(const AccountId&));
  MOCK_METHOD1(OnUserRemoved, void(const AccountId&));
  MOCK_CONST_METHOD1(GetResourceImagekiaNamed, const gfx::ImageSkia&(int));
  MOCK_CONST_METHOD1(GetResourceStringUTF16, base::string16(int));
  MOCK_CONST_METHOD3(DoScheduleResolveLocale,
                     void(const std::string&,
                          base::OnceClosure*,
                          std::string*));
  MOCK_CONST_METHOD1(IsValidDefaultUserImageId, bool(int));

  // You can't mock these functions easily because nobody can create
  // User objects but the ChromeUserManager and us.
  const user_manager::UserList& GetUsers() const override;
  user_manager::UserList GetUnlockUsers() const override;
  const AccountId& GetOwnerAccountId() const override;
  const user_manager::User* GetActiveUser() const override;
  user_manager::User* GetActiveUser() override;
  const user_manager::User* GetPrimaryUser() const override;

  // We can't mock it as easily.
  bool IsLoggedInAsAnyKioskApp() const override;

  // ChromeUserManager overrides:
  MultiProfileUserController* GetMultiProfileUserController() override;
  UserImageManager* GetUserImageManager(const AccountId& account_id) override;
  SupervisedUserManager* GetSupervisedUserManager() override;
  MOCK_METHOD2(SetUserFlow, void(const AccountId&, UserFlow*));
  MOCK_METHOD1(ResetUserFlow, void(const AccountId&));
  UserFlow* GetCurrentUserFlow() const override;
  UserFlow* GetUserFlow(const AccountId&) const override;
  MOCK_METHOD2(SetUserAffiliation,
               void(const AccountId& account_id,
                    const chromeos::AffiliationIDSet& user_affiliation_ids));

  bool ShouldReportUser(const std::string& user_id) const override;
  MOCK_CONST_METHOD1(IsManagedSessionEnabledForUser,
                     bool(const user_manager::User&));
  MOCK_CONST_METHOD1(IsFullManagementDisclosureNeeded,
                     bool(policy::DeviceLocalAccountPolicyBroker*));

  // We cannot mock ScheduleResolveLocale directly because of
  // base::OnceClosure's removed deleter. This is a trampoline to the actual
  // mock.
  void ScheduleResolveLocale(const std::string& locale,
                             base::OnceClosure on_resolved_callback,
                             std::string* out_resolved_locale) const override;

  // Sets a new User instance. Users previously created by this MockUserManager
  // become invalid.
  void SetActiveUser(const AccountId& account_id);

  // Creates a new public session user. Users previously created by this
  // MockUserManager become invalid.
  user_manager::User* CreatePublicAccountUser(const AccountId& account_id);

  // Creates a new kiosk app user. Users previously created by this
  // MockUserManager become invalid.
  user_manager::User* CreateKioskAppUser(const AccountId& account_id);

  // Adds a new User instance to the back of the user list. Users previously
  // created by this MockUserManager remain valid. The added User is not
  // affiliated with the domain, that owns the device.
  void AddUser(const AccountId& account_id);

  // The same as AddUser, but allows specifying affiliation with the domain,
  // that owns the device and user type.
  void AddUserWithAffiliationAndType(const AccountId& account_id,
                                     bool is_affiliated,
                                     user_manager::UserType user_type);

  // Clears the user list and the active user. Users previously created by this
  // MockUserManager become invalid.
  void ClearUserList();

  std::unique_ptr<UserFlow> user_flow_;
  std::unique_ptr<MockUserImageManager> user_image_manager_;
  std::unique_ptr<FakeSupervisedUserManager> supervised_user_manager_;
  user_manager::UserList user_list_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_MOCK_USER_MANAGER_H_
