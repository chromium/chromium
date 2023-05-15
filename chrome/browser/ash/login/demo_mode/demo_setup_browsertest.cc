// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"

#include <string>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_to_iso8601.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/ash/login/mock_network_state_helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/demo_setup_screen.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

using test::DemoModeSetupResult;
using test::SetupDummyOfflinePolicyDir;

constexpr char kConsolidatedConsentId[] = "consolidated-consent";
constexpr char kDemoSetupId[] = "demo-setup";
constexpr char kDemoPrefsId[] = "demo-preferences";
constexpr char kNetworkId[] = "network-selection";
constexpr char kWelcomeId[] = "connect";

const test::UIPath kDemoConfirmationDialog = {kWelcomeId,
                                              "demoModeConfirmationDialog"};
const test::UIPath kDemoConfirmationOkButton = {kWelcomeId, "okButton"};
const test::UIPath kDemoConfirmationCancelButton = {kWelcomeId, "cancelButton"};

const test::UIPath kDemoPreferencesScreen = {kDemoPrefsId};
const test::UIPath kDemoPreferencesCountry = {kDemoPrefsId, "countrySelect"};
const test::UIPath kDemoPreferencesCountrySelect = {kDemoPrefsId,
                                                    "countrySelect", "select"};
const test::UIPath kDemoPreferencesRetailerName = {kDemoPrefsId,
                                                   "retailerNameInput"};
const test::UIPath kDemoPreferencesStoreNumber = {kDemoPrefsId,
                                                  "storeNumberInput"};
const test::UIPath kDemoPreferencesStoreNumberInputDisplayMessage = {
    kDemoPrefsId, "store-number-input-display-text"};
const test::UIPath kDemoPreferencesNext = {kDemoPrefsId, "nextButton"};

const test::UIPath kNetworkScreen = {kNetworkId};
const test::UIPath kNetworkNextButton = {kNetworkId, "nextButton"};
const test::UIPath kNetworkBackButton = {kNetworkId, "backButton"};

const test::UIPath kDemoSetupProgressDialog = {kDemoSetupId,
                                               "demoSetupProgressDialog"};
const test::UIPath kDemoSetupErrorDialog = {kDemoSetupId,
                                            "demoSetupErrorDialog"};
const test::UIPath kDemoSetupErrorDialogRetry = {kDemoSetupId, "retryButton"};
const test::UIPath kDemoSetupErrorDialogPowerwash = {kDemoSetupId,
                                                     "powerwashButton"};
const test::UIPath kDemoSetupErrorDialogBack = {kDemoSetupId, "back"};
const test::UIPath kDemoSetupErrorDialogMessage = {kDemoSetupId,
                                                   "errorMessage"};

const test::UIPath kCCAcceptButton = {kConsolidatedConsentId, "acceptButton"};
const test::UIPath kCCArcTosLink = {kConsolidatedConsentId, "arcTosLink"};
const test::UIPath kCCBackButton = {kConsolidatedConsentId, "backButton"};

constexpr char kDefaultNetworkServicePath[] = "/service/eth1";
constexpr char kDefaultNetworkName[] = "eth1";

constexpr int kInvokeDemoModeGestureTapsCount = 10;

// Basic tests for demo mode setup flow.
class DemoSetupTestBase : public OobeBaseTest {
 public:
  DemoSetupTestBase() = default;

  DemoSetupTestBase(const DemoSetupTestBase&) = delete;
  DemoSetupTestBase& operator=(const DemoSetupTestBase&) = delete;

