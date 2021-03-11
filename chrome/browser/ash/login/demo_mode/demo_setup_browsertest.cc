// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_util.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::test::DemoModeSetupResult;
using chromeos::test::SetupDummyOfflinePolicyDir;

namespace chromeos {

namespace {

const test::UIPath kDemoConfirmationDialog = {"connect",
                                              "demoModeConfirmationDialog"};

const test::UIPath kDemoPreferencesScreen = {"demo-preferences"};
const test::UIPath kDemoPreferencesCountry = {"demo-preferences",
                                              "countrySelect"};
const test::UIPath kDemoPreferencesCountrySelect = {"demo-preferences",
                                                    "countrySelect", "select"};
const test::UIPath kDemoPreferencesNext = {"demo-preferences", "nextButton"};

const test::UIPath kNetworkScreen = {"network-selection"};
const test::UIPath kNetworkNextButton = {"network-selection", "nextButton"};
const test::UIPath kNetworkBackButton = {"network-selection", "backButton"};

const test::UIPath kDemoSetupProgressDialog = {"demo-setup",
                                               "demoSetupProgressDialog"};
const test::UIPath kDemoSetupErrorDialog = {"demo-setup",
                                            "demoSetupErrorDialog"};
const test::UIPath kDemoSetupErrorDialogRetry = {"demo-setup", "retryButton"};
const test::UIPath kDemoSetupErrorDialogPowerwash = {"demo-setup",
                                                     "powerwashButton"};
const test::UIPath kDemoSetupErrorDialogBack = {"demo-setup", "back"};
const test::UIPath kDemoSetupErrorDialogMessage = {"demo-setup",
                                                   "errorMessage"};

const test::UIPath kArcTosDialog = {"arc-tos", "arcTosDialog"};
const test::UIPath kArcTosAcceptButton = {"arc-tos", "arcTosAcceptButton"};
const test::UIPath kArcTosDemoAppsNotice = {"arc-tos", "arcTosMetricsDemoApps"};
const test::UIPath kArcTosBackButton = {"arc-tos", "arcTosBackButton"};
const test::UIPath kArcTosNextButton = {"arc-tos", "arcTosNextButton"};

constexpr char kDefaultNetworkServicePath[] = "/service/eth1";
constexpr char kDefaultNetworkName[] = "eth1";

constexpr int kInvokeDemoModeGestureTapsCount = 10;

}  // namespace

// Basic tests for demo mode setup flow.
class DemoSetupTestBase : public OobeBaseTest {
 public:
  DemoSetupTestBase() = default;
  ~DemoSetupTestBase() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    update_engine_client()->set_update_check_result(
        UpdateEngineClient::UPDATE_RESULT_FAILED);
    branded_build_override_ =
        WizardController::ForceBrandedBuildForTesting(true);
    DisconnectAllNetworks();
  }

  void IsConfirmationDialogShown() {
    test::OobeJS().ExpectAttributeEQ("open", kDemoConfirmationDialog, true);
  }

  void IsConfirmationDialogHidden() {
    test::OobeJS().ExpectAttributeEQ("open", kDemoConfirmationDialog, false);
  }

  void ClickOkOnConfirmationDialog() {
    test::OobeJS().TapOnPath({"connect", "okButton"});
  }

  void ClickCancelOnConfirmationDialog() {
    test::OobeJS().TapOnPath({"connect", "cancelButton"});
  }

  void TriggerDemoModeOnWelcomeScreen() {
    test::WaitForWelcomeScreen();
    IsConfirmationDialogHidden();

    InvokeDemoModeWithAccelerator();
    IsConfirmationDialogShown();

    ClickOkOnConfirmationDialog();

    OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
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
        ash::LoginAcceleratorAction::kStartDemoMode);
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

