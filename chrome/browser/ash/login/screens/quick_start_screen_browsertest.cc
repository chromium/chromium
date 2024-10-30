// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/quick_start_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "content/public/test/browser_test.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

namespace {

using bluetooth_config::mojom::BluetoothSystemState;

constexpr char kFakeEmail[] = "testemail@gmail.com";
constexpr char kWifiNetworkName[] = "wifi-test-network";
constexpr char kWelcomeScreen[] = "welcomeScreen";
constexpr char kQuickStartEntryPoint[] = "quickStartWelcomeEntryPoint";
constexpr char kQuickStartButton[] = "quickStartButton";
constexpr char kQuickStartBluetoothDialog[] = "quickStartBluetoothDialog";
constexpr char kQuickStartBluetoothCancelButton[] =
    "quickStartBluetoothCancelButton";
constexpr char kQuickStartBluetoothEnableButton[] =
    "quickStartBluetoothEnableButton";
constexpr char kLoadingDialog[] = "loadingDialog";
constexpr char kCancelButton[] = "cancelButton";
constexpr char kPinCodeWrapper[] = "pinWrapper";
constexpr char kConfirmAccountDialog[] = "confirmAccountDialog";
constexpr char kSetupCompleteDialog[] = "setupCompleteDialog";
constexpr char kSetupCompleteHistogram[] = "QuickStart.SetupComplete";
constexpr char kScreenOpenedHistogram[] = "QuickStart.ScreenOpened";
constexpr char kViewDurationHistogram[] = ".ViewDuration";
constexpr char kReasonHistogram[] = ".Reason";
constexpr char kScreenClosedQSSetUpWithAndroidPhone[] =
    "QuickStart.ScreenClosed.QSSetUpWithAndroidPhone";
constexpr char kScreenClosedQSConnectingToWifi[] =
    "QuickStart.ScreenClosed.QSConnectingToWifi";
constexpr char kScreenClosedQSWifiCredentialsReceived[] =
    "QuickStart.ScreenClosed.QSWifiCredentialsReceived";
constexpr char kScreenClosedChooseChromebookSetup[] =
    "QuickStart.ScreenClosed.ChooseChromebookSetup";
constexpr char kScreenClosedNetworkScreen[] =
    "QuickStart.ScreenClosed.NetworkScreen";
constexpr char kAuthenticationMethodHistogram[] =
    "QuickStart.AuthenticationMethod";
constexpr char kFlowAbortedReason[] = "QuickStart.FlowAborted.Reason";
constexpr char kEntryPointHistogram[] = "QuickStart.EntryPoint";
constexpr char kEntryPointVisibleHistogram[] = "QuickStart.EntryPointVisible";

constexpr test::UIPath kQuickStartEntryPointPath = {
    WelcomeView::kScreenId.name, kWelcomeScreen, kQuickStartEntryPoint};
constexpr test::UIPath kQuickStartButtonPath = {
    WelcomeView::kScreenId.name, kWelcomeScreen, kQuickStartEntryPoint,
    kQuickStartButton};
constexpr test::UIPath kNetworkBackButtonPath = {
    NetworkScreenView::kScreenId.name, "backButton"};
constexpr test::UIPath kWelcomeScreenNextButton = {
    WelcomeView::kScreenId.name, kWelcomeScreen, "getStarted"};
constexpr test::UIPath kQuickStartBluetoothDialogPath = {
    QuickStartView::kScreenId.name, kQuickStartBluetoothDialog};
constexpr test::UIPath kQuickStartBluetoothCancelButtonPath = {
    QuickStartView::kScreenId.name, kQuickStartBluetoothCancelButton};
constexpr test::UIPath kQuickStartBluetoothEnableButtonPath = {
    QuickStartView::kScreenId.name, kQuickStartBluetoothEnableButton};
constexpr test::UIPath kCancelButtonLoadingDialog = {
    QuickStartView::kScreenId.name, kLoadingDialog, kCancelButton};
constexpr test::UIPath kCancelButtonVerificationDialog = {
    QuickStartView::kScreenId.name, kCancelButton};
constexpr test::UIPath kQuickStartPinCode = {QuickStartView::kScreenId.name,
                                             kPinCodeWrapper};
constexpr test::UIPath kQuickStartQrCodeCanvas = {
    QuickStartView::kScreenId.name, "qrCodeCanvas"};
constexpr test::UIPath kConfirmAccountDialogPath = {
    QuickStartView::kScreenId.name, kConfirmAccountDialog};
constexpr test::UIPath kSetupCompleteDialogPath = {
    QuickStartView::kScreenId.name, kSetupCompleteDialog};
constexpr test::UIPath kSetupCompleteNextButton = {
    QuickStartView::kScreenId.name, "nextButton"};
constexpr test::UIPath kCancelButtonGaiaTransferDialog = {
    QuickStartView::kScreenId.name, kCancelButton};
constexpr test::UIPath kQuickStartButtonGaia = {
    "gaia-signin", "signin-frame-dialog", "quick-start-signin-button"};

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

void WaitAndClickOnPath(const test::UIPath& path) {
  test::OobeJS().CreateVisibilityWaiter(/*visibility=*/true, path)->Wait();
  test::OobeJS().ClickOnPath(path);
}

}  // namespace

