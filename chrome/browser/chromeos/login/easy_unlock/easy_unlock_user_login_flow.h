// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_USER_LOGIN_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_USER_LOGIN_FLOW_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/user_flow.h"

class AccountId;

namespace chromeos {

// Handler for login flow initiazted by Easy Signin login attempt.
// The only difference to the default login flow is hanlding of the auth
// failure.
class EasyUnlockUserLoginFlow : public ExtendedUserFlow {
 public:
  explicit EasyUnlockUserLoginFlow(const AccountId& account_id);
  ~EasyUnlockUserLoginFlow() override;

 private:
  // ExtendedUserFlow implementation.
  bool CanLockScreen() override;
  bool CanStartArc() override;
  bool ShouldLaunchBrowser() override;
  bool ShouldSkipPostLoginScreens() override;
  bool HandleLoginFailure(const AuthFailure& failure) override;
  void HandleLoginSuccess(const UserContext& context) override;
  void HandleOAuthTokenStatusChange(
      user_manager::User::OAuthTokenStatus status) override;
  void LaunchExtraSteps(Profile* profile) override;
  bool SupportsEarlyRestartToApplyFlags() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockUserLoginFlow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_USER_LOGIN_FLOW_H_
