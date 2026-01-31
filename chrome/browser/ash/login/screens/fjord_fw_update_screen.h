// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_FW_UPDATE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_FW_UPDATE_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class FjordFwUpdateScreenView;

// Implements the OOBE screen that is shown while peripherals are being updated
// on the station. This should only be shown in the Fjord variant of OOBE.
class FjordFwUpdateScreen : public BaseScreen {
 public:
  using TView = FjordFwUpdateScreenView;
  FjordFwUpdateScreen(base::WeakPtr<FjordFwUpdateScreenView> view,
                      base::RepeatingClosure exit_callback);
  FjordFwUpdateScreen(const FjordFwUpdateScreen&) = delete;
  FjordFwUpdateScreen& operator=(const FjordFwUpdateScreen&) = delete;
  ~FjordFwUpdateScreen() override;

  bool ExitScreen();

  void set_exit_callback_for_testing(base::RepeatingClosure exit_callback) {
    exit_callback_ = exit_callback;
  }

 private:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override {}

  base::WeakPtr<FjordFwUpdateScreenView> view_;
  base::RepeatingClosure exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_FW_UPDATE_SCREEN_H_
