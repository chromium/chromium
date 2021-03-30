// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_WEBUI_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_WEBUI_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "components/user_manager/user.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

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
  void ShowAllowlistCheckFailedError() override;

  // SigninScreenHandlerDelegate implementation:
  void Login(const UserContext& user_context,
             const SigninSpecifics& specifics) override;
  bool IsSigninInProgress() const override;
  void OnSigninScreenReady() override;
  void CancelUserAdding() override;
  void ShowEnterpriseEnrollmentScreen() override;
  void ShowKioskAutolaunchScreen() override;
  void ShowWrongHWIDScreen() override;
  void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler) override;
  bool AllowNewUserChanged() const override;
  bool IsUserSigninCompleted() const override;

  // ui::UserActivityDetector implementation:
  void OnUserActivity(const ui::Event* event) override;

 private:
  // Whether to show add new user.
  bool allow_new_user_ = false;

  // Whether the allow new user setting has changed.
  bool allow_new_user_changed_ = false;

  // Reference to the WebUI handling layer for the login screen
  LoginDisplayWebUIHandler* webui_handler_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayWebUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_WEBUI_H_