class QuickStartBrowserTest : public OobeBaseTest {
 public:
  QuickStartBrowserTest() {
    needs_network_screen_skip_check_ = true;

    // Force enable Gaia Info screen flag, which is the default behaviour
    // since adding field trial config entry for Gaia Info screen caused
    // the flag to be disabled and the info screen is not shown.
    // TODO: b/320870274 - Clean up GaiaInfoScreen flag upon
    // completion of the Gaia Info screen experiment.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kOobeQuickStart,
                              features::kOobeGaiaInfoScreen},
        /*disabled_features=*/{});
  }
  ~QuickStartBrowserTest() override = default;

  void SetUpOnMainThread() override {
    fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
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

    mock_bluetooth_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(
        mock_bluetooth_adapter_);
    ON_CALL(*mock_bluetooth_adapter_, IsPresent())
        .WillByDefault(testing::Return(true));

    SetUpBluetoothIsPoweredResponse(mock_bluetooth_adapter_, true);

    testing::Mock::AllowLeak(mock_bluetooth_adapter_.get());
  }

  void TearDownInProcessBrowserTestFixture() override {
    quick_start::TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
        nullptr);
    mock_bluetooth_adapter_.reset();
    OobeBaseTest::TearDownInProcessBrowserTestFixture();
  }

  void SetupFakeGaiaCredentialsResponse() {
    quick_start::TargetDeviceBootstrapController::GaiaCredentials gaia_creds;
    gaia_creds.auth_code = FakeGaiaMixin::kFakeAuthCode;
    gaia_creds.email = FakeGaiaMixin::kFakeUserEmail;
    gaia_creds.gaia_id = FakeGaiaMixin::kFakeAuthCode;
    quick_start::TargetDeviceBootstrapController::
        SetGaiaCredentialsResponseForTesting(gaia_creds);
  }

  void SetupAndWaitForGaiaScreen() {
    // We should be connected in order to test the entry point on the Gaia
    // screen.
    SetUpConnectedWifiNetwork();

    WaitForSigninScreen();
    WaitForGaiaPageLoad();
    OobeScreenWaiter(GaiaScreenHandler::kScreenId).Wait();
    histogram_tester_.ExpectBucketCount(
        kEntryPointVisibleHistogram,
        quick_start::QuickStartMetrics::EntryPoint::GAIA_SCREEN, 1);
  }

  void EnterQuickStartFlowFromWelcomeScreen() {
    test::WaitForWelcomeScreen();
    WaitAndClickOnPath(kQuickStartButtonPath);
    OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  }

  void EnterQuickStartFlowFromNetworkScreen() {
    test::WaitForWelcomeScreen();
    WizardController::default_controller()->AdvanceToScreen(
        NetworkScreenView::kScreenId);
    OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

    auto kQuickStartEntryPointName = l10n_util::GetStringUTF8(
        IDS_LOGIN_QUICK_START_SETUP_NETWORK_SCREEN_ENTRY_POINT);
    test::OobeJS()
        .CreateWaiter(NetworkElementSelector(kQuickStartEntryPointName) +
                      " != null")
        ->Wait();
    ClickOnWifiNetwork(kQuickStartEntryPointName);
    OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  }

  void SetUpBluetoothIsPoweredResponse(
      scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
          mock_bluetooth_adapter,
      bool is_powered) {
    ON_CALL(*mock_bluetooth_adapter, IsPowered())
        .WillByDefault(testing::Return(is_powered));
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

  void WaitForNetworkListWifiErrorTitleAndSubtitle() {
    auto expected_title =
        l10n_util::GetStringUTF8(IDS_LOGIN_QUICK_START_WIFI_ERROR_TITLE);
    auto expected_subtitle =
        l10n_util::GetStringUTF8(IDS_LOGIN_QUICK_START_WIFI_ERROR_SUBTITLE);
    test::OobeJS()
        .CreateElementTextContentWaiter(
            expected_subtitle,
            {NetworkScreenView::kScreenId.name /*"network-selection"*/,
             "subtitleText"})
        ->Wait();
    test::OobeJS()
        .CreateElementTextContentWaiter(
            expected_title,
            {NetworkScreenView::kScreenId.name /*"network-selection"*/,
             "titleText"})
        ->Wait();
  }

  void WaitForUserCreationAndTriggerPersonalFlow() {
    test::WaitForUserCreationScreen();
    test::TapForPersonalUseCrRadioButton();
    test::TapUserCreationNext();
    test::WaitForConsumerUpdateScreen();
    test::ExitConsumerUpdateScreenNoUpdate();
  }

  // Advertise, Initiate Connection, Authenticate
  void SimulatePhoneConnection() {
    connection_broker()->on_start_advertising_callback().Run(true);
    connection_broker()->InitiateConnection("fake_device_id");
    connection_broker()->AuthenticateConnection(
        "fake_device_id",
        quick_start::QuickStartMetrics::AuthenticationMethod::kQRCode);
  }

  void AbortFlowFromPhoneSide() {
    connection_broker()->CloseConnection(
        quick_start::TargetDeviceConnectionBroker::ConnectionClosedReason::
            kUnknownError);
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
      connection->SendWifiCredentials(std::nullopt);
    } else {
      auto security = ash::quick_start::mojom::WifiSecurityType::kPSK;
      connection->SendWifiCredentials(ash::quick_start::mojom::WifiCredentials(
          "fake-wifi", security, /*is_hidden=*/false, "secret"));
    }
  }

  void SimulateAccountInfoTransfer(bool send_empty_account_info = false) {
    auto* connection = connection_broker()->GetFakeConnection();
    connection->SendAccountInfo(
        send_empty_account_info ? "" : std::string{kFakeEmail});
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

  void SetUpDisconnectedWifiNetwork() { SetupNetwork(/*connected=*/false); }

  void SetUpConnectedWifiNetwork() { SetupNetwork(/*connected=*/true); }

  void SetupNetwork(bool connected = false) {
    network_helper_->device_test()->ClearDevices();
    network_helper_->service_test()->ClearServices();

    network_helper_->device_test()->AddDevice(
        "/device/stub_wifi_device", shill::kTypeWifi, "stub_wifi_device");
    network_helper_->service_test()->AddService(
        "stub_wifi", "wifi_guid", kWifiNetworkName, shill::kTypeWifi,
        connected ? shill::kStateOnline : shill::kStateIdle, true);
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
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
      mock_bluetooth_adapter_;
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<NetworkStateTestHelper> network_helper_;
  base::test::ScopedFeatureList feature_list_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

class QuickStartNotDeterminedBrowserTest : public QuickStartBrowserTest {
 public:
  QuickStartNotDeterminedBrowserTest() {
    connection_broker_factory_.set_initial_feature_support_status(
        quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
            kUndetermined);
  }
};

class QuickStartBrowserTestWithBluetoothDisabled
    : public QuickStartBrowserTest {
 public:
  QuickStartBrowserTestWithBluetoothDisabled() {}
  ~QuickStartBrowserTestWithBluetoothDisabled() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    QuickStartBrowserTest::SetUpInProcessBrowserTestFixture();
    SetUpBluetoothIsPoweredResponse(mock_bluetooth_adapter_, false);
  }

  void TearDownInProcessBrowserTestFixture() override {
    QuickStartBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void WaitForBluetoothDialogToOpen() {
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath(kQuickStartBluetoothDialogPath) +
                      ".open")
        ->Wait();
  }

  void WaitForBluetoothDialogToClose() {
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath(kQuickStartBluetoothDialogPath) +
                      ".open === false")
        ->Wait();
  }
};

