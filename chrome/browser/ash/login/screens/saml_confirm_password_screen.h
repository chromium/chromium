// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_SAML_CONFIRM_PASSWORD_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_SAML_CONFIRM_PASSWORD_SCREEN_H_

#include <memory>
#include <string>

#include "ash/components/login/auth/user_context.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/saml_confirm_password_handler.h"
#include "components/login/base_screen_handler_utils.h"

namespace ash {

// This class represents GAIA screen: login screen that is responsible for
// GAIA-based sign-in.
class SamlConfirmPasswordScreen : public BaseScreen {
 public:
  using TView = SamlConfirmPasswordView;

  enum class Result {
    kCancel,
    kTooManyAttempts,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  SamlConfirmPasswordScreen(base::WeakPtr<SamlConfirmPasswordView> view,
                            const ScreenExitCallback& exit_callback);

  SamlConfirmPasswordScreen(const SamlConfirmPasswordScreen&) = delete;
  SamlConfirmPasswordScreen& operator=(const SamlConfirmPasswordScreen&) =
      delete;

  ~SamlConfirmPasswordScreen() override;

  void SetContextAndPasswords(std::unique_ptr<UserContext> user_context,
                              ::login::StringList scraped_saml_passwords);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void TryPassword(const std::string& password);

  base::WeakPtr<SamlConfirmPasswordView> view_;

  ScreenExitCallback exit_callback_;

  std::unique_ptr<UserContext> user_context_;
  ::login::StringList scraped_saml_passwords_;
  int attempt_count_ = 0;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::SamlConfirmPasswordScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SAML_CONFIRM_PASSWORD_SCREEN_H_
