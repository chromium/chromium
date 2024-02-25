// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"
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

    // Override the screen exit callback with our own method.
    FingerprintSetupScreen* fingerprint_screen =
        WizardController::default_controller()
            ->GetScreen<FingerprintSetupScreen>();
    original_callback_ = fingerprint_screen->get_exit_callback_for_testing();
    fingerprint_screen->set_exit_callback_for_testing(
        base::BindRepeating(&FingerprintSetupTest::OnFingerprintSetupScreenExit,
                            base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowFingerprintScreen() {
    PerformLogin();
    WizardController::default_controller()->AdvanceToScreen(
        FingerprintSetupScreenView::kScreenId);
    OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();
  }

  void PerformLogin() {
    OobeScreenExitWaiter signin_screen_exit_waiter(GetFirstSigninScreen());
    login_manager_.LoginAsNewRegularUser();
    signin_screen_exit_waiter.Wait();
  }

  void WaitForScreenExit() {
    if (screen_exit_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnFingerprintSetupScreenExit(Result result) {
    screen_exit_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
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
    EXPECT_EQ(screen_result_.value(), result);
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
  bool screen_exit_ = false;
  std::optional<Result> screen_result_;
  base::HistogramTester histogram_tester_;
  FingerprintSetupScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;
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

  WizardController::default_controller()->AdvanceToScreen(
      FingerprintSetupScreenView::kScreenId);

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