  ~DemoSetupTestBase() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    update_engine_client()->set_update_check_result(
        UpdateEngineClient::UPDATE_RESULT_FAILED);
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
    DisconnectAllNetworks();
  }

  void IsConfirmationDialogShown() {
    test::OobeJS().ExpectDialogOpen(kDemoConfirmationDialog);
  }

  void IsConfirmationDialogHidden() {
    test::OobeJS().ExpectDialogClosed(kDemoConfirmationDialog);
  }

  void ClickOkOnConfirmationDialog() {
    test::OobeJS().TapOnPath(kDemoConfirmationOkButton);
  }

  void ClickCancelOnConfirmationDialog() {
    test::OobeJS().TapOnPath(kDemoConfirmationCancelButton);
  }

  void TriggerDemoModeOnWelcomeScreen() {
    // Start Demo Mode by initiate demo set up controller and showing the first
    // network screen in demo mode setup flow.
    test::WaitForWelcomeScreen();
    IsConfirmationDialogHidden();

    InvokeDemoModeWithTaps();
    IsConfirmationDialogShown();

    ClickOkOnConfirmationDialog();

    OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  }

  // Returns whether error message is shown on demo setup error screen and
  // contains text consisting of strings identified by `error_message_id` and
  // `recovery_message_id`.
  void ExpectErrorMessage(int error_message_id, int recovery_message_id) {
    const std::string expected_message =
        base::StrCat({l10n_util::GetStringUTF8(error_message_id), " ",
                      l10n_util::GetStringUTF8(recovery_message_id)});

    test::OobeJS().ExpectVisiblePath(kDemoSetupErrorDialogMessage);
    test::OobeJS().ExpectElementText(expected_message,
                                     kDemoSetupErrorDialogMessage);
  }

  void InvokeDemoModeWithAccelerator() {
    WizardController::default_controller()->HandleAccelerator(
        LoginAcceleratorAction::kStartDemoMode);
  }

  void InvokeDemoModeWithTaps() {
    MultiTapOobeContainer(kInvokeDemoModeGestureTapsCount);
  }

  // Simulates multi-tap gesture that consists of `tapCount` clicks on the OOBE
  // outer-container.
  void MultiTapOobeContainer(int tapsCount) {
    const std::string query = base::StrCat(
        {"for (var i = 0; i < ", base::NumberToString(tapsCount), "; ++i)",
         "{ document.querySelector('#outer-container').click(); }"});
    test::ExecuteOobeJS(query);
  }

  // Simulates click on the network list item. `element` should specify
  // the aria-label of the desired network-list-item.
  void ClickNetworkListElement(const std::string& name) {
    const std::string element =
        base::StrCat({test::GetOobeElementPath(kNetworkScreen),
                      ".getNetworkListItemByNameForTest('", name, "')"});
    // We are looking up element by localized text. In Polymer v2 we might
    // get to this point when element still not have proper localized string,
    // and getNetworkListItemByNameForTest would return null.
    test::OobeJS().CreateWaiter(element)->Wait();
    test::OobeJS().CreateVisibilityWaiter(true, element)->Wait();

    const std::string query = base::StrCat({element, ".click()"});
    test::ExecuteOobeJS(query);
  }

  void UseOnlineModeOnNetworkScreen() {
    test::WaitForNetworkSelectionScreen();
    // Wait until default network is connected.
    test::OobeJS().CreateEnabledWaiter(true, kNetworkNextButton)->Wait();
    test::OobeJS().ClickOnPath(kNetworkNextButton);
  }

  // Simulates device being connected to the network.
  void SimulateNetworkConnected() {
    ShillServiceClient::TestInterface* service =
        ShillServiceClient::Get()->GetTestInterface();
    service->SetServiceProperty(kDefaultNetworkServicePath,
                                shill::kStateProperty,
                                base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

  // Simulates device being disconnected from the network.
  void SimulateNetworkDisconnected() {
    ShillServiceClient::TestInterface* service =
        ShillServiceClient::Get()->GetTestInterface();
    service->SetServiceProperty(kDefaultNetworkServicePath,
                                shill::kStateProperty,
                                base::Value(shill::kStateIdle));
    base::RunLoop().RunUntilIdle();
  }

  // Sets all network services into idle state.
  void DisconnectAllNetworks() {
    NetworkStateHandler::NetworkStateList networks;
    NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
        NetworkTypePattern::Default(),
        true,   // configured_only
        false,  // visible_only,
        0,      // no limit to number of results
        &networks);
    ShillServiceClient::TestInterface* service =
        ShillServiceClient::Get()->GetTestInterface();
    for (const auto* const network : networks) {
      service->SetServiceProperty(network->path(), shill::kStateProperty,
                                  base::Value(shill::kStateIdle));
    }
    base::RunLoop().RunUntilIdle();
  }

  void ProceedThroughDemoPreferencesScreen() {
    SetAndVerifyValidRetailerNameAndStoreNumber("Retailer", "1234");
    test::OobeJS().ClickOnPath(kDemoPreferencesNext);
  }

  // Type in valid input and verify that the "continue" button is enabled.
  void SetAndVerifyValidRetailerNameAndStoreNumber(
      const std::string& expected_retailer_name,
      const std::string& expected_store_number) {
    test::OobeJS().TypeIntoPath(expected_retailer_name,
                                kDemoPreferencesRetailerName);
    test::OobeJS().TypeIntoPath(expected_store_number,
                                kDemoPreferencesStoreNumber);
    test::OobeJS().ExpectEnabledPath(kDemoPreferencesNext);
  }

  // Sets fake time in MultiTapDetector to remove dependency on real time in
  // test environment.
  void SetFakeTimeForMultiTapDetector(base::Time fake_time) {
    const std::string query =
        base::StrCat({"MultiTapDetector.setFakeTimeForTests(new Date('",
                      base::TimeToISO8601(fake_time), "'));"});
    test::ExecuteOobeJS(query);
  }

  DemoSetupScreen* GetDemoSetupScreen() {
    return static_cast<DemoSetupScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            DemoSetupScreenView::kScreenId));
  }

 protected:
  test::EnrollmentHelperMixin enrollment_helper_{&mixin_host_};
  base::HistogramTester histogram_tester_;

 private:
  // TODO(agawronska): Maybe create a separate test fixture for offline setup.
  base::ScopedTempDir fake_demo_resources_dir_;
  policy::MockCloudPolicyStore mock_policy_store_;
  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;
};

