// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/network_screen.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

using ::testing::ElementsAre;

constexpr char kWifiNetworkName[] = "wifi-test-network";
constexpr char kCancelButton[] = "cancelButton";
constexpr char kLoadingDialog[] = "loadingDialog";
constexpr char kConnectingDialog[] = "connectingDialog";
constexpr char kNextButton[] = "nextButton";
constexpr char kQuickStartEntryPointVisibleHistogram[] =
    "QuickStart.EntryPointVisible";

constexpr test::UIPath kCancelButtonLoadingDialog = {
    QuickStartView::kScreenId.name, kLoadingDialog, kCancelButton};
constexpr test::UIPath kNextNetworkButtonPath = {
    NetworkScreenView::kScreenId.name /*"network-selection"*/, kNextButton};
const test::UIPath kNetworkScreenErrorSubtitile = {
    NetworkScreenView::kScreenId.name /*"network-selection"*/, "subtitleText"};
const test::UIPath kNetworkScreenConnectingDialog = {
    NetworkScreenView::kScreenId.name /*"network-selection"*/,
    kConnectingDialog};

class NetworkScreenTest : public OobeBaseTest {
 public:
  NetworkScreenTest() {
    needs_network_screen_skip_check_ = true;
  }
  NetworkScreenTest(const NetworkScreenTest&) = delete;
  NetworkScreenTest& operator=(const NetworkScreenTest&) = delete;
  ~NetworkScreenTest() override = default;

  void SetUpOnMainThread() override {
    network_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);

    network_screen_ = static_cast<NetworkScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            NetworkScreenView::kScreenId));
    network_screen_->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    network_helper_.reset();
    OobeBaseTest::TearDownOnMainThread();
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
        "stub_wifi", shill::kConnectableProperty, base::Value(true));
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

  void SetUpConnectingToWifiNetwork() {
    network_helper_->device_test()->ClearDevices();
    network_helper_->service_test()->ClearServices();

    network_helper_->device_test()->AddDevice(
        "/device/stub_wifi_device", shill::kTypeWifi, "stub_wifi_device");
    network_helper_->service_test()->AddService(
        "stub_wifi", "wifi_guid", kWifiNetworkName, shill::kTypeWifi,
        shill::kStateAssociation, true);
    network_helper_->service_test()->SetServiceProperty(
        "stub_wifi", shill::kConnectableProperty, base::Value(true));
    network_helper_->profile_test()->AddService(
        ShillProfileClient::GetSharedProfilePath(), kWifiNetworkName);

    // Network modification notifications are posted asynchronously. Wait until
    // idle to ensure observers are notified.
    base::RunLoop().RunUntilIdle();
  }

  void SetUpConnectedEthernet() {
    network_helper_->device_test()->ClearDevices();
    network_helper_->service_test()->ClearServices();
    network_helper_->device_test()->AddDevice("/device/stub_eth",
                                              shill::kTypeEthernet, "stub_eth");
    network_helper_->service_test()->AddService(
        "stub_eth", "eth_guid", "eth-test-network", shill::kTypeEthernet,
        shill::kStateOnline, true);

    // Network modification notifications are posted asynchronously. Wait until
    // idle to ensure observers are notified.
    base::RunLoop().RunUntilIdle();
  }

  std::string NetworkElementSelector(const std::string& network_name) {
    return test::GetOobeElementPath(
               {"network-selection", "networkSelectLogin", "networkSelect"}) +
           ".getNetworkListItemByNameForTest('" + network_name + "')";
  }

  void ClickOnWifiNetwork(const std::string& wifi_network_name) {
    test::OobeJS().Evaluate(NetworkElementSelector(wifi_network_name) +
                            ".click()");
  }

  void ShowNetworkScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        NetworkScreenView::kScreenId);
  }

  NetworkScreen::Result WaitForScreenExitResult() {
    return screen_result_waiter_.Take();
  }

  NetworkScreen* network_screen() { return network_screen_; }

  void WaitForErrorMessageToBeShown() {
    auto expected_subtitle_text = l10n_util::GetStringFUTF8(
        IDS_NETWORK_SELECTION_ERROR,
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME),
        base::UTF8ToUTF16(std::string_view(kWifiNetworkName)));
    test::OobeJS()
        .CreateElementTextContentWaiter(expected_subtitle_text,
                                        kNetworkScreenErrorSubtitile)
        ->Wait();
  }

  base::HistogramTester histogram_tester_;

 private:
  raw_ptr<NetworkScreen, DanglingUntriaged> network_screen_;
  base::test::TestFuture<NetworkScreen::Result> screen_result_waiter_;
  std::unique_ptr<NetworkStateTestHelper> network_helper_;
};

