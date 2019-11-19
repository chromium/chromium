// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_MOJO_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_MOJO_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

class LoginDisplayHostMojo;

// Interface used by UI-agnostic code to send messages to views-based login
// screen.
// TODO(estade): rename to LoginDisplayAsh.
class LoginDisplayMojo : public LoginDisplay,
                         public SigninScreenHandlerDelegate,
                         public user_manager::UserManager::Observer {
 public:
  explicit LoginDisplayMojo(LoginDisplayHostMojo* host);
  ~LoginDisplayMojo() override;

  // Updates the state of the authentication methods supported for the user.
  void UpdatePinKeyboardState(const AccountId& account_id);
  void UpdateChallengeResponseAuthAvailability(const AccountId& account_id);

  // LoginDisplay:
  void ClearAndEnablePassword() override;
  void Init(const user_manager::UserList& filtered_users,
            bool show_guest,
            bool show_users,
            bool show_new_user) override;
  void OnPreferencesChanged() override;
  void SetUIEnabled(bool is_enabled) override;
  void ShowError(int error_msg_id,
                 int login_attempts,
                 HelpAppLauncher::HelpTopic help_topic_id) override;
  void ShowErrorScreen(LoginDisplay::SigninError error_id) override;
  void ShowPasswordChangedDialog(bool show_password_error,
                                 const std::string& email) override;
  void ShowSigninUI(const std::string& email) override;
  void ShowWhitelistCheckFailedError() override;

  // SigninScreenHandlerDelegate:
  void Login(const UserContext& user_context,
             const SigninSpecifics& specifics) override;
  bool IsSigninInProgress() const override;
  void Signout() override;
  void OnSigninScreenReady() override;
  void ShowEnterpriseEnrollmentScreen() override;
  void ShowEnableDebuggingScreen() override;
  void ShowKioskEnableScreen() override;
  void ShowKioskAutolaunchScreen() override;
  void ShowWrongHWIDScreen() override;
  void ShowUpdateRequiredScreen() override;
  void CancelUserAdding() override;
  void RemoveUser(const AccountId& account_id) override;
  void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler) override;
  bool IsShowGuest() const override;
  bool IsShowUsers() const override;
  bool ShowUsersHasChanged() const override;
  bool IsAllowNewUser() const override;
  bool AllowNewUserChanged() const override;
  bool IsUserSigninCompleted() const override;
  void HandleGetUsers() override;
  void CheckUserStatus(const AccountId& account_id) override;

  // user_manager::UserManager::Observer:
  void OnUserImageChanged(const user_manager::User& user) override;

 private:
  void OnPinCanAuthenticate(const AccountId& account_id, bool can_authenticate);

  bool initialized_ = false;

  LoginDisplayHostMojo* const host_ = nullptr;  // Unowned.
  LoginDisplayWebUIHandler* webui_handler_ = nullptr;

  base::WeakPtrFactory<LoginDisplayMojo> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayMojo);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_MOJO_H_