IN_PROC_BROWSER_TEST_F(QuickStartNotDeterminedBrowserTest,
                       ButtonVisibleOnWelcomeScreen) {
  test::WaitForWelcomeScreen();
  test::OobeJS().ExpectHiddenPath(kQuickStartEntryPointPath);

  connection_broker_factory_.instances().front()->set_feature_support_status(
      quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
          kSupported);

  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartButtonPath)
      ->Wait();
  histogram_tester_.ExpectBucketCount(
      kEntryPointVisibleHistogram,
      quick_start::QuickStartMetrics::EntryPoint::WELCOME_SCREEN, 1);
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTestWithBluetoothDisabled,
                       BluetoothDialogIsShownAndCancellingWorks) {
  test::WaitForWelcomeScreen();

  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartButtonPath)
      ->Wait();

  EXPECT_CALL(*mock_bluetooth_adapter_, IsPowered())
      .WillRepeatedly(testing::Return(false));

  // Clicking on the entry point when bluetooth is disabled should
  // transition to the QuickStart screen and show the dialog.
  WaitAndClickOnPath(kQuickStartButtonPath);
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  WaitForVerificationStep();
  WaitForBluetoothDialogToOpen();

  // Cancelling the dialog should bring the user back.
  WaitAndClickOnPath(kQuickStartBluetoothCancelButtonPath);
  test::WaitForWelcomeScreen();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTestWithBluetoothDisabled,
                       TurningOnBluetoothFromBluetoothDialog) {
  test::WaitForWelcomeScreen();

  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kQuickStartButtonPath)
      ->Wait();

  EXPECT_CALL(*mock_bluetooth_adapter_, IsPowered())
      .WillRepeatedly(testing::Return(false));

  // Clicking on the entry point when bluetooth is disabled should
  // transition to the QuickStart screen and show the dialog.
  WaitAndClickOnPath(kQuickStartButtonPath);
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  WaitForVerificationStep();
  WaitForBluetoothDialogToOpen();

  WaitAndClickOnPath(kQuickStartBluetoothEnableButtonPath);

  WaitForBluetoothDialogToClose();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, QRCode) {
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSSetUpWithAndroidPhone, 0);
  test::WaitForWelcomeScreen();
  WaitAndClickOnPath(kQuickStartButtonPath);

  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  connection_broker_factory_.instances().front()->InitiateConnection(
      "fake_device_id");

  WaitForVerificationStep();

  // Get the true number of cells that the QR code consists of.
  const int qr_code_size = WizardController::default_controller()
                               ->quick_start_controller()
                               ->GetQrCode()
                               .GetPixelData()
                               .size();

  // Get the number of cells per row/column (CELL_COUNT) exposed on <canvas>
  const int canvas_cell_count =
      test::OobeJS().GetAttributeInt("qrCellCount", kQuickStartQrCodeCanvas);

  // Squaring the CELL_COUNT must yield the total size of the QR code.
  EXPECT_EQ(canvas_cell_count * canvas_cell_count, qr_code_size);
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSSetUpWithAndroidPhone, 1);
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, PinCode) {
  test::WaitForWelcomeScreen();
  WaitAndClickOnPath(kQuickStartButtonPath);
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
        .CreateWaiter(test::GetOobeElementPath(
                          {QuickStartView::kScreenId.name, kPinCodeWrapper,
                           "digit" + base::NumberToString(i)}) +
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
  histogram_tester_.ExpectBucketCount(
      std::string{kScreenClosedQSSetUpWithAndroidPhone} + kReasonHistogram,
      quick_start::QuickStartMetrics::ScreenClosedReason::kUserCancelled, 0);
  histogram_tester_.ExpectBucketCount(
      kFlowAbortedReason,
      quick_start::QuickStartMetrics::AbortFlowReason::USER_CLICKED_CANCEL, 0);

  // Cancel flow.
  WaitAndClickOnPath(kCancelButtonLoadingDialog);
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  EnsureFlowNotActive();
  histogram_tester_.ExpectBucketCount(
      std::string{kScreenClosedQSSetUpWithAndroidPhone} + kReasonHistogram,
      quick_start::QuickStartMetrics::ScreenClosedReason::kUserCancelled, 1);
  histogram_tester_.ExpectBucketCount(
      kFlowAbortedReason,
      quick_start::QuickStartMetrics::AbortFlowReason::USER_CLICKED_CANCEL, 1);
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, CancelOnQRCode) {
  EnterQuickStartFlowFromWelcomeScreen();

  // Initiate connection and expect the 'verification' (QR) step.
  connection_broker()->InitiateConnection("fake_device_id");
  WaitForVerificationStep();

  // Cancel flow.
  WaitAndClickOnPath(kCancelButtonVerificationDialog);
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  EnsureFlowNotActive();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, CancelAndRestartWithNewSession) {
  EnterQuickStartFlowFromWelcomeScreen();
  uint64_t first_session_id = connection_broker()->session_id();

  SimulatePhoneConnection();
  SimulateUserVerification();

  // Cancel flow.
  WaitAndClickOnPath(kCancelButtonVerificationDialog);
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  EnsureFlowNotActive();

  // Enter again and check that session info is different.
  EnterQuickStartFlowFromWelcomeScreen();
  uint64_t second_session_id = connection_broker()->session_id();
  EXPECT_NE(first_session_id, second_session_id);
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, EndToEndWithMetrics) {
  SetUpDisconnectedWifiNetwork();
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSSetUpWithAndroidPhone, 0);
  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedQSSetUpWithAndroidPhone} +
          kViewDurationHistogram,
      0);
  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedQSSetUpWithAndroidPhone} + kReasonHistogram, 0);
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSConnectingToWifi, 0);
  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedQSConnectingToWifi} + kViewDurationHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedQSConnectingToWifi} + kReasonHistogram, 0);
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSWifiCredentialsReceived,
      0);
  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedQSWifiCredentialsReceived} +
          kViewDurationHistogram,
      0);
  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedQSWifiCredentialsReceived} + kReasonHistogram,
      0);
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kChooseChromebookSetup, 0);
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSSelectGoogleAccount, 0);
  histogram_tester_.ExpectBucketCount(
      kAuthenticationMethodHistogram,
      quick_start::QuickStartMetrics::AuthenticationMethod::kQRCode, 0);
  histogram_tester_.ExpectBucketCount(
      kEntryPointHistogram,
      quick_start::QuickStartMetrics::EntryPoint::WELCOME_SCREEN, 0);

  EnterQuickStartFlowFromWelcomeScreen();

  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSSetUpWithAndroidPhone, 1);
  histogram_tester_.ExpectBucketCount(
      kEntryPointHistogram,
      quick_start::QuickStartMetrics::EntryPoint::WELCOME_SCREEN, 1);

  SimulatePhoneConnection();
  SimulateUserVerification();

  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedQSSetUpWithAndroidPhone} +
          kViewDurationHistogram,
      1);
  histogram_tester_.ExpectBucketCount(
      std::string{kScreenClosedQSSetUpWithAndroidPhone} + kReasonHistogram,
      quick_start::QuickStartMetrics::ScreenClosedReason::kAdvancedInFlow, 1);
  histogram_tester_.ExpectBucketCount(
      kAuthenticationMethodHistogram,
      quick_start::QuickStartMetrics::AuthenticationMethod::kQRCode, 1);
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSConnectingToWifi, 1);

  SimulateWiFiTransfer();

  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedQSConnectingToWifi} + kViewDurationHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      std::string{kScreenClosedQSConnectingToWifi} + kReasonHistogram,
      quick_start::QuickStartMetrics::kAdvancedInFlow, 1);
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSWifiCredentialsReceived,
      1);

  test::WaitForNetworkSelectionScreen();

  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kNetworkScreen, 1);
  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedQSWifiCredentialsReceived} +
          kViewDurationHistogram,
      1);
  histogram_tester_.ExpectBucketCount(
      std::string{kScreenClosedQSWifiCredentialsReceived} + kReasonHistogram,
      quick_start::QuickStartMetrics::ScreenClosedReason::kAdvancedInFlow, 1);

  SkipUpdateScreenOnBrandedBuilds();
  WaitForUserCreationAndTriggerPersonalFlow();

  // The flow continues to QuickStart upon reaching the GaiaInfoScreen or
  // GaiaScreen.
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();

  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedNetworkScreen} + kViewDurationHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      std::string{kScreenClosedNetworkScreen} + kReasonHistogram,
      quick_start::QuickStartMetrics::ScreenClosedReason::kAdvancedInFlow, 1);
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kChooseChromebookSetup, 1);
  histogram_tester_.ExpectTotalCount(
      std::string{kScreenClosedChooseChromebookSetup} + kViewDurationHistogram,
      1);
  histogram_tester_.ExpectBucketCount(
      std::string{kScreenClosedChooseChromebookSetup} + kReasonHistogram,
      quick_start::QuickStartMetrics::ScreenClosedReason::kAdvancedInFlow, 1);
  histogram_tester_.ExpectBucketCount(
      kScreenOpenedHistogram,
      quick_start::QuickStartMetrics::ScreenName::kQSSelectGoogleAccount, 1);
}

