// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_

#include <map>
#include <string>
#include <vector>

#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/signin/token_handle_util.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "ui/base/user_activity/user_activity_observer.h"

class AccountId;

namespace chromeos {

class EasyUnlockService;
class LoginDisplayWebUIHandler;
class UserBoardView;

// This class represents User Selection screen: user pod-based login screen.
class UserSelectionScreen
    : public ui::UserActivityObserver,
      public proximity_auth::ScreenlockBridge::LockHandler,
      public BaseScreen {
 public:
  explicit UserSelectionScreen(const std::string& display_type);
  ~UserSelectionScreen() override;

  void SetHandler(LoginDisplayWebUIHandler* handler);
  void SetView(UserBoardView* view);

  static const user_manager::UserList PrepareUserListForSending(
      const user_manager::UserList& users,
      const AccountId& owner,
      bool is_signin_to_add);

  virtual void Init(const user_manager::UserList& users);
  void OnUserImageChanged(const user_manager::User& user);
  void OnBeforeUserRemoved(const AccountId& account_id);
  void OnUserRemoved(const AccountId& account_id);

  void OnPasswordClearTimerExpired();

  void HandleGetUsers();
  void CheckUserStatus(const AccountId& account_id);

  // Build list of users and send it to the webui.
  virtual void SendUserList();

  // Methods for easy unlock support.
  void HardLockPod(const AccountId& account_id);
  void AttemptEasyUnlock(const AccountId& account_id);

  // ui::UserActivityDetector implementation:
  void OnUserActivity(const ui::Event* event) override;

  void InitEasyUnlock();

  // proximity_auth::ScreenlockBridge::LockHandler implementation:
  void ShowBannerMessage(const base::string16& message,
                         bool is_warning) override;
  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions& icon)
      override;
  void HideUserPodCustomIcon(const AccountId& account_id) override;

  void EnableInput() override;
  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const base::string16& auth_value) override;
  proximity_auth::mojom::AuthType GetAuthType(
      const AccountId& account_id) const override;
  ScreenType GetScreenType() const override;

  void Unlock(const AccountId& account_id) override;
  void AttemptEasySignin(const AccountId& account_id,
                         const std::string& secret,
                         const std::string& key_label) override;

  // BaseScreen implementation:
  void Show() override;
  void Hide() override;

  // Fills |user_dict| with information about |user|.
  static void FillUserDictionary(
      const user_manager::User* user,
      bool is_owner,
      bool is_signin_to_add,
      proximity_auth::mojom::AuthType auth_type,
      const std::vector<std::string>* public_session_recommended_locales,
      base::DictionaryValue* user_dict);

  // Fills |user_dict| with |user| multi-profile related preferences.
  static void FillMultiProfileUserPrefs(const user_manager::User* user,
                                        base::DictionaryValue* user_dict,
                                        bool is_signin_to_add);

  // Determines if user auth status requires online sign in.
  static bool ShouldForceOnlineSignIn(const user_manager::User* user);

  // Builds a |UserAvatar| instance which contains the current image for |user|.
  static ash::UserAvatar BuildAshUserAvatarForUser(
      const user_manager::User& user);

  std::unique_ptr<base::ListValue> UpdateAndReturnUserListForWebUI();
  std::vector<ash::LoginUserInfo> UpdateAndReturnUserListForAsh();
  void SetUsersLoaded(bool loaded);

 protected:
  UserBoardView* view_ = nullptr;

  // Map from public session account IDs to recommended locales set by policy.
  std::map<AccountId, std::vector<std::string>>
      public_session_recommended_locales_;

  // Whether users have been sent to the UI(WebUI or Views).
  bool users_loaded_ = false;

 private:
  class DircryptoMigrationChecker;

  EasyUnlockService* GetEasyUnlockServiceForUser(
      const AccountId& account_id) const;

  void OnUserStatusChecked(const AccountId& account_id,
                           TokenHandleUtil::TokenHandleStatus status);

  LoginDisplayWebUIHandler* handler_ = nullptr;

  // Purpose of the screen (see constants in OobeUI).
  const std::string display_type_;

  // Set of Users that are visible.
  user_manager::UserList users_;

  // Map of account ids to their current authentication type. If a user is not
  // contained in the map, it is using the default authentication type.
  std::map<AccountId, proximity_auth::mojom::AuthType> user_auth_type_map_;

  // Timer for measuring idle state duration before password clear.
  base::OneShotTimer password_clear_timer_;

  // Token handler util for checking user OAuth token status.
  std::unique_ptr<TokenHandleUtil> token_handle_util_;

  // Helper to check whether a user needs dircrypto migration.
  std::unique_ptr<DircryptoMigrationChecker> dircrypto_migration_checker_;

  user_manager::UserList users_to_send_;

  base::WeakPtrFactory<UserSelectionScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserSelectionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_SELECTION_SCREEN_H_
