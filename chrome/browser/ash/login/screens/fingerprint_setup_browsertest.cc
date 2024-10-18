// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

using ::testing::ElementsAre;

const test::UIPath kFingerprintScreen = {"fingerprint-setup"};
const test::UIPath kStartPage = {"fingerprint-setup", "setupFingerprint"};
const test::UIPath kProgressPage = {"fingerprint-setup",
                                    "startFingerprintEnroll"};
const test::UIPath kFingerprintArc = {"fingerprint-setup", "arc"};
const test::UIPath kScanningAnimation = {"fingerprint-setup", "arc",
                                         "scanningAnimation"};
const test::UIPath kDoneButton = {"fingerprint-setup", "done"};
const test::UIPath kSkipButtonOnStart = {"fingerprint-setup", "skipStart"};
const test::UIPath kSkipButtonOnProgress = {"fingerprint-setup",
                                            "skipProgress"};
const test::UIPath kAddAnotherFingerButton = {"fingerprint-setup",
                                              "addAnotherFinger"};

constexpr char kTestFingerprintDataString[] = "testFinger";
constexpr char kAssetUrlAttribute[] = "assetUrl";
constexpr char kCheckmarkAssetUrl[] =
    "chrome://resources/ash/common/quick_unlock/fingerprint_check.json";

int kMaxAllowedFingerprints = 3;

PasswordSelectionScreen* GetPasswordSelectionScreen() {
  return static_cast<PasswordSelectionScreen*>(
      WizardController::default_controller()->screen_manager()->GetScreen(
          PasswordSelectionScreenView::kScreenId));
}

FingerprintSetupScreen* GetScreen() {
  return WizardController::default_controller()
      ->GetScreen<FingerprintSetupScreen>();
}

}  // namespace

class FingerprintSetupTest : public OobeBaseTest {
 public:
  using Result = FingerprintSetupScreen::Result;
  using UserAction = FingerprintSetupScreen::UserAction;

  FingerprintSetupTest() = default;
  ~FingerprintSetupTest() override = default;