class DemoSetupArcSupportedTest : public DemoSetupTestBase {
 public:
  DemoSetupArcSupportedTest() {
    statistics_provider_.SetMachineStatistic(system::kRegionKey, "us");
    statistics_provider_.SetVpdStatus(
        system::StatisticsProvider::VpdStatus::kValid);
  }
  ~DemoSetupArcSupportedTest() override = default;

  // DemoSetupTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DemoSetupTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    ASSERT_TRUE(arc::IsArcAvailable());
  }

  void WaitForConsolidatedConsentScreen() {
    test::WaitForConsolidatedConsentScreen();

    // Make sure that ARC ToS link is visible.
    test::OobeJS().ExpectVisiblePath(kCCArcTosLink);
    test::OobeJS().CreateVisibilityWaiter(true, kCCAcceptButton)->Wait();
    test::OobeJS().ExpectVisiblePath(kCCAcceptButton);
  }

  // Type in invalid input and the "continue" button is disabled.
  void SetAndVerifyInvalidRetailerNameAndStoreNumber(
      const std::string& expected_retailer_name,
      const std::string& expected_store_number) {
    test::OobeJS().TypeIntoPath(expected_retailer_name,
                                kDemoPreferencesRetailerName);
    test::OobeJS().TypeIntoPath(expected_store_number,
                                kDemoPreferencesStoreNumber);
    test::OobeJS().ExpectDisabledPath(kDemoPreferencesNext);
  }

  void AcceptTermsAndExpectDemoSetupProgress() {
    test::LockDemoDeviceInstallAttributes();
    // TODO(b/246012796): If possible, re-enable waiting on the setup screen to
    // be shown
    WaitForConsolidatedConsentScreen();
    test::TapConsolidatedConsentAccept();
  }

  void AcceptTermsAndExpectDemoSetupFailure() {
    WaitForConsolidatedConsentScreen();
    test::TapConsolidatedConsentAccept();

    // As we expect the error message to stay on the screen, it is safe to
    // wait for it in the usual manner.
    OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
    test::OobeJS().CreateVisibilityWaiter(true, kDemoSetupErrorDialog)->Wait();
  }

  std::string GetQueryForCountrySelectOptionFromCountryCode(
      const std::string country_code) {
    return base::StrCat({test::GetOobeElementPath(kDemoPreferencesCountry),
                         ".shadowRoot.querySelector('option[value=\"",
                         country_code, "\"]').innerHTML"});
  }

 protected:
  // Verify the country names are displayed correctly. Regression test for
  // potential country code changes.
  const base::flat_map<std::string, std::string> kCountryCodeToNameMap = {
      {"US", "United States"},
      {"AT", "Austria"},
      {"AU", "Australia"},
      {"BE", "Belgium"},
      {"BR", "Brazil"},
      {"CA", "Canada"},
      {"DE", "Germany"},
      {"DK", "Denmark"},
      {"ES", "Spain"},
      {"FI", "Finland"},
      {"FR", "France"},
      {"GB", "United Kingdom"},
      {"IE", "Ireland"},
      {"IN", "India"},
      {"IT", "Italy"},
      {"JP", "Japan"},
      {"LU", "Luxembourg"},
      {"MX", "Mexico"},
      {"N/A", "Please select a country"},
      {"NL", "Netherlands"},
      {"NO", "Norway"},
      {"NZ", "New Zealand"},
      {"PL", "Poland"},
      {"PT", "Portugal"},
      {"SE", "Sweden"},
      {"ZA", "South Africa"}};

  system::ScopedFakeStatisticsProvider statistics_provider_;

  void PopulateDemoPreferencesAndFinishSetup() {
    // Select France as the Demo Mode country and test retailer name and store
    // number.
    test::OobeJS().SelectElementInPath("FR", kDemoPreferencesCountrySelect);
    ProceedThroughDemoPreferencesScreen();

    AcceptTermsAndExpectDemoSetupProgress();

    // Verify the email corresponds to France.
    EXPECT_EQ("admin-fr@cros-demo-mode.com",
              DemoSetupController::GetSubOrganizationEmail());

    // LoginOrLockScreen is shown at beginning of OOBE, so we need to wait until
    // it's shown again when Demo setup completes.
    LoginOrLockScreenVisibleWaiter().WaitEvenIfShown();

    EXPECT_TRUE(StartupUtils::IsOobeCompleted());
    EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  }
};

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       ShowConfirmationDialogAndProceed) {
  WaitForOobeUI();
  IsConfirmationDialogHidden();

  InvokeDemoModeWithAccelerator();
  IsConfirmationDialogShown();

  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       ShowConfirmationDialogAndCancel) {
  WaitForOobeUI();
  IsConfirmationDialogHidden();

  InvokeDemoModeWithAccelerator();
  IsConfirmationDialogShown();

  ClickCancelOnConfirmationDialog();
  IsConfirmationDialogHidden();

  test::OobeJS().ExpectHiddenPath(kDemoPreferencesScreen);
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, InvokeWithTaps) {
  WaitForOobeUI();
  // Use fake time to avoid flakiness.
  SetFakeTimeForMultiTapDetector(base::Time::UnixEpoch());
  IsConfirmationDialogHidden();

  MultiTapOobeContainer(10);
  IsConfirmationDialogShown();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       DoNotInvokeWithNonConsecutiveTaps) {
  // Use fake time to avoid flakiness.
  const base::Time kFakeTime = base::Time::UnixEpoch();
  SetFakeTimeForMultiTapDetector(kFakeTime);
  IsConfirmationDialogHidden();

  MultiTapOobeContainer(5);
  IsConfirmationDialogHidden();

  // Advance time to make interval in between taps longer than expected by
  // multi-tap gesture detector.
  SetFakeTimeForMultiTapDetector(kFakeTime + base::Milliseconds(500));

  MultiTapOobeContainer(5);
  IsConfirmationDialogHidden();
}

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OnlineSetupFlowSuccess DISABLED_OnlineSetupFlowSuccess
#else
#define MAYBE_OnlineSetupFlowSuccess OnlineSetupFlowSuccess
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_OnlineSetupFlowSuccess) {
  // Simulate successful online setup.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  ProceedThroughDemoPreferencesScreen();

  AcceptTermsAndExpectDemoSetupProgress();

  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Consolidated-consent", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepShownStatus.Consolidated-consent", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Consolidated-consent."
      "AcceptedDemo",
      1);

  // Verify the email corresponds to US.
  EXPECT_EQ("admin-us@cros-demo-mode.com",
            DemoSetupController::GetSubOrganizationEmail());

  // LoginOrLockScreen is shown at beginning of OOBE, so we need to wait until
  // it's shown again when Demo setup completes.
  LoginOrLockScreenVisibleWaiter().WaitEvenIfShown();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       OnlineSetupFlowSuccessWithCountryCustomization) {
  // Simulate successful online setup.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  for (const std::string country_code : DemoSession::kSupportedCountries) {
    const auto it = kCountryCodeToNameMap.find(country_code);
    ASSERT_NE(kCountryCodeToNameMap.end(), it);
    const std::string query =
        GetQueryForCountrySelectOptionFromCountryCode(country_code);
    EXPECT_EQ(it->second, test::OobeJS().GetString(query));
  }

  SetAndVerifyValidRetailerNameAndStoreNumber("Retailer", "1234");

  // Expect active "OK" button with "US" selected as country.
  test::OobeJS().ExpectEnabledPath(kDemoPreferencesNext);
  test::OobeJS().ExpectElementValue("US", kDemoPreferencesCountrySelect);

  PopulateDemoPreferencesAndFinishSetup();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       OnlineSetupFlowSuccessWithValidRetailerAndStore) {
  // Simulate successful online setup.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  // Test a couple valid inputs, verify the "continue" button is enabled.
  SetAndVerifyValidRetailerNameAndStoreNumber("Ret@iler with $ymb0ls", "0000");
  SetAndVerifyValidRetailerNameAndStoreNumber("R", "1");
  SetAndVerifyValidRetailerNameAndStoreNumber("Retailer", "1234");

  test::OobeJS().ExpectElementText(
      l10n_util::GetStringUTF8(
          IDS_OOBE_DEMO_SETUP_PREFERENCES_STORE_NUMBER_INPUT_HELP_TEXT),
      kDemoPreferencesStoreNumberInputDisplayMessage);

  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  AcceptTermsAndExpectDemoSetupProgress();

  EXPECT_EQ("admin-us@cros-demo-mode.com",
            DemoSetupController::GetSubOrganizationEmail());
  // LoginOrLockScreen is shown at beginning of OOBE, so we need to wait until
  // it's shown again when Demo setup completes.
  LoginOrLockScreenVisibleWaiter().WaitEvenIfShown();

  // Verify that pref value has been normalized to uppercase.
  EXPECT_EQ("retailer", g_browser_process->local_state()->GetString(
                            prefs::kDemoModeRetailerId));
  EXPECT_EQ("1234", g_browser_process->local_state()->GetString(
                        prefs::kDemoModeStoreId));

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       OnlineSetupNoEnrollmentWithInvalidRetailerAndStore) {
  // Simulate demo online setup not finished.
  enrollment_helper_.ExpectNoEnrollment();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  test::OobeJS().ExpectElementText(
      l10n_util::GetStringUTF8(
          IDS_OOBE_DEMO_SETUP_PREFERENCES_STORE_NUMBER_INPUT_HELP_TEXT),
      kDemoPreferencesStoreNumberInputDisplayMessage);
  test::OobeJS().ExpectDisabledPath(kDemoPreferencesNext);

  SetAndVerifyInvalidRetailerNameAndStoreNumber("ValidRetailer", "NotANumber");
  SetAndVerifyInvalidRetailerNameAndStoreNumber("", "1234");
  SetAndVerifyInvalidRetailerNameAndStoreNumber("ValidRetailer", "");
  SetAndVerifyInvalidRetailerNameAndStoreNumber("ValidRetailer", "1234a");
  SetAndVerifyInvalidRetailerNameAndStoreNumber("ValidRetailer", "12-34");

  // Verify that continue button goes back to being disabled after enabled
  // for correct input
  SetAndVerifyValidRetailerNameAndStoreNumber("ValidRetailer", "1234");
  SetAndVerifyInvalidRetailerNameAndStoreNumber("", "");

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, OnlineSetupFlowErrorDefault) {
  // Simulate online setup failure.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  ProceedThroughDemoPreferencesScreen();

  AcceptTermsAndExpectDemoSetupFailure();

  // Default error returned by MockDemoModeOnlineEnrollmentHelperCreator.
  ExpectErrorMessage(IDS_DEMO_SETUP_TEMPORARY_ERROR,
                     IDS_DEMO_SETUP_RECOVERY_RETRY);

  test::OobeJS().ExpectVisiblePath(kDemoSetupErrorDialogRetry);
  test::OobeJS().ExpectHiddenPath(kDemoSetupErrorDialogPowerwash);
  test::OobeJS().ExpectEnabledPath(kDemoSetupErrorDialogBack);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       OnlineSetupFlowErrorPowerwashRequired) {
  // Simulate online setup failure that requires powerwash.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForLockError(
          InstallAttributes::LOCK_ALREADY_LOCKED));
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  ProceedThroughDemoPreferencesScreen();

  AcceptTermsAndExpectDemoSetupFailure();

  ExpectErrorMessage(IDS_DEMO_SETUP_ALREADY_LOCKED_ERROR,
                     IDS_DEMO_SETUP_RECOVERY_POWERWASH);

  test::OobeJS().ExpectHiddenPath(kDemoSetupErrorDialogRetry);
  test::OobeJS().ExpectVisiblePath(kDemoSetupErrorDialogPowerwash);
  test::OobeJS().ExpectDisabledPath(kDemoSetupErrorDialogBack);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       OnlineSetupFlowCrosComponentFailure) {
  // Simulate failure to load demo resources CrOS component.
  // There is no enrollment attempt, as process fails earlier.
  enrollment_helper_.ExpectNoEnrollment();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  // Set the component to fail to install when requested.
  WizardController::default_controller()
      ->demo_setup_controller()
      ->SetCrOSComponentLoadErrorForTest(
          component_updater::CrOSComponentManager::Error::INSTALL_FAILURE);

  ProceedThroughDemoPreferencesScreen();

  AcceptTermsAndExpectDemoSetupFailure();

  ExpectErrorMessage(IDS_DEMO_SETUP_COMPONENT_ERROR,
                     IDS_DEMO_SETUP_RECOVERY_CHECK_NETWORK);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, OfflineDemoModeUnavailable) {
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  test::OobeJS().ExpectDisabledPath(kNetworkNextButton);
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, ClickNetworkOnNetworkScreen) {
  TriggerDemoModeOnWelcomeScreen();
  test::WaitForNetworkSelectionScreen();

  test::OobeJS().ExpectDisabledPath(kNetworkNextButton);

  ClickNetworkListElement(kDefaultNetworkName);
  SimulateNetworkConnected();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();

  ProceedThroughDemoPreferencesScreen();

  test::WaitForConsolidatedConsentScreen();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       ClickConnectedNetworkOnNetworkScreen) {
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  test::WaitForNetworkSelectionScreen();

  test::OobeJS().ExpectEnabledPath(kNetworkNextButton);

  ClickNetworkListElement(kDefaultNetworkName);

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();

  ProceedThroughDemoPreferencesScreen();

  test::WaitForConsolidatedConsentScreen();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, BackOnNetworkScreen) {
  SimulateNetworkConnected();
  TriggerDemoModeOnWelcomeScreen();

  test::WaitForNetworkSelectionScreen();

  test::OobeJS().ClickOnPath(kNetworkBackButton);
  test::WaitForWelcomeScreen();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, BackOnTermsScreen) {
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();
  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  ProceedThroughDemoPreferencesScreen();
  test::WaitForConsolidatedConsentScreen();
  test::OobeJS().ClickOnPath(kCCBackButton);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTime.Consolidated-consent", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepShownStatus.Consolidated-consent", 1);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Consolidated-consent."
      "BackDemo",
      1);

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, BackOnErrorScreen) {
  // Simulate online setup failure.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  ProceedThroughDemoPreferencesScreen();

  AcceptTermsAndExpectDemoSetupFailure();

  test::OobeJS().ExpectEnabledPath(kDemoSetupErrorDialogBack);
  test::OobeJS().ClickOnPath(kDemoSetupErrorDialogBack);

  test::WaitForWelcomeScreen();
}

