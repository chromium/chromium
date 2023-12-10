// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_TRIAL_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_TRIAL_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class OsTrialScreenView;

class OsTrialScreen : public BaseScreen {
 public:
  enum class Result {
    kNextTry,
    kNextInstall,
    kBack,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  OsTrialScreen(base::WeakPtr<OsTrialScreenView> view,
                const ScreenExitCallback& exit_callback);
  OsTrialScreen(const OsTrialScreen&) = delete;
  OsTrialScreen& operator=(const OsTrialScreen&) = delete;
  ~OsTrialScreen() override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<OsTrialScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_TRIAL_SCREEN_H_
