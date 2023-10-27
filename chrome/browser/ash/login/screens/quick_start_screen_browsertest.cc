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
#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "content/public/test/browser_test.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

namespace {
constexpr char kWifiNetworkName[] = "wifi-test-network";
constexpr char kWelcomeScreen[] = "welcomeScreen";
constexpr char kQuickStartButton[] = "quick-start-welcome-button";
constexpr char kLoadingDialog[] = "loadingDialog";
constexpr char kCancelButton[] = "cancelButton";
constexpr char kPinCodeWrapper[] = "pinWrapper";
constexpr char kScreenOpenedHistogram[] = "QuickStart.ScreenOpened";
constexpr test::UIPath kQuickStartButtonPath = {
    WelcomeView::kScreenId.name, kWelcomeScreen, kQuickStartButton};
constexpr test::UIPath kCancelButtonLoadingDialog = {
    QuickStartView::kScreenId.name, kLoadingDialog, kCancelButton};
constexpr test::UIPath kCancelButtonVerificationDialog = {
    QuickStartView::kScreenId.name, kCancelButton};
constexpr test::UIPath kQuickStartPinCode = {QuickStartView::kScreenId.name,
                                             kPinCodeWrapper};

std::string NetworkElementSelector(const std::string& network_name) {
  return test::GetOobeElementPath(
             {"network-selection", "networkSelectLogin", "networkSelect"}) +
         ".getNetworkListItemByNameForTest('" + network_name + "')";
}

void ClickOnWifiNetwork(const std::string& wifi_network_name) {
  test::OobeJS()
      .CreateWaiter(NetworkElementSelector(wifi_network_name) + " != null")
      ->Wait();
  test::OobeJS().Evaluate(NetworkElementSelector(wifi_network_name) +
                          ".click()");
}
}  // namespace

class QuickStartBrowserTest : public OobeBaseTest {
 public:
  QuickStartBrowserTest() {
    needs_network_screen_skip_check_ = true;
    feature_list_.InitAndEnableFeature(features::kOobeQuickStart);
  }
  ~QuickStartBrowserTest() override = default;

  void SetUpOnMainThread() override {
    network_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    network_helper_.reset();
    OobeBaseTest::TearDownOnMainThread();
  }

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

  void SkipUpdateScreenOnBrandedBuilds() {
    if (!LoginDisplayHost::default_host()
             ->GetWizardContext()
             ->is_branded_build) {
      return;
    }

    test::WaitForUpdateScreen();
    test::ExitUpdateScreenNoUpdate();
  }

  // Verification step is used for both QR and PIN
  void WaitForVerificationStep() {
    test::OobeJS()
        .CreateWaiter(
            test::GetOobeElementPath({QuickStartView::kScreenId.name}) +
            ".uiStep === 'verification'")
        ->Wait();
  }

  void WaitForDefaultNetworkSubtitle() {
    WaitForNetworkListSubtitle(/*quickstart_subtitle=*/false);
  }

  void WaitForQuickStartNetworkSubtitle() {
    WaitForNetworkListSubtitle(/*quickstart_subtitle=*/true);
  }

  void WaitForNetworkListSubtitle(bool quickstart_subtitle = true) {
    auto expected_subtitle_text = l10n_util::GetStringFUTF8(
        quickstart_subtitle ? IDS_LOGIN_QUICK_START_NETWORK_NEEDED_SUBTITLE
                            : IDS_NETWORK_SELECTION_SUBTITLE,
        ui::GetChromeOSDeviceName());
    test::OobeJS()
        .CreateElementTextContentWaiter(
            expected_subtitle_text,
            {NetworkScreenView::kScreenId.name /*"network-selection"*/,
             "subtitleText"})
        ->Wait();
  }

  // Advertise, Initiate Connection, Authenticate
  void SimulatePhoneConnection() {
    connection_broker()->on_start_advertising_callback().Run(true);
    connection_broker()->InitiateConnection("fake_device_id");
    connection_broker()->AuthenticateConnection("fake_device_id");
  }