  // Returns whether a custom offline item is shown as a first element on the
  // network list.
  static bool IsOfflineNetworkListElementShown() {
    const char kOfflineNetworkElement[] = "offlineDemoSetupListItemName";

    const std::string element_selector = base::StrCat(
        {test::GetOobeElementPath(kNetworkScreen),
         ".getNetworkListItemWithQueryForTest('network-list-item')"});
    const std::string query =
        base::StrCat({"!!", element_selector, " && ", element_selector,
                      ".item.customItemName == '", kOfflineNetworkElement,
                      "' && !", element_selector, ".hidden"});
    return test::OobeJS().GetBool(query);
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

  void UseOfflineModeOnNetworkScreen() {
    test::WaitForNetworkSelectionScreen();
    test::OobeJS().ExpectDisabledPath(kNetworkNextButton);

    const std::string offline_setup_item_name =
        l10n_util::GetStringUTF8(IDS_NETWORK_OFFLINE_DEMO_SETUP_LIST_ITEM_NAME);
    ClickNetworkListElement(offline_setup_item_name);
  }

  void UseOnlineModeOnNetworkScreen() {
    test::WaitForNetworkSelectionScreen();
    // Wait until default network is connected.
    test::OobeJS().CreateEnabledWaiter(true, kNetworkNextButton)->Wait();
    test::OobeJS().ClickOnPath(kNetworkNextButton);
  }

  void SimulateOfflineEnvironment() {
    DemoSetupController* controller =
        WizardController::default_controller()->demo_setup_controller();

    // Simulate offline data directory.
    ASSERT_TRUE(SetupDummyOfflinePolicyDir("test", &fake_demo_resources_dir_));
    controller->SetPreinstalledOfflineResourcesPathForTesting(
        fake_demo_resources_dir_.GetPath());

    // Simulate policy store.
    EXPECT_CALL(mock_policy_store_, Store(testing::_))
        .WillRepeatedly(testing::InvokeWithoutArgs(
            &mock_policy_store_,
            &policy::MockCloudPolicyStore::NotifyStoreLoaded));
    controller->SetDeviceLocalAccountPolicyStoreForTest(&mock_policy_store_);
  }

  // Simulates device being connected to the network.
  void SimulateNetworkConnected() {
    ShillServiceClient::TestInterface* service =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service->SetServiceProperty(kDefaultNetworkServicePath,
                                shill::kStateProperty,
                                base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

  // Simulates device being disconnected from the network.
  void SimulateNetworkDisconnected() {
    ShillServiceClient::TestInterface* service =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
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
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    for (const auto* const network : networks) {
      service->SetServiceProperty(network->path(), shill::kStateProperty,
                                  base::Value(shill::kStateIdle));
    }
    base::RunLoop().RunUntilIdle();
  }

  // Sets fake time in MultiTapDetector to remove dependency on real time in
  // test environment.
  void SetFakeTimeForMultiTapDetector(base::Time fake_time) {
    const std::string query =
        base::StrCat({"MultiTapDetector.FAKE_TIME_FOR_TESTS = new Date('",
                      base::TimeToISO8601(fake_time), "');"});
    test::ExecuteOobeJS(query);
  }

  DemoSetupScreen* GetDemoSetupScreen() {
    return static_cast<DemoSetupScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            DemoSetupScreenView::kScreenId));
  }

 protected:
  test::EnrollmentHelperMixin enrollment_helper_{&mixin_host_};

 private:
  // TODO(agawronska): Maybe create a separate test fixture for offline setup.
  base::ScopedTempDir fake_demo_resources_dir_;
  policy::MockCloudPolicyStore mock_policy_store_;
  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupTestBase);
};

class DemoSetupArcSupportedTest : public DemoSetupTestBase {
 public:
  DemoSetupArcSupportedTest() = default;
  ~DemoSetupArcSupportedTest() override = default;

  // DemoSetupTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DemoSetupTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    ASSERT_TRUE(arc::IsArcAvailable());
  }

  void SetPlayStoreTermsForTesting() {
    test::ExecuteOobeJS(
        R"(login.ArcTermsOfServiceScreen.setTosForTesting(
              'Test Play Store Terms of Service');)");
  }

  void WaitForArcTosScreen() {
    OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
    SetPlayStoreTermsForTesting();
    test::OobeJS().CreateVisibilityWaiter(true, kArcTosDialog)->Wait();
  }

  void AcceptArcTos() {
    test::OobeJS().CreateVisibilityWaiter(true, kArcTosNextButton)->Wait();
    test::OobeJS().ClickOnPath(kArcTosNextButton);
    test::OobeJS().CreateVisibilityWaiter(true, kArcTosAcceptButton)->Wait();
    test::OobeJS().ClickOnPath(kArcTosAcceptButton);
  }

  void AcceptTermsAndExpectDemoSetupProgress() {
    test::WaitForEulaScreen();
    test::TapEulaAccept();

    WaitForArcTosScreen();

    test::OobeJS().ExpectVisiblePath(kArcTosDemoAppsNotice);

    // As setup screen shows only progress indicator and disappears afterwards
    // we need to set up waiter before the action that triggers the screen.
    OobeScreenWaiter setup_progress_waiter(DemoSetupScreenView::kScreenId);
    AcceptArcTos();
    setup_progress_waiter.Wait();
  }

  void AcceptTermsAndExpectDemoSetupFailure() {
    test::WaitForEulaScreen();
    test::TapEulaAccept();

    WaitForArcTosScreen();

    test::OobeJS().ExpectVisiblePath(kArcTosDemoAppsNotice);

    AcceptArcTos();
    // As we expect the error message to stay on the screen, it is safe to
    // wait for it in the usual manner.
    OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
    test::OobeJS().CreateVisibilityWaiter(true, kDemoSetupErrorDialog)->Wait();
  }
};

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       ShowConfirmationDialogAndProceed) {
  IsConfirmationDialogHidden();

  InvokeDemoModeWithAccelerator();
  IsConfirmationDialogShown();

  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
}

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ShowConfirmationDialogAndCancel \
  DISABLED_ShowConfirmationDialogAndCancel
#else
#define MAYBE_ShowConfirmationDialogAndCancel ShowConfirmationDialogAndCancel
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_ShowConfirmationDialogAndCancel) {
  IsConfirmationDialogHidden();

  InvokeDemoModeWithAccelerator();
  IsConfirmationDialogShown();

  ClickCancelOnConfirmationDialog();
  IsConfirmationDialogHidden();

  test::OobeJS().ExpectHiddenPath(kDemoPreferencesScreen);
}

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_InvokeWithTaps DISABLED_InvokeWithTaps
#else
#define MAYBE_InvokeWithTaps InvokeWithTaps
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, MAYBE_InvokeWithTaps) {
  // Use fake time to avoid flakiness.
  SetFakeTimeForMultiTapDetector(base::Time::UnixEpoch());
  IsConfirmationDialogHidden();

  MultiTapOobeContainer(10);
  IsConfirmationDialogShown();
}

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DoNotInvokeWithNonConsecutiveTaps \
  DISABLED_DoNotInvokeWithNonConsecutiveTaps