// TODO(crbug.com/1399073): Flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_RetryOnErrorScreen DISABLED_RetryOnErrorScreen
#else
#define MAYBE_RetryOnErrorScreen RetryOnErrorScreen
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, MAYBE_RetryOnErrorScreen) {
  // Simulate online setup failure.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  ProceedThroughDemoPreferencesScreen();

  AcceptTermsAndExpectDemoSetupFailure();
  test::LockDemoDeviceInstallAttributes();

  // We need to create another mock after showing error dialog.
  enrollment_helper_.ResetMock();
  // Simulate successful online setup on retry.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();

  test::OobeJS().ClickOnPath(kDemoSetupErrorDialogRetry);

  // LoginOrLockScreen is shown at beginning of OOBE, so we need to wait until
  // it's shown again when Demo setup completes.
  LoginOrLockScreenVisibleWaiter().WaitEvenIfShown();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       NoOfflineSetupOptionOnNetworkList) {
  test::WaitForWelcomeScreen();
  test::TapWelcomeNext();
  test::WaitForNetworkSelectionScreen();
}

class DemoSetupProgressStepsTest : public DemoSetupArcSupportedTest {
 public:
  DemoSetupProgressStepsTest() = default;

  DemoSetupProgressStepsTest(const DemoSetupProgressStepsTest&) = delete;
  DemoSetupProgressStepsTest& operator=(const DemoSetupProgressStepsTest&) =
      delete;

