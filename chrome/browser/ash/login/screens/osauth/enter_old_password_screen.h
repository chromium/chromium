// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_ENTER_OLD_PASSWORD_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_ENTER_OLD_PASSWORD_SCREEN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"

namespace ash {

class EnterOldPasswordScreenView;

class EnterOldPasswordScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = EnterOldPasswordScreenView;

  enum class Result {
    kForgotOldPassword,
    kCryptohomeError,
    kAuthenticated,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  EnterOldPasswordScreen(base::WeakPtr<EnterOldPasswordScreenView> view,
                         const ScreenExitCallback& exit_callback);

  EnterOldPasswordScreen(const EnterOldPasswordScreen&) = delete;
  EnterOldPasswordScreen& operator=(const EnterOldPasswordScreen&) = delete;

  ~EnterOldPasswordScreen() override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void OnRemovedUserDirectory(std::unique_ptr<UserContext> user_context,
                              std::optional<AuthenticationError> error);

  void AttemptAuthentication(const std::string& old_password);
  void OnPasswordAuthentication(std::unique_ptr<UserContext> user_context,
                                std::optional<AuthenticationError> error);

  base::WeakPtr<EnterOldPasswordScreenView> view_;

  ScreenExitCallback exit_callback_;

  std::unique_ptr<AuthPerformer> auth_performer_;

  base::WeakPtrFactory<EnterOldPasswordScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_ENTER_OLD_PASSWORD_SCREEN_H_