#else
#define MAYBE_DoNotInvokeWithNonConsecutiveTaps \
  DoNotInvokeWithNonConsecutiveTaps
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_DoNotInvokeWithNonConsecutiveTaps) {
  // Use fake time to avoid flakiness.
  const base::Time kFakeTime = base::Time::UnixEpoch();
  SetFakeTimeForMultiTapDetector(kFakeTime);
  IsConfirmationDialogHidden();

  MultiTapOobeContainer(5);
  IsConfirmationDialogHidden();

  // Advance time to make interval in between taps longer than expected by
  // multi-tap gesture detector.
  SetFakeTimeForMultiTapDetector(kFakeTime +
                                 base::TimeDelta::FromMilliseconds(500));

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

  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOnlineModeOnNetworkScreen();

  AcceptTermsAndExpectDemoSetupProgress();

  EXPECT_TRUE(DemoSetupController::GetSubOrganizationEmail().empty());

  OobeScreenWaiter(GetFirstSigninScreen()).Wait();

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

  // Verify the country names are displayed correctly. Regression test for
  // potential country code changes.
  const base::flat_map<std::string, std::string> kCountryCodeToNameMap(
      {{"us", "United States"},
       {"be", "Belgium"},
       {"ca", "Canada"},
       {"dk", "Denmark"},
       {"fi", "Finland"},
       {"fr", "France"},
       {"de", "Germany"},
       {"ie", "Ireland"},
       {"it", "Italy"},
       {"jp", "Japan"},
       {"lu", "Luxembourg"},
       {"nl", "Netherlands"},
       {"no", "Norway"},
       {"es", "Spain"},
       {"se", "Sweden"},
       {"gb", "United Kingdom"}});
  for (const std::string country_code : DemoSession::kSupportedCountries) {
    const auto it = kCountryCodeToNameMap.find(country_code);
    ASSERT_NE(kCountryCodeToNameMap.end(), it);
    const std::string query =
        base::StrCat({test::GetOobeElementPath(kDemoPreferencesCountry),
                      ".$$('option[value=\"", country_code, "\"]').innerHTML"});
    EXPECT_EQ(it->second, test::OobeJS().GetString(query));
  }

  // Select France as the Demo Mode country.
  test::OobeJS().SelectElementInPath("fr", kDemoPreferencesCountrySelect);
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOnlineModeOnNetworkScreen();

  AcceptTermsAndExpectDemoSetupProgress();

  // Verify the email corresponds to France.
  EXPECT_EQ("admin-fr@cros-demo-mode.com",
            DemoSetupController::GetSubOrganizationEmail());

  OobeScreenWaiter(GetFirstSigninScreen()).Wait();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
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

  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOnlineModeOnNetworkScreen();

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

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OnlineSetupFlowErrorPowerwashRequired \
  DISABLED_OnlineSetupFlowErrorPowerwashRequired
#else
#define MAYBE_OnlineSetupFlowErrorPowerwashRequired \
  OnlineSetupFlowErrorPowerwashRequired
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_OnlineSetupFlowErrorPowerwashRequired) {
  // Simulate online setup failure that requires powerwash.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForLockError(
          chromeos::InstallAttributes::LOCK_ALREADY_LOCKED));
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOnlineModeOnNetworkScreen();

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

  // Set the component to fail to install when requested.
  WizardController::default_controller()
      ->demo_setup_controller()
      ->SetCrOSComponentLoadErrorForTest(
          component_updater::CrOSComponentManager::Error::INSTALL_FAILURE);

  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOnlineModeOnNetworkScreen();

  AcceptTermsAndExpectDemoSetupFailure();

  ExpectErrorMessage(IDS_DEMO_SETUP_COMPONENT_ERROR,
                     IDS_DEMO_SETUP_RECOVERY_CHECK_NETWORK);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, OfflineDemoModeUnavailable) {
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  test::WaitForNetworkSelectionScreen();

  test::OobeJS().ExpectDisabledPath(kNetworkNextButton);

  // Offline Demo Mode is not available when there are no preinstalled demo
  // resources.
  EXPECT_FALSE(IsOfflineNetworkListElementShown());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, OfflineSetupFlowSuccess) {
  // Simulate offline setup success.
  enrollment_helper_.ExpectOfflineEnrollmentSuccess();
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOfflineModeOnNetworkScreen();

  test::WaitForEulaScreen();
  test::TapEulaAccept();

  WaitForArcTosScreen();

  test::OobeJS().ExpectVisiblePath(kArcTosDemoAppsNotice);

  OobeScreenWaiter setup_progress_waiter(DemoSetupScreenView::kScreenId);
  AcceptArcTos();
  setup_progress_waiter.Wait();

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OfflineSetupFlowErrorDefault DISABLED_OfflineSetupFlowErrorDefault
#else
#define MAYBE_OfflineSetupFlowErrorDefault OfflineSetupFlowErrorDefault
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_OfflineSetupFlowErrorDefault) {
  // Simulate offline setup failure.
  enrollment_helper_.ExpectOfflineEnrollmentError(
      policy::EnrollmentStatus::ForStatus(
          policy::EnrollmentStatus::OFFLINE_POLICY_DECODING_FAILED));
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOfflineModeOnNetworkScreen();

  test::WaitForEulaScreen();
  test::TapEulaAccept();

  WaitForArcTosScreen();

  test::OobeJS().ExpectVisiblePath(kArcTosDemoAppsNotice);

  AcceptArcTos();
  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kDemoSetupErrorDialog)->Wait();

  ExpectErrorMessage(IDS_DEMO_SETUP_OFFLINE_POLICY_ERROR,
                     IDS_DEMO_SETUP_RECOVERY_OFFLINE_FATAL);

  test::OobeJS().ExpectVisiblePath(kDemoSetupErrorDialogRetry);
  test::OobeJS().ExpectHiddenPath(kDemoSetupErrorDialogPowerwash);
  test::OobeJS().ExpectEnabledPath(kDemoSetupErrorDialogBack);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       OfflineSetupFlowErrorPowerwashRequired) {
  // Simulate offline setup failure.
  enrollment_helper_.ExpectOfflineEnrollmentError(
      policy::EnrollmentStatus::ForLockError(
          chromeos::InstallAttributes::LOCK_READBACK_ERROR));
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOfflineModeOnNetworkScreen();

  test::WaitForEulaScreen();
  test::TapEulaAccept();

  WaitForArcTosScreen();
  AcceptArcTos();

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kDemoSetupErrorDialog)->Wait();
  ExpectErrorMessage(IDS_DEMO_SETUP_LOCK_ERROR,
                     IDS_DEMO_SETUP_RECOVERY_POWERWASH);

  test::OobeJS().ExpectHiddenPath(kDemoSetupErrorDialogRetry);
  test::OobeJS().ExpectVisiblePath(kDemoSetupErrorDialogPowerwash);
  test::OobeJS().ExpectDisabledPath(kDemoSetupErrorDialogBack);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_NextDisabledOnNetworkScreen DISABLED_NextDisabledOnNetworkScreen
