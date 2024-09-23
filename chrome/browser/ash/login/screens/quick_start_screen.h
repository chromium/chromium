// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/quickstart_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"

namespace ash {

class ScopedSessionRefresher;

class QuickStartScreen : public BaseScreen,
                         public quick_start::QuickStartController::UiDelegate {
 public:
  using TView = QuickStartView;

  enum class Result {
    CANCEL_AND_RETURN_TO_WELCOME,
    CANCEL_AND_RETURN_TO_NETWORK,
    CANCEL_AND_RETURN_TO_GAIA_INFO,
    CANCEL_AND_RETURN_TO_SIGNIN,
    SETUP_COMPLETE_NEXT_BUTTON,
    WIFI_CREDENTIALS_RECEIVED,
    FALLBACK_URL_ON_GAIA,
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  QuickStartScreen(base::WeakPtr<TView> view,
                   quick_start::QuickStartController* controller,
                   const ScreenExitCallback& exit_callback);

  QuickStartScreen(const QuickStartScreen&) = delete;
  QuickStartScreen& operator=(const QuickStartScreen&) = delete;

  ~QuickStartScreen() override;

  static std::string GetResultString(Result result);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // quick_start::QuickStartController::UiDelegate:
  void OnUiUpdateRequested(
      quick_start::QuickStartController::UiState state) final;

  // Exits the screen and returns to the appropriate entry point. This is called
  // whenever the user aborts the flow, or when an error occurs.
  void ExitScreen();

  base::WeakPtr<TView> view_;
  raw_ptr<quick_start::QuickStartController> controller_;
  // For keeping the AuthSession alive while the success steps is shown.
  std::unique_ptr<ScopedSessionRefresher> session_refresher_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_