  ~DemoSetupProgressStepsTest() override = default;

  // Checks how many steps have been rendered in the demo setup screen.
  int CountNumberOfStepsInUi() {
    const std::string query =
        base::StrCat({test::GetOobeElementPath(kDemoSetupProgressDialog),
                      ".querySelectorAll('progress-list-item').length"});
    return test::OobeJS().GetInt(query);
  }

  // Checks how many steps are marked as given status in the demo setup screen.
  int CountStepsInUi(const std::string& status) {
    const std::string query = base::StrCat(
        {"Object.values(", test::GetOobeElementPath(kDemoSetupProgressDialog),
         ".querySelectorAll('progress-list-item')).filter(node => "
         "node.shadowRoot.querySelector('#icon-",
         status, ":not([hidden])')).length"});
    return test::OobeJS().GetInt(query);
  }
};

// TODO(b/271419599): Flaky.
IN_PROC_BROWSER_TEST_F(DemoSetupProgressStepsTest,
                       DISABLED_SetupProgessStepsDisplayCorrectly) {
  SimulateNetworkConnected();
  TriggerDemoModeOnWelcomeScreen();
  UseOnlineModeOnNetworkScreen();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);
  AcceptTermsAndExpectDemoSetupProgress();

  DemoSetupScreen* demoSetupScreen = GetDemoSetupScreen();

  DemoSetupController::DemoSetupStep orderedSteps[] = {
      DemoSetupController::DemoSetupStep::kDownloadResources,
      DemoSetupController::DemoSetupStep::kEnrollment,
      DemoSetupController::DemoSetupStep::kComplete};

  // Subtract 1 to account for kComplete step
  int numSteps =
      static_cast<int>(sizeof(orderedSteps) / sizeof(*orderedSteps)) - 1;
  ASSERT_EQ(CountNumberOfStepsInUi(), numSteps);

  for (int i = 0; i < numSteps; i++) {
    demoSetupScreen->SetCurrentSetupStepForTest(orderedSteps[i]);
    ASSERT_EQ(CountStepsInUi("pending"), numSteps - i - 1);
    ASSERT_EQ(CountStepsInUi("active"), 1);
    ASSERT_EQ(CountStepsInUi("completed"), i);
  }
}

