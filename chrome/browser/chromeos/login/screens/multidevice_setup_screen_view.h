// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_VIEW_H_

#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

class MultiDeviceSetupScreen;

// Interface for dependency injection between MultiDeviceSetupScreen and its
// WebUI representation.
class MultiDeviceSetupScreenView {
 public:
  constexpr static OobeScreen kScreenId = OobeScreen::SCREEN_MULTIDEVICE_SETUP;

  virtual ~MultiDeviceSetupScreenView() = default;

  virtual void Bind(MultiDeviceSetupScreen* screen) = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_VIEW_H_
