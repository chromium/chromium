// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_PIN_SETUP_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_PIN_SETUP_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class PinSetupScreenView;

class PinSetupScreen : public BaseScreen {
 public:
  enum class Result { NEXT, NOT_APPLICABLE };

  static std::string GetResultString(Result result);
  static bool ShouldSkip();

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  PinSetupScreen(PinSetupScreenView* view,
                 const ScreenExitCallback& exit_callback);
  ~PinSetupScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  PinSetupScreenView* const view_;
  ScreenExitCallback exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(PinSetupScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_PIN_SETUP_SCREEN_H_