// Simulates the phone cancelling the flow after the WiFi credentials are sent
// and while OOBE continues in between Quick Start screens.
IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, PhoneAbortsFlowOnUpdateScreen) {
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
  WaitForUserCreationAndTriggerPersonalFlow();
  test::WaitForGaiaInfoScreen();

  // Abort should be handled gracefully and the standard OOBE flow is expected.
  EnsureFlowNotActive();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest,
                       PhoneAbortsFlowOnUserCreationScreen) {
  // Force branded so that the update screen is shown.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  EnterQuickStartFlowFromWelcomeScreen();

  SimulatePhoneConnection();
  SimulateUserVerification();
  SimulateWiFiTransfer();

  test::WaitForUpdateScreen();
  test::ExitUpdateScreenNoUpdate();
  test::WaitForUserCreationScreen();
  AbortFlowFromPhoneSide();

  test::TapForPersonalUseCrRadioButton();
  test::TapUserCreationNext();
  test::WaitForConsumerUpdateScreen();
  test::ExitConsumerUpdateScreenNoUpdate();
  test::WaitForGaiaInfoScreen();

  // Abort should be handled gracefully and the standard OOBE flow is expected.
  EnsureFlowNotActive();
}

IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest,
                       PhoneAbortsFlowOnConsumerUpdateScreen) {
  // Force branded so that the update screen is shown.
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  EnterQuickStartFlowFromWelcomeScreen();

  SimulatePhoneConnection();
  SimulateUserVerification();
  SimulateWiFiTransfer();

  test::WaitForUpdateScreen();
  test::ExitUpdateScreenNoUpdate();
  test::WaitForUserCreationScreen();
  test::TapForPersonalUseCrRadioButton();
  test::TapUserCreationNext();
  test::WaitForConsumerUpdateScreen();
  AbortFlowFromPhoneSide();

  test::ExitConsumerUpdateScreenNoUpdate();
  test::WaitForGaiaInfoScreen();

  // Abort should be handled gracefully and the standard OOBE flow is expected.
  EnsureFlowNotActive();
}

