// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_SETUP_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class DemoSetupScreenView;

// Controls demo mode setup. The screen can be shown during OOBE. It allows
// user to setup retail demo mode on the device.
class DemoSetupScreen : public BaseScreen {
 public:
  enum class Result { kCompleted, kCanceled };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  DemoSetupScreen(base::WeakPtr<DemoSetupScreenView> view,
                  const ScreenExitCallback& exit_callback);

  DemoSetupScreen(const DemoSetupScreen&) = delete;
  DemoSetupScreen& operator=(const DemoSetupScreen&) = delete;

  ~DemoSetupScreen() override;

  // Test utilities.
  void SetCurrentSetupStepForTest(
      const DemoSetupController::DemoSetupStep current_step);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

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

  base::WeakPtr<DemoSetupScreenView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<DemoSetupScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_DEMO_SETUP_SCREEN_H_
