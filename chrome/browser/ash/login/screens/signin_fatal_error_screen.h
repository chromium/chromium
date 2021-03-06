// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_SIGNIN_FATAL_ERROR_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_SIGNIN_FATAL_ERROR_SCREEN_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

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
  };

  explicit SignInFatalErrorScreen(SignInFatalErrorView* view,
                                  const base::RepeatingClosure& exit_callback);
  SignInFatalErrorScreen(const SignInFatalErrorScreen&) = delete;
  SignInFatalErrorScreen& operator=(const SignInFatalErrorScreen&) = delete;
  ~SignInFatalErrorScreen() override;

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(SignInFatalErrorView* view);

  // Setting the error state.
  void SetErrorState(Error error, const base::Value* params);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  Error error_state_ = Error::UNKNOWN;
  base::Optional<base::Value> extra_error_info_;

  SignInFatalErrorView* view_ = nullptr;
  base::RepeatingClosure exit_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SIGNIN_FATAL_ERROR_SCREEN_H_
