// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_LOGIN_UI_SIGNIN_UI_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_SIGNIN_UI_H_

#include "base/callback.h"
#include "chrome/browser/ash/login/screens/encryption_migration_mode.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"

namespace chromeos {

// This class represents an interface between code that performs sign-in
// operations and code that handles sign-in UI. It is used to encapsulate UI
// implementation details and declare the required set of parameters that need
// to be set for particular UI.
class SigninUI {
 public:
  SigninUI() = default;
  virtual ~SigninUI() = default;
  SigninUI(const SigninUI&) = delete;
  SigninUI& operator=(const SigninUI&) = delete;

  // Starts user onboarding after successful sign-in for new users.
  virtual void StartUserOnboarding() = 0;
  // Show UI for supervision transition flow.
  virtual void StartSupervisionTransition() = 0;

  virtual void StartEncryptionMigration(
      const UserContext& user_context,
      EncryptionMigrationMode migration_mode,
      base::OnceCallback<void(const UserContext&)> skip_migration_callback) = 0;

  // Might store authentication data so that additional auth factors can be
  // added during user onboarding.
  virtual void SetAuthSessionForOnboarding(const UserContext& user_context) = 0;

  // Show password changed dialog. If `show_password_error` is true, user
  // already tried to enter old password but it turned out to be incorrect.
  virtual void ShowPasswordChangedDialog(const AccountId& account_id,
                                         bool password_incorrect) = 0;
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_ASH_LOGIN_UI_SIGNIN_UI_H_