// Full flow with empty WiFi credentials sent by the phone. Simulates the user
// manually connecting to a network and the QuickStart flow continuing
// normally.
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
  WaitForUserCreationAndTriggerPersonalFlow();

  // The flow continues to QuickStart upon reaching the GaiaInfoScreen or
  // GaiaScreen.
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  EnsureFlowActive();
}

// Tests that the entry point for QuickStart is hidden while the network screen
// is being used to show a list of networks when the flow started on the Welcome
// screen.
IN_PROC_BROWSER_TEST_F(
    QuickStartBrowserTest,
    NoEntryPointWhileShowingNetworkListWhenStartingOnWelcome) {
  auto kQuickStartEntryPointName = l10n_util::GetStringUTF8(
      IDS_LOGIN_QUICK_START_SETUP_NETWORK_SCREEN_ENTRY_POINT);
  // Set up a network that will be used for manually connecting.
  SetUpDisconnectedWifiNetwork();

  EnterQuickStartFlowFromWelcomeScreen();

  SimulatePhoneConnection();
  SimulateUserVerification();

  // Send empty WiFi credentials to trigger the network list step.
  SimulateWiFiTransfer(/*send_empty_creds=*/true);

  // Expect the network screen to be shown without the QuickStart entry point.
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  test::OobeJS().ExpectTrue(NetworkElementSelector(kQuickStartEntryPointName) +
                            " == null");
}

