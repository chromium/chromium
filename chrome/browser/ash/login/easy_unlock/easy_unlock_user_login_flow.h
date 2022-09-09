// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_USER_LOGIN_FLOW_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_USER_LOGIN_FLOW_H_

#include "chrome/browser/ash/login/user_flow.h"

class AccountId;

namespace ash {

class UserContext;

// Handler for login flow initiazted by Easy Signin login attempt.
// The only difference to the default login flow is hanlding of the auth
// failure.
class EasyUnlockUserLoginFlow : public ExtendedUserFlow {
 public:
  explicit EasyUnlockUserLoginFlow(const AccountId& account_id);

  EasyUnlockUserLoginFlow(const EasyUnlockUserLoginFlow&) = delete;
  EasyUnlockUserLoginFlow& operator=(const EasyUnlockUserLoginFlow&) = delete;

  ~EasyUnlockUserLoginFlow() override;

 private:
  // ExtendedUserFlow implementation.
  bool HandleLoginFailure(const AuthFailure& failure) override;
  void HandleLoginSuccess(const UserContext& context) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_USER_LOGIN_FLOW_H_