#else
#define MAYBE_NextDisabledOnNetworkScreen NextDisabledOnNetworkScreen
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_NextDisabledOnNetworkScreen) {
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  test::WaitForNetworkSelectionScreen();

  test::OobeJS().ExpectDisabledPath(kNetworkNextButton);

  test::OobeJS()
      .CreateEnabledWaiter(false /* disabled */, kNetworkNextButton)
      ->Wait();

  test::OobeJS().TapOnPath(kNetworkNextButton);

  // Screen should not change.
  test::WaitForNetworkSelectionScreen();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, ClickNetworkOnNetworkScreen) {
  TriggerDemoModeOnWelcomeScreen();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);
  test::WaitForNetworkSelectionScreen();

  test::OobeJS().ExpectDisabledPath(kNetworkNextButton);

  ClickNetworkListElement(kDefaultNetworkName);
  SimulateNetworkConnected();

  test::WaitForEulaScreen();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       ClickConnectedNetworkOnNetworkScreen) {
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  test::WaitForNetworkSelectionScreen();

  test::OobeJS().ExpectEnabledPath(kNetworkNextButton);

  ClickNetworkListElement(kDefaultNetworkName);

  test::WaitForEulaScreen();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, BackOnNetworkScreen) {
  SimulateNetworkConnected();
  TriggerDemoModeOnWelcomeScreen();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  test::WaitForNetworkSelectionScreen();

  test::OobeJS().ClickOnPath(kNetworkBackButton);
  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
}

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_BackOnArcTermsScreen DISABLED_BackOnArcTermsScreen
#else
#define MAYBE_BackOnArcTermsScreen BackOnArcTermsScreen
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, MAYBE_BackOnArcTermsScreen) {
  // User cannot go to ARC ToS screen without accepting eula - simulate that.
  StartupUtils::MarkEulaAccepted();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOnlineModeOnNetworkScreen();

  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(kArcTosBackButton);

  test::WaitForNetworkSelectionScreen();
}

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_BackOnErrorScreen DISABLED_BackOnErrorScreen
#else
#define MAYBE_BackOnErrorScreen BackOnErrorScreen
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, MAYBE_BackOnErrorScreen) {
  // Simulate online setup failure.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOnlineModeOnNetworkScreen();

  AcceptTermsAndExpectDemoSetupFailure();

  test::OobeJS().ExpectEnabledPath(kDemoSetupErrorDialogBack);
  test::OobeJS().ClickOnPath(kDemoSetupErrorDialogBack);

  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

// TODO(crbug.com/1150349): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
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

  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOnlineModeOnNetworkScreen();

  AcceptTermsAndExpectDemoSetupFailure();

  // We need to create another mock after showing error dialog.
  enrollment_helper_.ResetMock();
  // Simulate successful online setup on retry.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();

  test::OobeJS().ClickOnPath(kDemoSetupErrorDialogRetry);

  OobeScreenWaiter(GetFirstSigninScreen()).Wait();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       ShowOfflineSetupOptionOnNetworkList) {
  TriggerDemoModeOnWelcomeScreen();

  SimulateOfflineEnvironment();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  test::WaitForNetworkSelectionScreen();

  test::TestPredicateWaiter waiter(
      base::BindRepeating([]() { return IsOfflineNetworkListElementShown(); }));
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       NoOfflineSetupOptionOnNetworkList) {
  test::WaitForWelcomeScreen();
  test::TapWelcomeNext();
  test::WaitForNetworkSelectionScreen();
  EXPECT_FALSE(IsOfflineNetworkListElementShown());
}

