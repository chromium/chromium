// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_SETUP_SCREEN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_SETUP_SCREEN_VIEW_H_

#include <string>

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

class DemoSetupScreen;

// Interface of the demo mode setup screen view.
class DemoSetupScreenView {
 public:
  constexpr static OobeScreen kScreenId = OobeScreen::SCREEN_OOBE_DEMO_SETUP;

  virtual ~DemoSetupScreenView();

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Sets view and screen.
  virtual void Bind(DemoSetupScreen* screen) = 0;

  // Handles successful setup.
  virtual void OnSetupSucceeded() = 0;

  // Handles setup failure.
  virtual void OnSetupFailed(
      const DemoSetupController::DemoSetupError& error) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_SETUP_SCREEN_VIEW_H_
