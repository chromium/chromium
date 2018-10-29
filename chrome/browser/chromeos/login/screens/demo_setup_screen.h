// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_SETUP_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_SETUP_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class BaseScreenDelegate;
class DemoSetupScreenView;

// Controlls demo mode setup. The screen can be shown during OOBE. It allows
// user to setup retail demo mode on the device.
class DemoSetupScreen : public BaseScreen {
 public:
  DemoSetupScreen(BaseScreenDelegate* base_screen_delegate,
                  DemoSetupScreenView* view);
  ~DemoSetupScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  // Called when view is being destroyed. If Screen is destroyed earlier
  // then it has to call Bind(nullptr).
  void OnViewDestroyed(DemoSetupScreenView* view);

 private:
  void StartEnrollment();

  // Called when the setup flow finished with error.
  void OnSetupError(const DemoSetupController::DemoSetupError& error);

  // Called when the setup flow finished successfully.
  void OnSetupSuccess();

  DemoSetupScreenView* view_;

  base::WeakPtrFactory<DemoSetupScreen> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_SETUP_SCREEN_H_
