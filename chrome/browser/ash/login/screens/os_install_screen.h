// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_INSTALL_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_INSTALL_SCREEN_H_

#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

class OsInstallScreenView;

class OsInstallScreen : public BaseScreen {
 public:
  explicit OsInstallScreen(OsInstallScreenView* view);
  OsInstallScreen(const OsInstallScreen&) = delete;
  OsInstallScreen& operator=(const OsInstallScreen&) = delete;
  ~OsInstallScreen() override;

  void OnViewDestroyed(OsInstallScreenView* view);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  OsInstallScreenView* view_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OS_INSTALL_SCREEN_H_
