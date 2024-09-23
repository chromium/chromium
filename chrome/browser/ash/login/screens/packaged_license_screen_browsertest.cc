// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/packaged_license_screen.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/packaged_license_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

using ::testing::ElementsAre;

class PackagedLicenseScreenTest : public OobeBaseTest {
 public:
  PackagedLicenseScreenTest() {}
  ~PackagedLicenseScreenTest() override = default;

  void SetUpOnMainThread() override {
    PackagedLicenseScreen* screen = static_cast<PackagedLicenseScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            PackagedLicenseView::kScreenId));
    screen->AddExitCallbackForTesting(base::BindRepeating(
        &PackagedLicenseScreenTest::HandleScreenExit, base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void SetUpLicense(bool value) {
    ScopedDictPrefUpdate dict(local_state(), prefs::kServerBackedDeviceState);
    if (value) {
      dict->Set(policy::kDeviceStatePackagedLicense, true);
    } else {
      dict->Remove(policy::kDeviceStatePackagedLicense);
    }
  }

  void ShowPackagedLicenseScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        PackagedLicenseView::kScreenId);
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(PackagedLicenseView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CheckResult(PackagedLicenseScreen::Result result) {
    EXPECT_EQ(result_, result);
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

  base::HistogramTester histogram_tester_;

 private:
  void HandleScreenExit(PackagedLicenseScreen::Result result) {
    screen_exited_ = true;
    result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  PackagedLicenseScreen::Result result_;
  base::RepeatingClosure screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(PackagedLicenseScreenTest, DontEnroll) {
  SetUpLicense(true);
  ShowPackagedLicenseScreen();
  WaitForScreenShown();

  test::OobeJS().TapOnPath({"packaged-license", "dont-enroll-button"});

  WaitForScreenExit();
  CheckResult(PackagedLicenseScreen::Result::DONT_ENROLL);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Packaged-license.Enroll", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Packaged-license.DontEnroll", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Packaged-license",
                                     1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.StepShownStatus.Packaged-license"),
      ElementsAre(base::Bucket(
          static_cast<int>(OobeMetricsHelper::ScreenShownStatus::kShown), 1)));
}

IN_PROC_BROWSER_TEST_F(PackagedLicenseScreenTest, Enroll) {
  SetUpLicense(true);
  ShowPackagedLicenseScreen();
  WaitForScreenShown();

  test::OobeJS().TapOnPath({"packaged-license", "enroll-button"});

  WaitForScreenExit();
  CheckResult(PackagedLicenseScreen::Result::ENROLL);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Packaged-license.Enroll", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Packaged-license.DontEnroll", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Packaged-license",
                                     1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.StepShownStatus.Packaged-license"),
      ElementsAre(base::Bucket(
          static_cast<int>(OobeMetricsHelper::ScreenShownStatus::kShown), 1)));
}

IN_PROC_BROWSER_TEST_F(PackagedLicenseScreenTest, NoLicense) {
  SetUpLicense(false);
  ShowPackagedLicenseScreen();

  WaitForScreenExit();
  CheckResult(PackagedLicenseScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Packaged-license.Enroll", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Packaged-license.DontEnroll", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Packaged-license",
                                     0);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.StepShownStatus.Packaged-license"),
      ElementsAre(base::Bucket(
          static_cast<int>(OobeMetricsHelper::ScreenShownStatus::kSkipped),
          1)));
}

}  // namespace ash
