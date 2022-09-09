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
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
class SignInFatalErrorView;
}

namespace ash {

// Controller for the gaia fatal error screen.
class SignInFatalErrorScreen : public BaseScreen {
 public:
  using TView = chromeos::SignInFatalErrorView;

  // Sets the error information to be shown on the screen
  enum class Error {
    UNKNOWN = 0,
    SCRAPED_PASSWORD_VERIFICATION_FAILURE = 1,
    INSECURE_CONTENT_BLOCKED = 2,
    MISSING_GAIA_INFO = 3,
    CUSTOM = 4,
  };

  explicit SignInFatalErrorScreen(
      base::WeakPtr<chromeos::SignInFatalErrorView> view,
      const base::RepeatingClosure& exit_callback);
  SignInFatalErrorScreen(const SignInFatalErrorScreen&) = delete;
  SignInFatalErrorScreen& operator=(const SignInFatalErrorScreen&) = delete;
  ~SignInFatalErrorScreen() override;

  // Setting the error methods.
  void SetErrorState(Error error, const base::Value* params);
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
  absl::optional<base::Value> extra_error_info_;

  base::WeakPtr<chromeos::SignInFatalErrorView> view_;
  base::RepeatingClosure exit_callback_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::SignInFatalErrorScreen;
}

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::SignInFatalErrorScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SIGNIN_FATAL_ERROR_SCREEN_H_
