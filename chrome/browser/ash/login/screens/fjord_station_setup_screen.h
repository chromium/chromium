// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_STATION_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_STATION_SETUP_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class FjordStationSetupScreenView;

// Implements the OOBE screen that explains the steps to calibrate the device.
// This should only be shown in the Fjord variant of OOBE.
class FjordStationSetupScreen : public BaseScreen {
 public:
  using TView = FjordStationSetupScreenView;
  explicit FjordStationSetupScreen(
      base::WeakPtr<FjordStationSetupScreenView> view);
  FjordStationSetupScreen(const FjordStationSetupScreen&) = delete;
  FjordStationSetupScreen& operator=(const FjordStationSetupScreen&) = delete;
  ~FjordStationSetupScreen() override;

 private:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override {}

  base::WeakPtr<FjordStationSetupScreenView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_STATION_SETUP_SCREEN_H_