// Tests that the entry point for QuickStart is hidden while the network screen
// is being used to show a list of networks when the flow started on the network
// screen itself.
IN_PROC_BROWSER_TEST_F(
    QuickStartBrowserTest,
    NoEntryPointWhileShowingNetworkListWhenStartingOnNetwork) {
  auto kQuickStartEntryPointName = l10n_util::GetStringUTF8(
      IDS_LOGIN_QUICK_START_SETUP_NETWORK_SCREEN_ENTRY_POINT);
  // Set up a network that will be used for manually connecting.
  SetUpDisconnectedWifiNetwork();

  EnterQuickStartFlowFromNetworkScreen();

  SimulatePhoneConnection();
  SimulateUserVerification();

  // Send empty WiFi credentials to trigger the network list step.
  SimulateWiFiTransfer(/*send_empty_creds=*/true);

  // Expect the network screen to be shown without the QuickStart entry point.
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  test::OobeJS()
      .CreateWaiter(NetworkElementSelector(kQuickStartEntryPointName) +
                    " == null")
      ->Wait();
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

  // When the flow is aborted from the phone side on the network screen, there
  // should be custom strings informing the user about the error.
  WaitForNetworkListWifiErrorTitleAndSubtitle();

  // Clicking on 'Back' should bring us back to the Welcome screen, and entering
  // it again should show the default strings.
  WaitAndClickOnPath(kNetworkBackButtonPath);
  test::WaitForWelcomeScreen();
  WaitAndClickOnPath(kWelcomeScreenNextButton);
  WaitForDefaultNetworkSubtitle();

  // Manually connect to a network.
  ClickOnWifiNetwork(kWifiNetworkName);

  SkipUpdateScreenOnBrandedBuilds();

  // Abort should be handled gracefully and the standard OOBE flow is expected.
  WaitForUserCreationAndTriggerPersonalFlow();
  EnsureFlowNotActive();
}

