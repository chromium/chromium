// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_STATION_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_STATION_SETUP_SCREEN_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"
namespace ash {

class FjordStationSetupScreenView;

// Implements the OOBE screen that explains the steps to calibrate the device.
// This should only be shown in the Fjord variant of OOBE.
class FjordStationSetupScreen
    : public BaseScreen,
      public screens_common::mojom::FjordStationSetupPageHandler,
      public OobeMojoBinder<
          screens_common::mojom::FjordStationSetupPageHandler> {
 public:
  using TView = FjordStationSetupScreenView;
  FjordStationSetupScreen(base::WeakPtr<FjordStationSetupScreenView> view,
                          const base::RepeatingClosure& exit_callback);
  FjordStationSetupScreen(const FjordStationSetupScreen&) = delete;
  FjordStationSetupScreen& operator=(const FjordStationSetupScreen&) = delete;
  ~FjordStationSetupScreen() override;

  void set_exit_callback_for_testing(base::RepeatingClosure exit_callback) {
    exit_callback_ = exit_callback;
  }

 private:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override {}

  // screens_common::mojom::FjordStationSetupPageHandler
  void OnSetupComplete() override;

  base::WeakPtr<FjordStationSetupScreenView> view_;
  base::RepeatingClosure exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_STATION_SETUP_SCREEN_H_
