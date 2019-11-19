// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"
#include "chromeos/dbus/biod/fake_biod_client.h"

namespace chromeos {

namespace {

constexpr char kTestFingerprintDataString[] = "testFinger";

int kMaxAllowedFingerprints = 3;

}  // namespace

class FingerprintSetupTest : public OobeBaseTest {
 public:
  FingerprintSetupTest() = default;
  ~FingerprintSetupTest() override = default;

  void SetUpOnMainThread() override {
    // Enable fingerprint for testing.
    quick_unlock::EnabledForTesting(true);

    // Override the screen exit callback with our own method.
    FingerprintSetupScreen* fingerprint_screen = FingerprintSetupScreen::Get(
        WizardController::default_controller()->screen_manager());
    fingerprint_screen->set_exit_callback_for_testing(
        base::BindRepeating(&FingerprintSetupTest::OnFingerprintSetupScreenExit,
                            base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  // Shows the fingerprint screen and overrides its exit callback.
  void ShowFingerprintScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        FingerprintSetupScreenView::kScreenId);
    OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (screen_exit_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnFingerprintSetupScreenExit() {
    screen_exit_ = true;
    if (screen_exit_callback_) {
      std::move(screen_exit_callback_).Run();
    }
  }

  void EnrollFingerprint(int percent_complete) {
    base::RunLoop().RunUntilIdle();
    FakeBiodClient::Get()->SendEnrollScanDone(
        kTestFingerprintDataString, biod::SCAN_RESULT_SUCCESS,
        percent_complete == 100 /* is_complete */, percent_complete);
    base::RunLoop().RunUntilIdle();
  }

  void CheckCompletedEnroll() {
    test::OobeJS().ExpectVisiblePath({"fingerprint-setup-impl", "arc"});
    test::OobeJS()
        .CreateVisibilityWaiter(
            true, {"fingerprint-setup-impl", "fingerprintEnrollDone"})
        ->Wait();
    test::OobeJS().ExpectHiddenPath(
        {"fingerprint-setup-impl", "skipFingerprintEnroll"});
    test::OobeJS().ExpectVisiblePath(
        {"fingerprint-setup-impl", "arc", "checkmarkAnimation"});
    test::OobeJS().ExpectVisiblePath(
        {"fingerprint-setup-impl", "fingerprintAddAnother"});
  }

 private:
  bool screen_exit_ = false;
  base::RepeatingClosure screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintEnrollHalf) {
  ShowFingerprintScreen();

  EnrollFingerprint(50);
  test::OobeJS().ExpectVisiblePath({"fingerprint-setup-impl", "arc"});
  test::OobeJS().ExpectVisiblePath(
      {"fingerprint-setup-impl", "skipFingerprintEnroll"});
  test::OobeJS().ExpectHiddenPath(
      {"fingerprint-setup-impl", "fingerprintAddAnother"});
  test::OobeJS().ExpectHiddenPath(
      {"fingerprint-setup-impl", "fingerprintEnrollDone"});

  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "skipFingerprintEnroll"});

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintEnrollFull) {
  ShowFingerprintScreen();

  EnrollFingerprint(100);
  CheckCompletedEnroll();

  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "fingerprintEnrollDone"});

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintEnrollLimit) {
  ShowFingerprintScreen();

  for (int i = 0; i < kMaxAllowedFingerprints - 1; i++) {
    EnrollFingerprint(100);
    CheckCompletedEnroll();
    test::OobeJS().TapOnPath(
        {"fingerprint-setup-impl", "fingerprintAddAnother", "textButton"});
  }

  EnrollFingerprint(100);
  test::OobeJS().ExpectHiddenPath(
      {"fingerprint-setup-impl", "fingerprintAddAnother"});
  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "fingerprintEnrollDone"});

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintDisabled) {
  // Disable fingerprint
  quick_unlock::EnabledForTesting(false);

  WizardController::default_controller()->AdvanceToScreen(
      FingerprintSetupScreenView::kScreenId);

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupScreenElements) {
  ShowFingerprintScreen();

  test::OobeJS().CreateVisibilityWaiter(true, {"fingerprint-setup"})->Wait();
  test::OobeJS().ExpectVisible("fingerprint-setup-impl");

  test::OobeJS().ExpectVisiblePath(
      {"fingerprint-setup-impl", "setupFingerprint"});
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupCancel) {
  ShowFingerprintScreen();

  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "skipFingerprintSetup"});

  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupNext) {
  ShowFingerprintScreen();

  test::OobeJS().CreateVisibilityWaiter(true, {"fingerprint-setup"})->Wait();

  test::OobeJS().TapOnPath(
      {"fingerprint-setup-impl", "showSensorLocationButton"});

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"fingerprint-setup-impl", "placeFinger"})
      ->Wait();

  test::OobeJS().ExpectHiddenPath(
      {"fingerprint-setup-impl", "setupFingerprint"});
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupLater) {
  ShowFingerprintScreen();

  test::OobeJS().CreateVisibilityWaiter(true, {"fingerprint-setup"})->Wait();
  test::OobeJS().TapOnPath(
      {"fingerprint-setup-impl", "showSensorLocationButton"});
  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {"fingerprint-setup-impl", "setupFingerprintLater"})
      ->Wait();
  test::OobeJS().TapOnPath({"fingerprint-setup-impl", "setupFingerprintLater"});

  WaitForScreenExit();
}

}  // namespace chromeos
