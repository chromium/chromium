// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_MOJO_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_MOJO_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/ui/login_display.h"
#include "components/user_manager/user_manager.h"

namespace ash {
class LoginDisplayHostMojo;

// Interface used by UI-agnostic code to send messages to views-based login
// screen.
// TODO(estade): rename to LoginDisplayAsh.
class LoginDisplayMojo : public LoginDisplay,
                         public user_manager::UserManager::Observer {
 public:
  explicit LoginDisplayMojo(LoginDisplayHostMojo* host);

  LoginDisplayMojo(const LoginDisplayMojo&) = delete;
  LoginDisplayMojo& operator=(const LoginDisplayMojo&) = delete;

  ~LoginDisplayMojo() override;

  // Updates the state of the authentication methods supported for the user.
  void UpdatePinKeyboardState(const AccountId& account_id);
  void UpdateChallengeResponseAuthAvailability(const AccountId& account_id);

  // LoginDisplay:
  void Init(const user_manager::UserList& filtered_users,
            bool show_guest) override;
  void SetUIEnabled(bool is_enabled) override;

  // user_manager::UserManager::Observer:
  void OnUserImageChanged(const user_manager::User& user) override;

  void ShowOwnerPod(const AccountId& owner);

 private:
  void OnPinCanAuthenticate(const AccountId& account_id, bool can_authenticate);

  bool initialized_ = false;

  LoginDisplayHostMojo* const host_ = nullptr;  // Unowned.

  base::WeakPtrFactory<LoginDisplayMojo> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_MOJO_H_