  void AbortFlowFromPhoneSide() {
    connection_broker()->CloseConnection(
        quick_start::TargetDeviceConnectionBroker::ConnectionClosedReason::
            kUserAborted);
  }

  void SimulateUserVerification(bool simulate_failure = false) {
    auto* connection = connection_broker()->GetFakeConnection();
    auto result =
        simulate_failure
            ? ash::quick_start::mojom::UserVerificationResult::kUserNotVerified
            : ash::quick_start::mojom::UserVerificationResult::kUserVerified;
    connection->VerifyUser(ash::quick_start::mojom::UserVerificationResponse(
        result, /*is_first_user_verification=*/true));
  }

  void SimulateWiFiTransfer(bool send_empty_creds = false) {
    auto* connection = connection_broker()->GetFakeConnection();
    if (send_empty_creds) {
      connection->SendWifiCredentials(absl::nullopt);
    } else {
      auto security = ash::quick_start::mojom::WifiSecurityType::kPSK;
      connection->SendWifiCredentials(ash::quick_start::mojom::WifiCredentials(
          "fake-wifi", security, /*is_hidden=*/false, "secret"));
    }
  }

  void EnsureFlowActive() { EnsureFlowState(true); }
  void EnsureFlowNotActive() { EnsureFlowState(false); }
  void EnsureFlowState(bool state) {
    EXPECT_EQ(LoginDisplayHost::default_host()
                  ->GetWizardContext()
                  ->quick_start_setup_ongoing,
              state);
    EXPECT_EQ(LoginDisplayHost::default_host()
                  ->GetWizardController()
                  ->quick_start_controller()
                  ->IsSetupOngoing(),
              state);
    ;
  }

  quick_start::FakeTargetDeviceConnectionBroker* connection_broker() {
    return connection_broker_factory_.instances().front();
  }

  void SetUpDisconnectedWifiNetwork() {
    network_helper_->device_test()->ClearDevices();
    network_helper_->service_test()->ClearServices();

    network_helper_->device_test()->AddDevice(
        "/device/stub_wifi_device", shill::kTypeWifi, "stub_wifi_device");
    network_helper_->service_test()->AddService(
        "stub_wifi", "wifi_guid", kWifiNetworkName, shill::kTypeWifi,
        shill::kStateIdle, true);
    network_helper_->service_test()->SetServiceProperty(
        "stub_wifi", shill::kConnectableProperty, base::Value(false));
    network_helper_->service_test()->SetServiceProperty(
        "stub_wifi", shill::kSecurityClassProperty,
        base::Value(shill::kSecurityClassPsk));
    network_helper_->service_test()->SetServiceProperty(
        "stub_wifi", shill::kPassphraseProperty, base::Value("secret"));
    network_helper_->profile_test()->AddService(
        ShillProfileClient::GetSharedProfilePath(), "stub_wifi");
    // Network modification notifications are posted asynchronously. Wait until
    // idle to ensure observers are notified.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  quick_start::FakeTargetDeviceConnectionBroker::Factory
      connection_broker_factory_;

 private:
  std::unique_ptr<NetworkStateTestHelper> network_helper_;
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
      quick_start::QuickStartMetrics::ScreenName::kSetUpAndroidPhone, 0);
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
      quick_start::QuickStartMetrics::ScreenName::kSetUpAndroidPhone, 1);
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
  EnsureFlowActive();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, ClickingCancelReturnsToWelcome) {
  EnterQuickStartFlowFromWelcomeScreen();

  // Cancel button must be present.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kCancelButtonLoadingDialog)
      ->Wait();
  test::OobeJS().ClickOnPath(kCancelButtonLoadingDialog);
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  EnsureFlowNotActive();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, CancelOnQRCode) {
  EnterQuickStartFlowFromWelcomeScreen();

  // Initiate connection and expect the 'verification' (QR) step.
  connection_broker()->InitiateConnection("fake_device_id");
  WaitForVerificationStep();

  // Cancel button must be present.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true,
                              kCancelButtonVerificationDialog)
      ->Wait();
  test::OobeJS().ClickOnPath(kCancelButtonVerificationDialog);
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  EnsureFlowNotActive();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, EndToEndWithMetrics) {
  SetUpDisconnectedWifiNetwork();
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kSetUpAndroidPhone, 0);
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kConnectingToWifi, 0);

  EnterQuickStartFlowFromWelcomeScreen();
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kSetUpAndroidPhone, 1);

  SimulatePhoneConnection();
  histogram_tester.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kConnectingToWifi, 1);

  SimulateUserVerification();
  SimulateWiFiTransfer();

  test::WaitForNetworkSelectionScreen();
  SkipUpdateScreenOnBrandedBuilds();

  // The flow continues to QuickStart upon reaching the UserCreationScreen.
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
}