class DemoSetupArcUnsupportedTest : public DemoSetupTestBase {
 public:
  DemoSetupArcUnsupportedTest() = default;

  DemoSetupArcUnsupportedTest(const DemoSetupArcUnsupportedTest&) = delete;
  DemoSetupArcUnsupportedTest& operator=(const DemoSetupArcUnsupportedTest&) =
      delete;

  ~DemoSetupArcUnsupportedTest() override = default;

  // DemoSetupTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DemoSetupTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kArcAvailability, "none");
    ASSERT_FALSE(arc::IsArcAvailable());
  }
};

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DoNotStartWithAccelerator DISABLED_DoNotStartWithAccelerator
#else
#define MAYBE_DoNotStartWithAccelerator DoNotStartWithAccelerator
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcUnsupportedTest,
                       MAYBE_DoNotStartWithAccelerator) {
  IsConfirmationDialogHidden();

  InvokeDemoModeWithAccelerator();

  IsConfirmationDialogHidden();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcUnsupportedTest, DoNotInvokeWithTaps) {
  IsConfirmationDialogHidden();

  InvokeDemoModeWithTaps();

  IsConfirmationDialogHidden();
}

/**
 * Test case of device variant region code, e.g. ca.fr etc.
 */
class DemoSetupVariantCountryCodeRegionTest : public DemoSetupArcSupportedTest {
 public:
  ~DemoSetupVariantCountryCodeRegionTest() override = default;

