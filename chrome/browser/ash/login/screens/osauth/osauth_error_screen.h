// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_OSAUTH_ERROR_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_OSAUTH_ERROR_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"

namespace ash {

class OSAuthErrorScreenView;

// Screen to indicate an error that happened during configuring
// user's auth factors.
class OSAuthErrorScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = OSAuthErrorScreenView;
  enum class Result {
    kAbortSignin,
    kFallbackOnline,
    kFallbackLocal,
    kProceedAuthenticated,
  };

  static std::string GetResultString(Result result);
  using ScreenExitCallback = base::RepeatingCallback<void(Result)>;

  OSAuthErrorScreen(base::WeakPtr<OSAuthErrorScreenView> view,
                    ScreenExitCallback exit_callback);
  ~OSAuthErrorScreen() override;

  OSAuthErrorScreen(const OSAuthErrorScreen&) = delete;
  OSAuthErrorScreen& operator=(const OSAuthErrorScreen&) = delete;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void OnTokenInvalidated();

  base::WeakPtr<OSAuthErrorScreenView> view_ = nullptr;
  ScreenExitCallback exit_callback_;
  base::WeakPtrFactory<OSAuthErrorScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_OSAUTH_ERROR_SCREEN_H_
