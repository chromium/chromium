// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_TOUCH_CONTROLLER_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_TOUCH_CONTROLLER_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class FjordTouchControllerScreenView;

// Implements the OOBE screen that explains the touch controller setup steps.
// This should only be shown in the Fjord variant of OOBE.
class FjordTouchControllerScreen : public BaseScreen {
 public:
  using TView = FjordTouchControllerScreenView;

  explicit FjordTouchControllerScreen(
      base::WeakPtr<FjordTouchControllerScreenView> view);
  FjordTouchControllerScreen(const FjordTouchControllerScreen&) = delete;
  FjordTouchControllerScreen& operator=(const FjordTouchControllerScreen&) =
      delete;
  ~FjordTouchControllerScreen() override;

 private:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override {}

  base::WeakPtr<FjordTouchControllerScreenView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_TOUCH_CONTROLLER_SCREEN_H_