  DemoSetupVariantCountryCodeRegionTest() {
    statistics_provider_.SetMachineStatistic(system::kRegionKey, "ca.fr");
  }
};

// Flaky test: crbug.com/1340982, crbug.com/1147265
IN_PROC_BROWSER_TEST_F(DemoSetupVariantCountryCodeRegionTest,
                       DISABLED_VariantCountryCodeRegionDefaultCountryIsSet) {
  // Simulate successful online setup.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();
  UseOnlineModeOnNetworkScreen();

  // Expect active "OK" button when entering the preference screen.
  test::OobeJS().ExpectEnabledPath(kDemoPreferencesNext);
  test::OobeJS().ExpectElementValue("CA", kDemoPreferencesCountrySelect);
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  AcceptTermsAndExpectDemoSetupProgress();

  // Verify the email corresponds to France.
  EXPECT_EQ("admin-ca@cros-demo-mode.com",
            DemoSetupController::GetSubOrganizationEmail());

  // LoginOrLockScreen is shown at beginning of OOBE, so we need to wait until
  // it's shown again when Demo setup completes.
  LoginOrLockScreenVisibleWaiter().WaitEvenIfShown();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

/**
 * Test case of device virtual set region code, e.g. nordic etc.
 */
class DemoSetupVirtualSetRegionCodeTest : public DemoSetupArcSupportedTest {
 public:
  ~DemoSetupVirtualSetRegionCodeTest() override = default;

  DemoSetupVirtualSetRegionCodeTest() {
    statistics_provider_.SetMachineStatistic(system::kRegionKey, "nordic");
  }
};

// Flake on ASAN: crbug.com/1340618
// Flake on Linux Chrome OS: crbug.com/1351186
#if defined(ADDRESS_SANITIZER) || !defined(NDEBUG) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_VirtualSetCountryCodeRegionPlaceholderIsSet \
  DISABLED_VirtualSetCountryCodeRegionPlaceholderIsSet
#else
#define MAYBE_VirtualSetCountryCodeRegionPlaceholderIsSet \
  VirtualSetCountryCodeRegionPlaceholderIsSet
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupVirtualSetRegionCodeTest,
                       MAYBE_VirtualSetCountryCodeRegionPlaceholderIsSet) {
  // Simulate successful online setup.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  SimulateNetworkConnected();
  TriggerDemoModeOnWelcomeScreen();
  UseOnlineModeOnNetworkScreen();

  // Expect inactive "OK" button when entering the preference screen.
  test::OobeJS().ExpectDisabledPath(kDemoPreferencesNext);
  test::OobeJS().ExpectElementValue("N/A", kDemoPreferencesCountrySelect);

  PopulateDemoPreferencesAndFinishSetup();
}

