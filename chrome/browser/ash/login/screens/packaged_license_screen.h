// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PACKAGED_LICENSE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PACKAGED_LICENSE_SCREEN_H_

#include <string>

#include "base/bind.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/packaged_license_screen_handler.h"

namespace ash {

// Screen which is shown before login and enterprise screens.
// It advertises the packaged license which allows user enroll device.
class PackagedLicenseScreen : public BaseScreen {
 public:
  enum class Result {
    // Show login screen
    DONT_ENROLL,
    // Show enterprise enrollment screen
    ENROLL,
    // No information about license in the `enrollment_config_`
    NOT_APPLICABLE,
    // Enterprise license should start from GAIA enrollment screen. This result
    // is different from Enroll since in this case the screen is skipped and
    // should not be recorded in metrics.
    NOT_APPLICABLE_SKIP_TO_ENROLL
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  PackagedLicenseScreen(PackagedLicenseView* view,
                        const ScreenExitCallback& exit_callback);
  PackagedLicenseScreen(const PackagedLicenseScreen&) = delete;
  PackagedLicenseScreen& operator=(const PackagedLicenseScreen&) = delete;
  ~PackagedLicenseScreen() override;

  void AddExitCallbackForTesting(const ScreenExitCallback& testing_callback) {
    exit_callback_ = base::BindRepeating(
        [](const ScreenExitCallback& original_callback,
           const ScreenExitCallback& testing_callback, Result result) {
          original_callback.Run(result);
          testing_callback.Run(result);
        },
        exit_callback_, testing_callback);
  }

  // BaseScreen
  bool MaybeSkip(WizardContext* context) override;

 protected:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserActionDeprecated(const std::string& action_id) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

 private:
  PackagedLicenseView* view_ = nullptr;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::PackagedLicenseScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PACKAGED_LICENSE_SCREEN_H_
