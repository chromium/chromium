// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_TEST_API_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_TEST_API_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"

class AccountId;

namespace ash {

class ASH_PUBLIC_EXPORT LoginScreenTestApi {
 public:
  static bool IsLockShown();
  static bool IsLoginShelfShown();
  static bool IsRestartButtonShown();
  static bool IsShutdownButtonShown();
  static bool IsAuthErrorBubbleShown();
  static bool IsGuestButtonShown();
  static bool IsAddUserButtonShown();
  static bool IsParentAccessButtonShown();
  static void SubmitPassword(const AccountId& account_id,
                             const std::string& password);
  static int64_t GetUiUpdateCount();
  static bool LaunchApp(const std::string& app_id);
  static bool ClickAddUserButton();
  static bool ClickGuestButton();
  static bool WaitForUiUpdate(int64_t previous_update_count);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(LoginScreenTestApi);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_TEST_API_H_