class NetworkScreenQuickStartEnabled : public NetworkScreenTest {
 public:
  NetworkScreenQuickStartEnabled() {
    feature_list_.InitAndEnableFeature(features::kOobeQuickStart);
    connection_broker_factory_.set_initial_feature_support_status(
        quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
            kUndetermined);
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

  void EnterQuickStartFlowFromNetworkScreen() {
    auto kQuickStartEntryPointName = l10n_util::GetStringUTF8(
        IDS_LOGIN_QUICK_START_SETUP_NETWORK_SCREEN_ENTRY_POINT);

    // Open network screen
    ShowNetworkScreen();
    OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

    // Check that QuickStart button is missing from network_selector since
    // QuickStart feature is not enabled
    test::OobeJS().ExpectTrue(
        NetworkElementSelector(kQuickStartEntryPointName) + " == null");

    connection_broker_factory_.instances().front()->set_feature_support_status(
        quick_start::TargetDeviceConnectionBroker::FeatureSupportStatus::
            kSupported);

    // Check that QuickStart button is visible since QuickStart feature is
    // enabled
    test::OobeJS()
        .CreateWaiter(NetworkElementSelector(kQuickStartEntryPointName) +
                      " != null")
        ->Wait();

    ClickOnWifiNetwork(kQuickStartEntryPointName);

    EXPECT_EQ(WaitForScreenExitResult(), NetworkScreen::Result::QUICK_START);
  }

  quick_start::FakeTargetDeviceConnectionBroker::Factory
      connection_broker_factory_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NetworkScreenQuickStartEnabled,
                       QuickStartButtonNotShownByDefault) {
  // Open network screen
  ShowNetworkScreen();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kNextNetworkButtonPath)->Wait();

  // Check that QuickStart button is missing from network_selector since
  // QuickStart feature is not enabled
  auto kQuickStartEntryPointName = l10n_util::GetStringUTF8(
      IDS_LOGIN_QUICK_START_SETUP_NETWORK_SCREEN_ENTRY_POINT);
  test::OobeJS().ExpectTrue(NetworkElementSelector(kQuickStartEntryPointName) +
                            " == null");
  histogram_tester_.ExpectBucketCount(
      kQuickStartEntryPointVisibleHistogram,
      quick_start::QuickStartMetrics::EntryPoint::NETWORK_SCREEN, 0);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenQuickStartEnabled,
                       QuickStartButtonFunctionalWhenFeatureEnabled) {
  EnterQuickStartFlowFromNetworkScreen();
  histogram_tester_.ExpectBucketCount(
      kQuickStartEntryPointVisibleHistogram,
      quick_start::QuickStartMetrics::EntryPoint::NETWORK_SCREEN, 1);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenQuickStartEnabled,
                       ClickingCancelReturnsToNetwork) {
  EnterQuickStartFlowFromNetworkScreen();

  // Cancel button must be present.
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true, kCancelButtonLoadingDialog)
      ->Wait();
  test::OobeJS().ClickOnPath(kCancelButtonLoadingDialog);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(NetworkScreenQuickStartEnabled,
                       WifiCredentialsTransfered) {
  // Prevent the test from waiting for the stabilization period.
  network_screen()->set_no_quickstart_delay_for_testing();

  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->quick_start_setup_ongoing = true;
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->quick_start_wifi_credentials = {
      kWifiNetworkName, quick_start::mojom::WifiSecurityType::kPSK,
      /*is_hidden=*/false, "secret"};
  SetUpDisconnectedWifiNetwork();
  ShowNetworkScreen();
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true,
                              kNetworkScreenConnectingDialog)
      ->Wait();
  // Wait for connection to propagate.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WaitForScreenExitResult(), NetworkScreen::Result::CONNECTED);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, CanConnect) {
  ShowNetworkScreen();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  SetUpDisconnectedWifiNetwork();
  test::OobeJS()
      .CreateWaiter(NetworkElementSelector(kWifiNetworkName) + " != null")
      ->Wait();
  ClickOnWifiNetwork(kWifiNetworkName);
  EXPECT_EQ(WaitForScreenExitResult(), NetworkScreen::Result::CONNECTED);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Timeout) {
  ShowNetworkScreen();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  SetUpConnectingToWifiNetwork();

  // Trigger timeout explicitly for test.
  network_screen()->connection_timer_.FireNow();
  WaitForErrorMessageToBeShown();
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, EthernetConnection_Skipped) {
  SetUpConnectedEthernet();
  ShowNetworkScreen();
  EXPECT_EQ(WaitForScreenExitResult(), NetworkScreen::Result::NOT_APPLICABLE);

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Network-selection.Connected", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Network-selection.OfflineDemoSetup",
      0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Network-selection.Back", 0);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Network-selection", 0);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.StepShownStatus.Network-selection"),
      ElementsAre(base::Bucket(
          static_cast<int>(OobeMetricsHelper::ScreenShownStatus::kSkipped),
          1)));
  // Next time the screen is shown it is not skipped.
  ShowNetworkScreen();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, DelayedEthernetConnection_Skipped) {
  ShowNetworkScreen();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  SetUpConnectedEthernet();
  EXPECT_EQ(WaitForScreenExitResult(), NetworkScreen::Result::CONNECTED);

  // Showing screen again to test skip doesn't work now.
  ShowNetworkScreen();
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

}  // namespace ash