  void SetUpOnMainThread() override {
    // Enable fingerprint for testing.
    test_api_ = std::make_unique<quick_unlock::TestApi>(
        /*override_quick_unlock=*/true);
    test_api_->EnableFingerprintByPolicy(quick_unlock::Purpose::kUnlock);

    // Set it up so that we capture the exit result.
    original_callback_ = GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(
        base::BindLambdaForTesting([&](FingerprintSetupScreen::Result result) {
          // Save the result and trigger the original callback. This ensures
          // that metrics are properly recorded after the screen exits.
          std::move(screen_exit_result_waiter_.GetRepeatingCallback())
              .Run(result);
          original_callback_.Run(result);
        }));

    // In the normal flow, PasswordSelection leads to FingerprintSetup. This
    // is used for stopping the flow immediately before it.
    password_selection_callback_ =
        GetPasswordSelectionScreen()->get_exit_callback_for_testing();
    GetPasswordSelectionScreen()->set_exit_callback_for_testing(
        password_selection_result_waiter_.GetRepeatingCallback());

    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowFingerprintScreen() {
    PerformLogin();
    ProceedToFingerprintScreen();
    OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();
  }

  // This method can be used to login and block the system immediately before
  // showing the FingerprintSetupScreen. Any preparation steps can be triggered
  // before invoking `ProceedToFingerprintScreen`.
  void PerformLogin() {
    // Login and skip all screens until the FingerprintSetupScreen.
    auto* context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    context->skip_post_login_screens_for_tests = true;
    login_manager_.LoginAsNewRegularUser();

    // About to show the FingerprintSetup screen.
    ASSERT_TRUE(password_selection_result_waiter_.Wait());
    context->skip_post_login_screens_for_tests = false;
  }

  void ProceedToFingerprintScreen() {
    // Unblock the PasswordSelectionScreen to show the FingerprintSetupScreen.
    // We don't explicitly wait for the screen to be shown because it might be
    // intentionally skipped, setting only its exit result.
    password_selection_callback_.Run(password_selection_result_waiter_.Take());
  }

  void WaitForScreenExit() { ASSERT_TRUE(screen_exit_result_waiter_.Wait()); }

  void EnrollFingerprint(int percent_complete) {
    base::RunLoop().RunUntilIdle();
    FakeBiodClient::Get()->SendEnrollScanDone(
        kTestFingerprintDataString, biod::SCAN_RESULT_SUCCESS,
        percent_complete == 100 /* is_complete */, percent_complete);
    base::RunLoop().RunUntilIdle();
  }

  void CheckCompletedEnroll() {
    test::OobeJS().ExpectVisiblePath(kProgressPage);
    test::OobeJS().ExpectVisiblePath(kFingerprintArc);
    test::OobeJS().CreateVisibilityWaiter(true, kDoneButton)->Wait();
    test::OobeJS().ExpectHiddenPath(kSkipButtonOnProgress);
    test::OobeJS().CreateVisibilityWaiter(true, kScanningAnimation)->Wait();
    test::OobeJS().ExpectAttributeEQ(kAssetUrlAttribute, kScanningAnimation,
                                     std::string(kCheckmarkAssetUrl));
    test::OobeJS().ExpectVisiblePath(kAddAnotherFingerButton);
  }

  std::vector<base::Bucket> GetAllRecordedUserActions() {
    return histogram_tester_.GetAllSamples(
        "OOBE.FingerprintSetupScreen.UserActions");
  }

  void ExpectResult(Result result) {
    EXPECT_EQ(screen_exit_result_waiter_.Get(), result);
    histogram_tester_.ExpectTotalCount(
        "OOBE.StepCompletionTimeByExitReason.Fingerprint-setup.Done",
        result == Result::DONE);
    histogram_tester_.ExpectTotalCount(
        "OOBE.StepCompletionTimeByExitReason.Fingerprint-setup.Skipped",
        result == Result::SKIPPED);
    histogram_tester_.ExpectTotalCount(
        "OOBE.StepCompletionTime.Fingerprint-setup",
        result != Result::NOT_APPLICABLE);
  }

 private:
  FingerprintSetupScreen::ScreenExitCallback original_callback_;
  base::test::TestFuture<FingerprintSetupScreen::Result>
      screen_exit_result_waiter_;

  PasswordSelectionScreen::ScreenExitCallback password_selection_callback_;
  base::test::TestFuture<PasswordSelectionScreen::Result>
      password_selection_result_waiter_;

  base::HistogramTester histogram_tester_;
  LoginManagerMixin login_manager_{&mixin_host_};
  std::unique_ptr<quick_unlock::TestApi> test_api_;
};

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintEnrollHalf) {
  ShowFingerprintScreen();

  EnrollFingerprint(50);
  test::OobeJS().ExpectVisiblePath(kProgressPage);
  test::OobeJS().ExpectVisiblePath(kFingerprintArc);
  test::OobeJS().ExpectVisiblePath(kSkipButtonOnProgress);
  test::OobeJS().ExpectHiddenPath(kAddAnotherFingerButton);
  test::OobeJS().ExpectHiddenPath(kDoneButton);

  test::OobeJS().TapOnPath(kSkipButtonOnProgress);

  WaitForScreenExit();
  ExpectResult(FingerprintSetupScreen::Result::SKIPPED);
  EXPECT_THAT(GetAllRecordedUserActions(),
              ElementsAre(base::Bucket(
                  static_cast<int>(UserAction::kSkipButtonClickedInFlow), 1)));
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintEnrollFull) {
  ShowFingerprintScreen();

  EnrollFingerprint(100);
  CheckCompletedEnroll();

  test::OobeJS().TapOnPath(kDoneButton);

  WaitForScreenExit();
  ExpectResult(FingerprintSetupScreen::Result::DONE);
  EXPECT_THAT(
      GetAllRecordedUserActions(),
      ElementsAre(base::Bucket(static_cast<int>(UserAction::kSetupDone), 1)));
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintEnrollLimit) {
  ShowFingerprintScreen();

  for (int i = 0; i < kMaxAllowedFingerprints - 1; i++) {
    EnrollFingerprint(100);
    CheckCompletedEnroll();
    test::OobeJS().TapOnPath(kAddAnotherFingerButton);
  }

  EnrollFingerprint(100);
  test::OobeJS().ExpectHiddenPath(kAddAnotherFingerButton);
  test::OobeJS().TapOnPath(kDoneButton);

  WaitForScreenExit();
  ExpectResult(FingerprintSetupScreen::Result::DONE);
  EXPECT_THAT(
      GetAllRecordedUserActions(),
      ElementsAre(base::Bucket(static_cast<int>(UserAction::kSetupDone), 1),
                  base::Bucket(static_cast<int>(UserAction::kAddAnotherFinger),
                               kMaxAllowedFingerprints - 1)));
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintDisabled) {
  PerformLogin();

  // Disable fingerprint (resetting flags).
  auto test_api = std::make_unique<quick_unlock::TestApi>(
      /*override_quick_unlock=*/true);

  ProceedToFingerprintScreen();

  WaitForScreenExit();
  ExpectResult(FingerprintSetupScreen::Result::NOT_APPLICABLE);
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupScreenElements) {
  ShowFingerprintScreen();

  test::OobeJS().ExpectVisiblePath(kFingerprintScreen);

  test::OobeJS().ExpectVisiblePath(kStartPage);
}

IN_PROC_BROWSER_TEST_F(FingerprintSetupTest, FingerprintSetupCancel) {
  ShowFingerprintScreen();

  test::OobeJS().CreateVisibilityWaiter(true, kStartPage)->Wait();
  test::OobeJS().TapOnPath(kSkipButtonOnStart);

  WaitForScreenExit();
  ExpectResult(FingerprintSetupScreen::Result::SKIPPED);
  EXPECT_THAT(GetAllRecordedUserActions(),
              ElementsAre(base::Bucket(
                  static_cast<int>(UserAction::kSkipButtonClickedOnStart), 1)));
}

}  // namespace ash
