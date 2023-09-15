// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {
constexpr char kWelcomeScreen[] = "welcomeScreen";
constexpr char kQuickStartButton[] = "quick-start-welcome-button";
constexpr char kLoadingDialog[] = "loadingDialog";
constexpr char kCancelButton[] = "cancelButton";
constexpr char kWifiConnectedButton[] = "wifiConnected";
constexpr char kPinCodeWrapper[] = "pinWrapper";
constexpr char kScreenOpenedHistogram[] = "QuickStart.ScreenOpened";
constexpr test::UIPath kQuickStartButtonPath = {
    WelcomeView::kScreenId.name, kWelcomeScreen, kQuickStartButton};
constexpr test::UIPath kCancelButtonLoadingDialog = {
    QuickStartView::kScreenId.name, kLoadingDialog, kCancelButton};
constexpr test::UIPath kCancelButtonVerificationDialog = {
    QuickStartView::kScreenId.name, kCancelButton};
constexpr test::UIPath kNextButtonWifiConnectedDialog = {
    QuickStartView::kScreenId.name, kWifiConnectedButton};
constexpr test::UIPath kQuickStartPinCode = {QuickStartView::kScreenId.name,
                                             kPinCodeWrapper};
}  // namespace

class QuickStartBrowserTest : public OobeBaseTest {
 public:
  QuickStartBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kOobeQuickStart);
  }
  ~QuickStartBrowserTest() override = default;

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

  // Verification step is used for both QR and PIN
  void WaitForVerificationStep() {
    test::OobeJS()
        .CreateWaiter(
            test::GetOobeElementPath({QuickStartView::kScreenId.name}) +
            ".uiStep === 'verification'")
        ->Wait();
  }

  quick_start::FakeTargetDeviceConnectionBroker* connection_broker() {
    return connection_broker_factory_.instances().front();
  }

 protected:
  quick_start::FakeTargetDeviceConnectionBroker::Factory
      connection_broker_factory_;

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
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::quick_start_metrics::ScreenName::kSetUpAndroidPhone, 0);
  test::WaitForWelcomeScreen();
  test::OobeJS().ExpectVisiblePath(kQuickStartButtonPath);

  test::OobeJS().ClickOnPath(kQuickStartButtonPath);

  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  connection_broker_factory_.instances().front()->InitiateConnection(
      "fake_device_id");

  WaitForVerificationStep();

  int canvas_size = test::OobeJS().GetAttributeInt(
      "canvasSize_", {QuickStartView::kScreenId.name});
  EXPECT_GE(canvas_size, 185);
  EXPECT_LE(canvas_size, 265);
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::quick_start_metrics::ScreenName::kSetUpAndroidPhone, 1);
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, PinCode) {
  test::WaitForWelcomeScreen();
  test::OobeJS().ExpectVisiblePath(kQuickStartButtonPath);

  test::OobeJS().ClickOnPath(kQuickStartButtonPath);

  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  connection_broker()->set_use_pin_authentication(true);
  connection_broker()->InitiateConnection("fake_device_id");

  WaitForVerificationStep();

  // <quick-start-pin> should become visible and contain the PIN.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartPinCode)
      ->Wait();

  // Verify that PINs match.
  const auto pin = connection_broker()->GetPinForTests();
  EXPECT_EQ(pin.length(), std::size_t(4));
  for (auto i = 0; i < 4; i++) {
    const auto digit = std::string{pin[i]};
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath({QuickStartView::kScreenId.name,
                                                kPinCodeWrapper,
                                                "digit" + std::to_string(i)}) +
                      ".textContent === '" + digit + "'")
        ->Wait();
  }
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest,
                       ClickingOnButtonEntersScreenFromWelcome) {
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
  connection_broker()->InitiateConnection("fake_device_id");
  WaitForVerificationStep();

  // Cancel button must be present.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true,
                              kCancelButtonVerificationDialog)
      ->Wait();
  test::OobeJS().ClickOnPath(kCancelButtonVerificationDialog);
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, EndToEnd) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::quick_start_metrics::ScreenName::kSetUpAndroidPhone, 0);
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::quick_start_metrics::ScreenName::kConnectingToWifi, 0);

  EnterQuickStartFlowFromWelcomeScreen();
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::quick_start_metrics::ScreenName::kSetUpAndroidPhone, 1);

  // Advertise, Initiate Connection, Authenticate, Transfer WiFi
  connection_broker()->on_start_advertising_callback().Run(true);
  connection_broker()->InitiateConnection("fake_device_id");
  connection_broker()->AuthenticateConnection("fake_device_id");
  auto* connection = connection_broker()->GetFakeConnection();
  connection->VerifyUser(ash::quick_start::mojom::UserVerificationResponse(
      ash::quick_start::mojom::UserVerificationResult::kUserVerified,
      /*is_first_user_verification=*/true));
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::quick_start_metrics::ScreenName::kConnectingToWifi, 1);

  auto security = ash::quick_start::mojom::WifiSecurityType::kPSK;
  connection->SendWifiCredentials(ash::quick_start::mojom::WifiCredentials(
      "TestSSID", security, /*is_hidden=*/false, "TestPassword"));

  // 'Next' button on the WiFi connected step should be shown.
  // Clicking on it moves the flow to the network screen.
  // TODO(rrsilva) - Replace with final logic.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true,
                              kNextButtonWifiConnectedDialog)
      ->Wait();
  test::OobeJS().ClickOnPath(kNextButtonWifiConnectedDialog);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

  // Skip to the UserCreation screen where the flow will be picked up from.
  WizardController::default_controller()->AdvanceToScreen(
      UserCreationView::kScreenId);
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
}

}  // namespace ash
