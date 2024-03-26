// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_PACKAGED_LICENSE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_PACKAGED_LICENSE_SCREEN_H_

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_oobe.mojom.h"

namespace ash {

class PackagedLicenseView;

// Screen which is shown before login and enterprise screens.
// It advertises the packaged license which allows user enroll device.
class PackagedLicenseScreen
    : public BaseScreen,
      public OobeMojoBinder<screens_oobe::mojom::PackagedLicensePageHandler>,
      public screens_oobe::mojom::PackagedLicensePageHandler {
 public:
  using TView = PackagedLicenseView;

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
  PackagedLicenseScreen(base::WeakPtr<PackagedLicenseView> view,
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
  bool MaybeSkip(WizardContext& context) override;

 protected:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  // screens_oobe::mojom::PackagedLicensePageHandler
  void OnDontEnrollClicked() override;
  void OnEnrollClicked() override;

 private:
  base::WeakPtr<PackagedLicenseView> view_;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_PACKAGED_LICENSE_SCREEN_H_
