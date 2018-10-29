// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class BaseScreenDelegate;
class MultiDeviceSetupScreenView;

class MultiDeviceSetupScreen : public BaseScreen {
 public:
  MultiDeviceSetupScreen(BaseScreenDelegate* base_screen_delegate,
                         MultiDeviceSetupScreenView* view);
  ~MultiDeviceSetupScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  // Exits the screen.
  void ExitScreen();

  MultiDeviceSetupScreenView* view_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_