class DemoSetupProgressStepsTest : public DemoSetupArcSupportedTest {
 public:
  DemoSetupProgressStepsTest() = default;
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(DemoSetupProgressStepsTest);
};

IN_PROC_BROWSER_TEST_F(DemoSetupProgressStepsTest,
                       SetupProgessStepsDisplayCorrectly) {
  SimulateNetworkConnected();
  TriggerDemoModeOnWelcomeScreen();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);
  UseOnlineModeOnNetworkScreen();
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
  ~DemoSetupArcUnsupportedTest() override = default;

  // DemoSetupTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DemoSetupTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kArcAvailability, "none");
    ASSERT_FALSE(arc::IsArcAvailable());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoSetupArcUnsupportedTest);
};

IN_PROC_BROWSER_TEST_F(DemoSetupArcUnsupportedTest, DoNotStartWithAccelerator) {
  IsConfirmationDialogHidden();

  InvokeDemoModeWithAccelerator();

  IsConfirmationDialogHidden();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcUnsupportedTest, DoNotInvokeWithTaps) {
  IsConfirmationDialogHidden();

  InvokeDemoModeWithTaps();

  IsConfirmationDialogHidden();
}

// Demo setup tests related to Force Re-Enrollment.
class DemoSetupFRETest : public DemoSetupArcSupportedTest {
 protected:
  DemoSetupFRETest() {
    statistics_provider_.SetMachineStatistic(system::kSerialNumberKeyForTest,
                                             "testserialnumber");
  }
  ~DemoSetupFRETest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DemoSetupArcSupportedTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        chromeos::AutoEnrollmentController::kForcedReEnrollmentAlways);
  }

  system::ScopedFakeStatisticsProvider statistics_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoSetupFRETest);
};

IN_PROC_BROWSER_TEST_F(DemoSetupFRETest, DeviceFromFactory) {
  // Simulating brand new device - "active_date", "check_enrollment",
  // "block_devmode" flags do not exist in VPD.

  // Simulate offline setup success.
  enrollment_helper_.ExpectOfflineEnrollmentSuccess();
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  SimulateOfflineEnvironment();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOfflineModeOnNetworkScreen();

  // We accept success during demo setup, but "network not connected" error
  // afterwards.
  AcceptTermsAndExpectDemoSetupProgress();
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupFRETest, NonEnterpriseDevice) {
  // Simulating device that was never set for enterprise:
  // * "active_date" is set
  // * "check_enrollment" and "block_devmode" flags are set to false.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2018-01");
  statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey, "0");
  statistics_provider_.SetMachineStatistic(system::kBlockDevModeKey, "0");

  // Simulate offline setup success.
  enrollment_helper_.ExpectOfflineEnrollmentSuccess();
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  SimulateOfflineEnvironment();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOfflineModeOnNetworkScreen();

  // We accept success during demo setup, but "network not connected" error
  // afterwards.
  AcceptTermsAndExpectDemoSetupProgress();
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupFRETest, LegacyDemoModeDevice) {
  // Simulating device enrolled into legacy demo mode:
  // * "active_date" and "check_enrollment" are set
  // * "block_devmode" is set to false, because legacy demo mode does not have
  // FRE.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2018-01");
  statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey, "1");
  statistics_provider_.SetMachineStatistic(system::kBlockDevModeKey, "0");

  // Simulate offline setup success.
  enrollment_helper_.ExpectOfflineEnrollmentSuccess();
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  SimulateOfflineEnvironment();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOfflineModeOnNetworkScreen();

  // We accept success during demo setup, but "network not connected" error
  // afterwards.
  AcceptTermsAndExpectDemoSetupProgress();
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupFRETest, DeviceWithFRE) {
  // Simulating device that requires FRE. "check_enrollment", "block_devmode"
  // and "ActivateDate" flags are set.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2018-01");
  statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey, "1");
  statistics_provider_.SetMachineStatistic(system::kBlockDevModeKey, "1");

  // Expect no enrollment to take place due to error.
  enrollment_helper_.ExpectNoEnrollment();
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  SimulateOfflineEnvironment();
  test::OobeJS().ClickOnPath(kDemoPreferencesNext);

  UseOfflineModeOnNetworkScreen();

  AcceptTermsAndExpectDemoSetupFailure();

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

}  // namespace chromeos
