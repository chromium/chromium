// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gaia_info_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kBackButton[] = "backButton";
constexpr char kManualButton[] = "manualButton";
constexpr char kQuickStartButton[] = "quickstartButton";
constexpr char kNextButton[] = "nextButton";
constexpr char kQuickStartEntryPointVisibleHistogram[] =
    "QuickStart.EntryPointVisible";
constexpr test::UIPath kBackButtonPath = {GaiaInfoScreenView::kScreenId.name,
                                          kBackButton};
constexpr test::UIPath kManualButtonPath = {GaiaInfoScreenView::kScreenId.name,
                                            kManualButton};
constexpr test::UIPath kQuickStartButtonPath = {
    GaiaInfoScreenView::kScreenId.name, kQuickStartButton};
constexpr test::UIPath kNextButtonPath = {GaiaInfoScreenView::kScreenId.name,
                                          kNextButton};

constexpr char kCancelButton[] = "cancelButton";
constexpr test::UIPath kQuickStartCancelButtonPath = {
    QuickStartView::kScreenId.name, kCancelButton};

class GaiaInfoScreenTest : public OobeBaseTest {
 public:
  GaiaInfoScreenTest() {
    feature_list_.InitAndEnableFeature(features::kOobeGaiaInfoScreen);
  }

  ~GaiaInfoScreenTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    GaiaInfoScreen* gaia_info_screen =
        WizardController::default_controller()->GetScreen<GaiaInfoScreen>();

    original_callback_ = gaia_info_screen->get_exit_callback_for_testing();
    gaia_info_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
  }

  void ShowGaiaInfoScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        GaiaInfoScreenView::kScreenId);
  }

  GaiaInfoScreen::Result WaitForScreenExitResult() {
    GaiaInfoScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TestFuture<GaiaInfoScreen::Result> screen_result_waiter_;
  GaiaInfoScreen::ScreenExitCallback original_callback_;
};

IN_PROC_BROWSER_TEST_F(GaiaInfoScreenTest, ForwardFlow) {
  ShowGaiaInfoScreen();
  OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();
  test::OobeJS().TapOnPath(kNextButtonPath);
  EXPECT_EQ(WaitForScreenExitResult(), GaiaInfoScreen::Result::kManual);
}

IN_PROC_BROWSER_TEST_F(GaiaInfoScreenTest, BackFlow) {
  ShowGaiaInfoScreen();
  OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();
  test::OobeJS().TapOnPath(kBackButtonPath);
  EXPECT_EQ(WaitForScreenExitResult(), GaiaInfoScreen::Result::kBack);
}

class GaiaInfoScreenTestQuickStartEnabled : public GaiaInfoScreenTest {
 public:
  GaiaInfoScreenTestQuickStartEnabled() {
    feature_list_.InitAndEnableFeature(features::kOobeQuickStart);
    connection_broker_factory_.set_initial_feature_support_status(
        quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
            kUndetermined);
  }

  ~GaiaInfoScreenTestQuickStartEnabled() override = default;

  void ShowGaiaInfoScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        GaiaInfoScreenView::kScreenId);
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
    quick_start::TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        &connection_broker_factory_);
  }

  quick_start::FakeTargetDeviceConnectionBroker::Factory
      connection_broker_factory_;
  base::HistogramTester histogram_tester_;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GaiaInfoScreenTestQuickStartEnabled, ForwardFlowManual) {
  ShowGaiaInfoScreen();
  OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();

  connection_broker_factory_.instances().front()->set_feature_support_status(
      quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
          kSupported);

  histogram_tester_.ExpectBucketCount(
      kQuickStartEntryPointVisibleHistogram,
      quick_start::QuickStartMetrics::EntryPoint::GAIA_INFO_SCREEN, 1);
  test::OobeJS().ExpectDisabledPath(kNextButtonPath);

  test::OobeJS().TapOnPath(kManualButtonPath);
  test::OobeJS().ExpectHasAttribute("checked", kManualButtonPath);
  test::OobeJS().TapOnPath(kNextButtonPath);
  EXPECT_EQ(WaitForScreenExitResult(), GaiaInfoScreen::Result::kManual);
}

IN_PROC_BROWSER_TEST_F(GaiaInfoScreenTestQuickStartEnabled,
                       ForwardFlowUserEntersQuickStart) {
  ShowGaiaInfoScreen();
  OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();

  connection_broker_factory_.instances().front()->set_feature_support_status(
      quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
          kSupported);

  histogram_tester_.ExpectBucketCount(
      kQuickStartEntryPointVisibleHistogram,
      quick_start::QuickStartMetrics::EntryPoint::GAIA_INFO_SCREEN, 1);
  test::OobeJS().ExpectDisabledPath(kNextButtonPath);

  test::OobeJS().TapOnPath(kQuickStartButtonPath);
  test::OobeJS().ExpectHasAttribute("checked", kQuickStartButtonPath);
  test::OobeJS().TapOnPath(kNextButtonPath);
  EXPECT_EQ(WaitForScreenExitResult(),
            GaiaInfoScreen::Result::kEnterQuickStart);
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartCancelButtonPath)
      ->Wait();
  test::OobeJS().TapOnPath(kQuickStartCancelButtonPath);
  OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(GaiaInfoScreenTestQuickStartEnabled,
                       ForwardFlowQuickStartOngoing) {
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->quick_start_setup_ongoing = true;
  connection_broker_factory_.instances().front()->set_feature_support_status(
      quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
          kSupported);
  ShowGaiaInfoScreen();

  histogram_tester_.ExpectBucketCount(
      kQuickStartEntryPointVisibleHistogram,
      quick_start::QuickStartMetrics::EntryPoint::GAIA_INFO_SCREEN, 0);
  EXPECT_EQ(WaitForScreenExitResult(),
            GaiaInfoScreen::Result::kQuickStartOngoing);
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
}

}  // namespace
}  // namespace ash