// Simulates the phone cancelling the flow after the WiFi credentials are sent
// and while OOBE is performing its update and enrollment checks.
IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, PhoneAbortsFlowDuringUpdate) {
  // Force branded so that the update screen is shown.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  EnterQuickStartFlowFromWelcomeScreen();

  SimulatePhoneConnection();
  SimulateUserVerification();
  SimulateWiFiTransfer();

  // Simulate the phone aborting the flow during update and enrollment checks.
  test::WaitForUpdateScreen();
  AbortFlowFromPhoneSide();
  test::ExitUpdateScreenNoUpdate();

  // Abort should be handled gracefully and the standard OOBE flow is expected.
  test::WaitForUserCreationScreen();
  EnsureFlowNotActive();
}

// Full flow with empty WiFi credentials sent by the phone. Simulates the user
// manually connecting to a network and the QuickStart flow continuing normally.
IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, EndToEndWithEmptyWifiCreds) {
  // Set up a network that will be used for manually connecting.
  SetUpDisconnectedWifiNetwork();

  EnterQuickStartFlowFromWelcomeScreen();

  SimulatePhoneConnection();
  SimulateUserVerification();

  // Empty WiFi credentials
  SimulateWiFiTransfer(/*send_empty_creds=*/true);

  // When an empty WiFi is received, the user should see a subtitle telling them
  // that they need to add a connection in order to continue the setup using the
  // Android device.
  test::WaitForNetworkSelectionScreen();
  WaitForQuickStartNetworkSubtitle();

  // Manually connect to a network.
  ClickOnWifiNetwork(kWifiNetworkName);

  SkipUpdateScreenOnBrandedBuilds();

  // The flow continues to QuickStart upon reaching the UserCreationScreen.
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  EnsureFlowActive();
}

// Simulate the phone cancelling the flow when the user is prompted to connect
// to a network because the phone did not have WiFi credentials to share.
IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, PhoneAbortOnManualNetworkNeeded) {
  // Set up a network that will be used for manually connecting.
  SetUpDisconnectedWifiNetwork();

  EnterQuickStartFlowFromWelcomeScreen();
  SimulatePhoneConnection();
  SimulateUserVerification();

  // Empty WiFi credentials
  SimulateWiFiTransfer(/*send_empty_creds=*/true);

  // When an empty WiFi is received, the user should see a subtitle telling them
  // that they need to add a connection in order to continue the setup using the
  // Android device.
  test::WaitForNetworkSelectionScreen();
  WaitForQuickStartNetworkSubtitle();

  // Simulate the phone aborting the flow.
  AbortFlowFromPhoneSide();
  EnsureFlowNotActive();

  // Once QuickStart is no longer active, the subtitle on the network list must
  // show its default state.
  WaitForDefaultNetworkSubtitle();

  // Manually connect to a network.
  ClickOnWifiNetwork(kWifiNetworkName);

  SkipUpdateScreenOnBrandedBuilds();

  // Abort should be handled gracefully and the standard OOBE flow is expected.
  test::WaitForUserCreationScreen();
  EnsureFlowNotActive();
}

}  // namespace ash