// Test that the flow can be started on the Gaia screen and also cancelled.
IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest,
                       GaiaEntryPoint_StartAndCancelFlow) {
  SetupAndWaitForGaiaScreen();

  WaitAndClickOnPath(kQuickStartButtonGaia);
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  EnsureFlowActive();

  histogram_tester_.ExpectBucketCount(
      std::string{kScreenClosedQSSetUpWithAndroidPhone} + kReasonHistogram,
      quick_start::QuickStartMetrics::ScreenClosedReason::kUserCancelled, 0);

  // Cancel the flow.
  WaitAndClickOnPath(kCancelButtonLoadingDialog);

  histogram_tester_.ExpectBucketCount(
      std::string{kScreenClosedQSSetUpWithAndroidPhone} + kReasonHistogram,
      quick_start::QuickStartMetrics::ScreenClosedReason::kUserCancelled, 1);
  // Returns to the Gaia screen
  OobeScreenWaiter(GaiaScreenHandler::kScreenId).Wait();
}

// Test that the flow can be started on the Gaia screen and also cancelled.
IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest,
                       GaiaEntryPoint_TransfersGaiaCredentialsOnceConnected) {
  SetupAndWaitForGaiaScreen();
  WaitAndClickOnPath(kQuickStartButtonGaia);
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  EnsureFlowActive();

  SimulatePhoneConnection();
  SimulateUserVerification();

  // "Confirm your account" step should become visible
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kConfirmAccountDialogPath)
      ->Wait();

  // Cancel and return to the Gaia screen.
  WaitAndClickOnPath(kCancelButtonGaiaTransferDialog);
  OobeScreenWaiter(GaiaScreenHandler::kScreenId).Wait();
}

