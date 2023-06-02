// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/screens/quick_start_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {
constexpr char kWelcomeScreen[] = "welcomeScreen";
constexpr char kQuickStartButton[] = "quickStart";
constexpr char kLoadingDialog[] = "loadingDialog";
constexpr char kCancelButton[] = "cancelButton";
constexpr test::UIPath kQuickStartButtonPath = {
    WelcomeView::kScreenId.name, kWelcomeScreen, kQuickStartButton};
constexpr test::UIPath kCancelButtonLoadingDialog = {
    QuickStartView::kScreenId.name, kLoadingDialog, kCancelButton};
constexpr test::UIPath kCancelButtonVerificationDialog = {
    QuickStartView::kScreenId.name, kCancelButton};
}  // namespace

class QuickStartBrowserTestBase : public OobeBaseTest {
 public:
  QuickStartBrowserTestBase() = default;
  ~QuickStartBrowserTestBase() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
    quick_start::TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        &connection_broker_factory_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    quick_start::TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        nullptr);
    OobeBaseTest::TearDownInProcessBrowserTestFixture();
  }

  void EnterQuickStartFlowFromWelcomeScreen() {
    test::WaitForWelcomeScreen();
    test::OobeJS()
        .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartButtonPath)
        ->Wait();

    test::OobeJS().ClickOnPath(kQuickStartButtonPath);
    OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  }

 protected:
  quick_start::FakeTargetDeviceConnectionBroker::Factory
      connection_broker_factory_;
};

class QuickStartBrowserTest : public QuickStartBrowserTestBase {
 public:
  QuickStartBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kOobeQuickStart);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class QuickStartNotDeterminedBrowserTest : public QuickStartBrowserTest {
 public:
  QuickStartNotDeterminedBrowserTest() {
    connection_broker_factory_.set_initial_feature_support_status(
        quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
            kUndetermined);
  }
};

class QuickStartAcceleratorBrowserTest : public QuickStartBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(QuickStartNotDeterminedBrowserTest,
                       ButtonVisibleOnWelcomeScreen) {
  test::WaitForWelcomeScreen();
  test::OobeJS().ExpectHiddenPath(kQuickStartButtonPath);

  connection_broker_factory_.instances().front()->set_feature_support_status(
      quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
          kSupported);

  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartButtonPath)
      ->Wait();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, QRCode) {
  test::WaitForWelcomeScreen();
  test::OobeJS().ExpectVisiblePath(kQuickStartButtonPath);

  test::OobeJS().ClickOnPath(kQuickStartButtonPath);

  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  connection_broker_factory_.instances().front()->InitiateConnection(
      "fake_device_id");

  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath({QuickStartView::kScreenId.name}) +
                    ".uiStep === 'verification'")
      ->Wait();
  test::OobeJS().ExpectAttributeEQ("canvasSize_",
                                   {QuickStartView::kScreenId.name}, 185);
}

IN_PROC_BROWSER_TEST_F(QuickStartAcceleratorBrowserTest,
                       ButtonVisibleOnWelcomeScreen) {
  test::WaitForWelcomeScreen();
  test::OobeJS().ExpectHiddenPath(kQuickStartButtonPath);

  WizardController::default_controller()->HandleAccelerator(
      LoginAcceleratorAction::kEnableQuickStart);

  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartButtonPath)
      ->Wait();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, ClickingOnButtonEntersScreen) {
  EnterQuickStartFlowFromWelcomeScreen();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, ClickingCancelReturnsToWelcome) {
  EnterQuickStartFlowFromWelcomeScreen();

  // Cancel button must be present.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kCancelButtonLoadingDialog)
      ->Wait();
  test::OobeJS().ClickOnPath(kCancelButtonLoadingDialog);
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, CancelOnQRCode) {
  EnterQuickStartFlowFromWelcomeScreen();

  // Initiate connection and expect the 'verification' step.
  connection_broker_factory_.instances().front()->InitiateConnection(
      "fake_device_id");
  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath({QuickStartView::kScreenId.name}) +
                    ".uiStep === 'verification'")
      ->Wait();

  // Cancel button must be present.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true,
                              kCancelButtonVerificationDialog)
      ->Wait();
  test::OobeJS().ClickOnPath(kCancelButtonVerificationDialog);
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

// connection_broker_factory_.instances().front()

}  // namespace ash
