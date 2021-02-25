// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_SETUP_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

class DemoSetupScreenView;

// Controls demo mode setup. The screen can be shown during OOBE. It allows
// user to setup retail demo mode on the device.
class DemoSetupScreen : public BaseScreen {
 public:
  enum class Result { COMPLETED, CANCELED };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  DemoSetupScreen(DemoSetupScreenView* view,
                  const ScreenExitCallback& exit_callback);
  ~DemoSetupScreen() override;

  // Called when view is being destroyed. If Screen is destroyed earlier
  // then it has to call Bind(nullptr).
  void OnViewDestroyed(DemoSetupScreenView* view);

  // Test utilities.
  void SetCurrentSetupStepForTest(
      const DemoSetupController::DemoSetupStep current_step);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  void StartEnrollment();

  // Updates current setup step.
  void SetCurrentSetupStep(
      const DemoSetupController::DemoSetupStep current_step);

  // Called when the setup flow finished with error.
  void OnSetupError(const DemoSetupController::DemoSetupError& error);

  // Called when the setup flow finished successfully.
  void OnSetupSuccess();

  DemoSetupScreenView* view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<DemoSetupScreen> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DemoSetupScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_SETUP_SCREEN_H_