// Test the correct behavior when there are no accounts on the phone.
IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, HandleEmptyAccounts) {
  SetupAndWaitForGaiaScreen();
  WaitAndClickOnPath(kQuickStartButtonGaia);
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  EnsureFlowActive();

  SimulatePhoneConnection();
  SimulateUserVerification();

  // Receiving an empty email from the phone will abort the flow.
  SimulateAccountInfoTransfer(/*send_empty_account_info=*/true);

  // Returns to the Gaia screen
  OobeScreenWaiter(GaiaScreenHandler::kScreenId).Wait();
}

// Goes through the full flow of QuickStart simulating the non-fallback flow.
IN_PROC_BROWSER_TEST_F(QuickStartBrowserTest, FullFlow) {
  SetupFakeGaiaCredentialsResponse();

  SetupAndWaitForGaiaScreen();
  WaitAndClickOnPath(kQuickStartButtonGaia);
  OobeScreenWaiter(QuickStartView::kScreenId).Wait();
  EnsureFlowActive();

  SimulatePhoneConnection();
  SimulateUserVerification();

  SimulateAccountInfoTransfer(/*send_empty_account_info=*/false);

  OobeScreenWaiter(QuickStartView::kScreenId).Wait();

  // "Setup complete" step should become visible.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kSetupCompleteDialogPath)
      ->Wait();
  histogram_tester_.ExpectBucketCount(kSetupCompleteHistogram, true, 1);

  // Ensure that there is a SessionRefresher on the QuickStart screen keeping
  // the AuthSession alive.
  EXPECT_TRUE(AuthSessionStorage::Get()->CheckHasKeepAliveForTesting(
      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->extra_factors_token.value()));

  // Clicking Next should bring the user to the Consolidated Consent screen.
  WaitAndClickOnPath(kSetupCompleteNextButton);
  test::WaitForConsolidatedConsentScreen();
}

class QuickStartLoginScreenTest : public QuickStartBrowserTest {
 public:
  QuickStartLoginScreenTest() : QuickStartBrowserTest() {
    login_manager_mixin_.AppendRegularUsers(1);
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(QuickStartLoginScreenTest, EntryPointNotVisible) {
  SetupNetwork(/*connected=*/true);
  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();

  WaitAndClickOnPath({"user-creation", "selfButton"});
  WaitAndClickOnPath({"user-creation", "nextButton"});

  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  base::RunLoop().RunUntilIdle();
  test::OobeJS().ExpectHiddenPath(kQuickStartButtonGaia);
}

}  // namespace ash
