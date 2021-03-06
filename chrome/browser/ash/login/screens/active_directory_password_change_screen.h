// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/authpolicy/authpolicy_helper.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace authpolicy {
class ActiveDirectoryAccountInfo;
}

namespace chromeos {

class ActiveDirectoryPasswordChangeView;
class Key;

// Controller for the active directory password change screen.
class ActiveDirectoryPasswordChangeScreen : public BaseScreen {
 public:
  using TView = ActiveDirectoryPasswordChangeView;

  explicit ActiveDirectoryPasswordChangeScreen(
      ActiveDirectoryPasswordChangeView* view,
      const base::RepeatingClosure& exit_callback);
  ActiveDirectoryPasswordChangeScreen(
      const ActiveDirectoryPasswordChangeScreen&) = delete;
  ActiveDirectoryPasswordChangeScreen& operator=(
      const ActiveDirectoryPasswordChangeScreen&) = delete;
  ~ActiveDirectoryPasswordChangeScreen() override;

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(ActiveDirectoryPasswordChangeView* view);

  // Set username.
  void SetUsername(const std::string& username);

  // Handles password change request.
  void ChangePassword(const std::string& old_password,
                      const std::string& new_password);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  // Handles cancel password change request.
  void HandleCancel();

  // Callback called by AuthPolicyHelper::AuthenticateUser with results and
  // error code. (see AuthPolicyHelper::AuthenticateUser)
  void OnAuthFinished(
      const std::string& username,
      const Key& key,
      authpolicy::ErrorType error,
      const authpolicy::ActiveDirectoryAccountInfo& account_info);

  std::string username_;

  // Helper to call AuthPolicyClient and cancel calls if needed. Used to change
  // password on the Active Directory server.
  std::unique_ptr<AuthPolicyHelper> authpolicy_login_helper_;

  ActiveDirectoryPasswordChangeView* view_ = nullptr;

  base::RepeatingClosure exit_callback_;

  base::WeakPtrFactory<ActiveDirectoryPasswordChangeScreen> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_H_
