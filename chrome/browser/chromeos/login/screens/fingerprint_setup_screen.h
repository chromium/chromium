// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_FINGERPRINT_SETUP_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_FINGERPRINT_SETUP_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class FingerprintSetupScreenView;

// Controls fingerprint setup. The screen can be shown during OOBE. It allows
// user to enroll fingerprint on the device.
class FingerprintSetupScreen : public BaseScreen {
 public:
  FingerprintSetupScreen(FingerprintSetupScreenView* view,
                         const base::RepeatingClosure& exit_callback);
  ~FingerprintSetupScreen() override;

  static FingerprintSetupScreen* Get(ScreenManager* manager);

  void set_exit_callback_for_testing(
      const base::RepeatingClosure& exit_callback) {
    exit_callback_ = exit_callback;
  }

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  FingerprintSetupScreenView* const view_;
  base::RepeatingClosure exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintSetupScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_FINGERPRINT_SETUP_SCREEN_H_
