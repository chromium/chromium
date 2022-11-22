// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/authpolicy/authpolicy_helper.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace authpolicy {
class ActiveDirectoryAccountInfo;
}

namespace ash {

class ActiveDirectoryPasswordChangeView;
class Key;

// Controller for the active directory password change screen.
class ActiveDirectoryPasswordChangeScreen : public BaseScreen {
 public:
  using TView = ActiveDirectoryPasswordChangeView;

  ActiveDirectoryPasswordChangeScreen(
      base::WeakPtr<TView> view,
      const base::RepeatingClosure& exit_callback);
  ActiveDirectoryPasswordChangeScreen(
      const ActiveDirectoryPasswordChangeScreen&) = delete;
  ActiveDirectoryPasswordChangeScreen& operator=(
      const ActiveDirectoryPasswordChangeScreen&) = delete;
  ~ActiveDirectoryPasswordChangeScreen() override;

  // Set username.
  void SetUsername(const std::string& username);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // Handles cancel password change request.
  void HandleCancel();

  // Handles password change request.
  void HandleChangePassword(const std::string& old_password,
                            const std::string& new_password);

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

  base::WeakPtr<TView> view_;

  base::RepeatingClosure exit_callback_;

  base::WeakPtrFactory<ActiveDirectoryPasswordChangeScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ACTIVE_DIRECTORY_PASSWORD_CHANGE_SCREEN_H_
