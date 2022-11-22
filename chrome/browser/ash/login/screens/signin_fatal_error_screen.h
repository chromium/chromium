// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_SIGNIN_FATAL_ERROR_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_SIGNIN_FATAL_ERROR_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class SignInFatalErrorView;

// Controller for the gaia fatal error screen.
class SignInFatalErrorScreen : public BaseScreen {
 public:
  using TView = SignInFatalErrorView;

  // Sets the error information to be shown on the screen
  enum class Error {
    UNKNOWN = 0,
    SCRAPED_PASSWORD_VERIFICATION_FAILURE = 1,
    INSECURE_CONTENT_BLOCKED = 2,
    MISSING_GAIA_INFO = 3,
    CUSTOM = 4,
  };

  explicit SignInFatalErrorScreen(base::WeakPtr<SignInFatalErrorView> view,
                                  const base::RepeatingClosure& exit_callback);
  SignInFatalErrorScreen(const SignInFatalErrorScreen&) = delete;
  SignInFatalErrorScreen& operator=(const SignInFatalErrorScreen&) = delete;
  ~SignInFatalErrorScreen() override;

  // Setting the error methods.
  void SetErrorState(Error error, base::Value::Dict params);
  void SetCustomError(const std::string& error_text,
                      const std::string& keyboard_hint,
                      const std::string& details,
                      const std::string& help_link_text);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  Error error_state_ = Error::UNKNOWN;
  base::Value::Dict extra_error_info_;

  base::WeakPtr<SignInFatalErrorView> view_;
  base::RepeatingClosure exit_callback_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SIGNIN_FATAL_ERROR_SCREEN_H_
