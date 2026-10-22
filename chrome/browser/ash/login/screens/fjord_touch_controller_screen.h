// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_TOUCH_CONTROLLER_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_TOUCH_CONTROLLER_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"

namespace ash {

class FjordTouchControllerScreenView;

// Implements the OOBE screen that explains the touch controller setup steps.
// This should only be shown in the Fjord variant of OOBE.
class FjordTouchControllerScreen
    : public BaseScreen,
      public screens_common::mojom::FjordTouchControllerPageHandler,
      public OobeMojoBinder<
          screens_common::mojom::FjordTouchControllerPageHandler,
          screens_common::mojom::FjordTouchControllerPage> {
 public:
  using TView = FjordTouchControllerScreenView;

  FjordTouchControllerScreen(base::WeakPtr<FjordTouchControllerScreenView> view,
                             const base::RepeatingClosure& exit_callback);
  FjordTouchControllerScreen(const FjordTouchControllerScreen&) = delete;
  FjordTouchControllerScreen& operator=(const FjordTouchControllerScreen&) =
      delete;
  ~FjordTouchControllerScreen() override;

  bool ExitScreen();

  // Called when the touch controller signals that its setup is finished.
  // Enables the next button so that the user can continue to the next screen.
  // Returns false if the screen is not showing.
  bool OnTouchControllerSetupFinished();

  void set_exit_callback_for_testing(base::RepeatingClosure exit_callback) {
    exit_callback_ = exit_callback;
  }

 private:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override {}

  // screens_common::mojom::FjordTouchControllerPageHandler
  void OnSetupComplete() override;
  void OnTestImageSkipClicked() override;

  // Updates the enabled state of the next button in the UI.
  void UpdateNextButtonState();

  // Whether the touch controller has signaled that its setup is finished.
  bool touch_controller_setup_finished_ = false;

  // Whether the device is running a test image. Determined when the screen is
  // shown and used to gate the test image skip button.
  bool is_test_image_ = false;

  base::RepeatingClosure exit_callback_;

  base::WeakPtr<FjordTouchControllerScreenView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_TOUCH_CONTROLLER_SCREEN_H_
