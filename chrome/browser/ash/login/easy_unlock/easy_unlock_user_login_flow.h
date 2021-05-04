// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_USER_LOGIN_FLOW_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_USER_LOGIN_FLOW_H_

#include "base/macros.h"
#include "chrome/browser/ash/login/user_flow.h"

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
  bool HandleLoginFailure(const AuthFailure& failure) override;
  void HandleLoginSuccess(const UserContext& context) override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockUserLoginFlow);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when the //chrome/browser/chromeos
// source code migration is finished.
namespace ash {
using ::chromeos::EasyUnlockUserLoginFlow;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_USER_LOGIN_FLOW_H_
