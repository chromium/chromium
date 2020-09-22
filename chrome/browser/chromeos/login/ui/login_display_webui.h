// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_WEBUI_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_WEBUI_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "components/user_manager/user.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/views/widget/widget.h"

class AccountId;

namespace chromeos {

class UserBoardView;
class UserSelectionScreen;

// WebUI-based login UI implementation.
class LoginDisplayWebUI : public LoginDisplay,
                          public SigninScreenHandlerDelegate,
                          public ui::UserActivityObserver {
 public:
  LoginDisplayWebUI();
  ~LoginDisplayWebUI() override;

  // LoginDisplay implementation:
  void ClearAndEnablePassword() override;
  void Init(const user_manager::UserList& users,
            bool show_guest,
            bool show_users,
            bool allow_new_user) override;
  void OnPreferencesChanged() override;
  void SetUIEnabled(bool is_enabled) override;
  void ShowError(int error_msg_id,
                 int login_attempts,
                 HelpAppLauncher::HelpTopic help_topic_id) override;
  void ShowPasswordChangedDialog(bool show_password_error,
                                 const AccountId& account_id) override;
  void ShowSigninUI(const std::string& email) override;
  void ShowAllowlistCheckFailedError() override;

  // SigninScreenHandlerDelegate implementation:
  void Login(const UserContext& user_context,
             const SigninSpecifics& specifics) override;
  bool IsSigninInProgress() const override;
  void OnSigninScreenReady() override;
  void CancelUserAdding() override;
  void ShowEnterpriseEnrollmentScreen() override;
  void ShowKioskEnableScreen() override;
  void ShowKioskAutolaunchScreen() override;
  void ShowWrongHWIDScreen() override;
  void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler) override;
  bool IsShowUsers() const override;
  bool ShowUsersHasChanged() const override;
  bool AllowNewUserChanged() const override;
  bool IsUserSigninCompleted() const override;

  void HandleGetUsers() override;
  void CheckUserStatus(const AccountId& account_id) override;

  // ui::UserActivityDetector implementation:
  void OnUserActivity(const ui::Event* event) override;

 private:
  // Whether to show the user pods or a GAIA sign in.
  // Public sessions are always shown.
  bool show_users_ = false;

  // Whether the create new account option in GAIA is enabled by the setting.
  bool show_users_changed_ = false;

  // Whether to show add new user.
  bool allow_new_user_ = false;

  // Whether the allow new user setting has changed.
  bool allow_new_user_changed_ = false;

  // Reference to the WebUI handling layer for the login screen
  LoginDisplayWebUIHandler* webui_handler_ = nullptr;

  // Used only for the "user-adding" (aka "multiprofile") flow.
  std::unique_ptr<UserSelectionScreen> user_selection_screen_;
  base::WeakPtr<UserBoardView> user_board_view_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayWebUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_WEBUI_H_