/**
 * Test case of device with VPD region not set.
 */
class DemoSetupRegionCodeNotExistTest : public DemoSetupArcSupportedTest {
 public:
  ~DemoSetupRegionCodeNotExistTest() override = default;

  DemoSetupRegionCodeNotExistTest() {
    statistics_provider_.ClearMachineStatistic(system::kRegionKey);
  }
};

IN_PROC_BROWSER_TEST_F(DemoSetupRegionCodeNotExistTest,
                       RegionCodeNotExistPlaceholderIsSet) {
  // Simulate successful online setup.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();
  UseOnlineModeOnNetworkScreen();

  // Expect inactive "OK" button when entering the preference screen.
  test::OobeJS().ExpectDisabledPath(kDemoPreferencesNext);
  test::OobeJS().ExpectElementValue("N/A", kDemoPreferencesCountrySelect);

  PopulateDemoPreferencesAndFinishSetup();
}

/**
 * Test case of Blazey specific device.
 */
class DemoSetupBlazeyDeviceTest : public DemoSetupArcSupportedTest {
 public:
  ~DemoSetupBlazeyDeviceTest() override = default;

  DemoSetupBlazeyDeviceTest() {
    statistics_provider_.SetMachineStatistic(system::kRegionKey, "us");
    feature_list_.InitAndEnableFeature(chromeos::features::kCloudGamingDevice);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1342461): Flaky on release bots.
#if defined(ADDRESS_SANITIZER) || defined(NDEBUG)
#define MAYBE_DeviceIsBlazeyEnabledDevice DISABLED_DeviceIsBlazeyEnabledDevice
#else
#define MAYBE_DeviceIsBlazeyEnabledDevice DeviceIsBlazeyEnabledDevice
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupBlazeyDeviceTest,
                       MAYBE_DeviceIsBlazeyEnabledDevice) {
  // Simulate successful online setup.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();
  UseOnlineModeOnNetworkScreen();

  // Expect active "OK" button when entering the preference screen.
  test::OobeJS().ExpectElementValue("US", kDemoPreferencesCountrySelect);
  ProceedThroughDemoPreferencesScreen();

  AcceptTermsAndExpectDemoSetupProgress();

  // Verify the email corresponds to US.
  EXPECT_EQ("admin-us-blazey@cros-demo-mode.com",
            DemoSetupController::GetSubOrganizationEmail());

  // LoginOrLockScreen is shown at beginning of OOBE, so we need to wait until
  // it's shown again when Demo setup completes.
  LoginOrLockScreenVisibleWaiter().WaitEvenIfShown();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

}  // namespace
}  // namespace ash
