// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <string>
#include <string_view>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
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
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
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

inline constexpr char kGrowthCampaignsComponentName[] = "growth-campaigns";
inline constexpr char kDemoAppComponentName[] = "demo-mode-app";
inline constexpr char kDemoResourcesComponentName[] = "demo-mode-resources";
inline constexpr char kCampaignsFileName[] = "campaigns.json";

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

    WizardController::default_controller()
        ->demo_setup_controller()
        ->EnableLoadRealComponentsForTest();
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
                      base::TimeFormatAsIso8601(fake_time), "'));"});
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

// Extra parts for setting up the FakeComponentManagerAsh before the real one
// has been initialized on the browser
class DemoSetupTestMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  explicit DemoSetupTestMainExtraParts(
      bool growth_campaigns_enabled = false,
      component_updater::ComponentManagerAsh::Error
          demo_mode_app_load_response =
              component_updater::ComponentManagerAsh::Error::NONE)
      : growth_campaigns_enabled_(growth_campaigns_enabled),
        demo_mode_app_load_response_(demo_mode_app_load_response) {
    CHECK(components_temp_dir_.CreateUniqueTempDir());
  }
  DemoSetupTestMainExtraParts(const DemoSetupTestMainExtraParts&) = delete;
  DemoSetupTestMainExtraParts& operator=(const DemoSetupTestMainExtraParts&) =
      delete;

  base::FilePath GetGrowthCampaignsPath() {
    return components_temp_dir_.GetPath()
        .AppendASCII("cros-components")
        .AppendASCII(kGrowthCampaignsComponentName);
  }

  void PostEarlyInitialization() override {
    auto component_manager_ash =
        base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
    std::set<std::string> supported_components = {kDemoResourcesComponentName,
                                                  kDemoAppComponentName};
    if (growth_campaigns_enabled_) {
      supported_components.insert(kGrowthCampaignsComponentName);
    }

    component_manager_ash->set_supported_components(supported_components);
    if (demo_mode_app_load_response_ ==
        component_updater::ComponentManagerAsh::Error::NONE) {
      component_manager_ash->ResetComponentState(
          kDemoAppComponentName,
          component_updater::FakeComponentManagerAsh::ComponentInfo(
              demo_mode_app_load_response_, base::FilePath("/dev/null"),
              base::FilePath("/run/imageloader/demo-mode-app")));
    } else {
      component_manager_ash->ResetComponentState(
          kDemoAppComponentName,
          component_updater::FakeComponentManagerAsh::ComponentInfo(
              demo_mode_app_load_response_, base::FilePath(),
              base::FilePath()));
    }
    component_manager_ash->ResetComponentState(
        kDemoResourcesComponentName,
        component_updater::FakeComponentManagerAsh::ComponentInfo(
            component_updater::ComponentManagerAsh::Error::NONE,
            base::FilePath("/dev/null"),
            base::FilePath("/run/imageloader/demo-mode-resources")));

    if (growth_campaigns_enabled_) {
      component_manager_ash->ResetComponentState(
          kGrowthCampaignsComponentName,
          component_updater::FakeComponentManagerAsh::ComponentInfo(
              component_updater::ComponentManagerAsh::Error::NONE,
              base::FilePath("/dev/null"), GetGrowthCampaignsPath()));
    }

    platform_part_test_api_ =
        std::make_unique<BrowserProcessPlatformPartTestApi>(
            g_browser_process->platform_part());
    platform_part_test_api_->InitializeComponentManager(
        std::move(component_manager_ash));
  }

  void PostMainMessageLoopRun() override {
    platform_part_test_api_->ShutdownComponentManager();
    platform_part_test_api_.reset();
  }

 private:
  std::unique_ptr<BrowserProcessPlatformPartTestApi> platform_part_test_api_;
  base::ScopedTempDir components_temp_dir_;
  bool growth_campaigns_enabled_;
  component_updater::ComponentManagerAsh::Error demo_mode_app_load_response_;
};

class DemoSetupArcSupportedTest : public DemoSetupTestBase {
 public:
  DemoSetupArcSupportedTest() {
    statistics_provider_.SetMachineStatistic(system::kRegionKey, "us");
    statistics_provider_.SetVpdStatus(
        system::StatisticsProvider::VpdStatus::kValid);
  }
  ~DemoSetupArcSupportedTest() override = default;

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    auto extra_parts = std::make_unique<DemoSetupTestMainExtraParts>();
    static_cast<ChromeBrowserMainParts*>(browser_main_parts)
        ->AddParts(std::move(extra_parts));
    DemoSetupTestBase::CreatedBrowserMainParts(browser_main_parts);
  }

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

  void AcceptTermsAndExpectDemoSetupFailure(
      DemoSetupController::DemoSetupError::ErrorCode setup_error_code) {
    WaitForConsolidatedConsentScreen();
    test::TapConsolidatedConsentAccept();

    // After accepting the metrics reporting consent, there should be no
    // DemoMode.Setup.Error metrics reported yet before the setup process.
    histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 0);

    // As we expect the error message to stay on the screen, it is safe to
    // wait for it in the usual manner.
    OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
    test::OobeJS().CreateVisibilityWaiter(true, kDemoSetupErrorDialog)->Wait();

    // The corresponding error `setup_error_code` should be reported after the
    // setup fails.
    histogram_tester_.ExpectBucketCount("DemoMode.Setup.Error",
                                        setup_error_code, 1);
    histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
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

// TODO(crbug.com/40157834): Flaky on ChromeOS ASAN.
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

  // Both components were successfully loaded on the initial attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);

  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
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

  // Both components were successfully loaded on the initial attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);

  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
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

  // Both components were successfully loaded on the initial attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);

  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
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
  // Have the store number numerical but have 257 characters
  SetAndVerifyInvalidRetailerNameAndStoreNumber(
      "ValidRetailer",
      "257257257257257257257257257257257257257257257257257257257257257257257257"
      "257257257257257257257257257257257257257257257257257257257257257257257257"
      "257257257257257257257257257257257257257257257257257257257257257257257257"
      "25725725725725725725725725725725725725725");
  // Have the retailer Name have 257 characters
  SetAndVerifyInvalidRetailerNameAndStoreNumber(
      "257characters257characters257characters257characters257characters257char"
      "acters257characters257characters257characters257characters257characters2"
      "57characters257characters257characters257characters257characters257chara"
      "cters257characters257characters257charact",
      "1234");

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

  // policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE matching to
  // DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable in
  // DemoSetupController::CreateFromClientStatus().
  AcceptTermsAndExpectDemoSetupFailure(
      DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable);

  // Default error returned by MockDemoModeOnlineEnrollmentHelperCreator.
  ExpectErrorMessage(IDS_DEMO_SETUP_TEMPORARY_ERROR,
                     IDS_DEMO_SETUP_RECOVERY_RETRY);

  test::OobeJS().ExpectVisiblePath(kDemoSetupErrorDialogRetry);
  test::OobeJS().ExpectHiddenPath(kDemoSetupErrorDialogPowerwash);
  test::OobeJS().ExpectEnabledPath(kDemoSetupErrorDialogBack);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());

  // The error occurred at the enrollment step. In the previous component
  // loading step, both components were still successfully loaded on the initial
  // attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);
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

  // policy::DeviceManagementStatus::LOCK_ALREADY_LOCKED matching to
  // DemoSetupController::DemoSetupError::ErrorCode::kAlreadyLocked in
  // DemoSetupController::CreateFromClientStatus().
  AcceptTermsAndExpectDemoSetupFailure(
      DemoSetupController::DemoSetupError::ErrorCode::kAlreadyLocked);

  ExpectErrorMessage(IDS_DEMO_SETUP_ALREADY_LOCKED_ERROR,
                     IDS_DEMO_SETUP_RECOVERY_POWERWASH);

  test::OobeJS().ExpectHiddenPath(kDemoSetupErrorDialogRetry);
  test::OobeJS().ExpectVisiblePath(kDemoSetupErrorDialogPowerwash);
  test::OobeJS().ExpectDisabledPath(kDemoSetupErrorDialogBack);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());

  // The error occurred at the enrollment step. In the previous component
  // loading step, both components were still successfully loaded on the initial
  // attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, OfflineDemoModeUnavailable) {
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  test::OobeJS().ExpectDisabledPath(kNetworkNextButton);
}

// Flaky. https://crbug.com/1453362.
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       DISABLED_ClickNetworkOnNetworkScreen) {
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

  // policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE matching to
  // DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable in
  // DemoSetupController::CreateFromClientStatus().
  AcceptTermsAndExpectDemoSetupFailure(
      DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable);

  test::OobeJS().ExpectEnabledPath(kDemoSetupErrorDialogBack);
  test::OobeJS().ClickOnPath(kDemoSetupErrorDialogBack);

  test::WaitForWelcomeScreen();
}

// TODO(crbug.com/40249751): Flaky on ChromeOS.
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

  // policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE matching to
  // DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable in
  // DemoSetupController::CreateFromClientStatus().
  AcceptTermsAndExpectDemoSetupFailure(
      DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable);
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
  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success. There should have been two counts because of two tries.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 2);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
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

// TODO(crbug.com/40157834): Flaky on ChromeOS ASAN.
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
 * Test case of Demo Mode setup with Growth Framework enabled.
 */
class DemoSetupComponentLoadErrorTest : public DemoSetupArcSupportedTest {
 public:
  DemoSetupComponentLoadErrorTest() = default;
  ~DemoSetupComponentLoadErrorTest() override = default;

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    auto extra_parts = std::make_unique<DemoSetupTestMainExtraParts>(
        /*growth_campaigns_enabled=*/false,
        component_updater::ComponentManagerAsh::Error::INSTALL_FAILURE);
    static_cast<ChromeBrowserMainParts*>(browser_main_parts)
        ->AddParts(std::move(extra_parts));
    DemoSetupTestBase::CreatedBrowserMainParts(browser_main_parts);
  }
};

IN_PROC_BROWSER_TEST_F(DemoSetupComponentLoadErrorTest,
                       OnlineSetupFlowCrosComponentFailure) {
  // Simulate failure to load demo resources CrOS component.
  // There is no enrollment attempt, as process fails earlier.
  enrollment_helper_.ExpectNoEnrollment();
  SimulateNetworkConnected();

  TriggerDemoModeOnWelcomeScreen();

  UseOnlineModeOnNetworkScreen();

  ProceedThroughDemoPreferencesScreen();

  // We should expect
  // DemoSetupController::DemoSetupError::ErrorCode::kOnlineComponentError for
  // cros component failure.
  AcceptTermsAndExpectDemoSetupFailure(
      DemoSetupController::DemoSetupError::ErrorCode::kOnlineComponentError);

  ExpectErrorMessage(IDS_DEMO_SETUP_COMPONENT_ERROR,
                     IDS_DEMO_SETUP_RECOVERY_CHECK_NETWORK);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());

  // DemoSetupComponentLoadErrorTest gives INSTALL_FAILURE to the demo mode app
  // component. So there should be app failure and resources success. There is
  // no second attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppFailureResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);
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
  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
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
  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
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

// TODO(crbug.com/1486991): Flaky under dbg and asan.
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER)
#define MAYBE_RegionCodeNotExistPlaceholderIsSet \
  DISABLED_RegionCodeNotExistPlaceholderIsSet
#else
#define MAYBE_RegionCodeNotExistPlaceholderIsSet \
  RegionCodeNotExistPlaceholderIsSet
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupRegionCodeNotExistTest,
                       MAYBE_RegionCodeNotExistPlaceholderIsSet) {
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
  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
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
  // `LoginOrLockScreenVisibleWaiter::WaitImpl` has a time out equals to
  // `TestTimeouts::test_launcher_timeout()` (which is equals to 135s in this
  // test), but sometimes it might be longer than 2 minutes, which left 15s for
  // this test to run. Increase the timeout of this test so that it has enough
  // time.
  base::test::ScopedRunLoopTimeout increase_timeout(
      FROM_HERE, TestTimeouts::test_launcher_timeout() + base::Seconds(60));

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
  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
}

/**
 * Test case of Quick Start enabled device, where quick start button should not
 * show for demo mode.
 */
class DemoSetupQuickStartEnabledTest : public DemoSetupArcSupportedTest {
 public:
  ~DemoSetupQuickStartEnabledTest() override = default;

  DemoSetupQuickStartEnabledTest() {
    feature_list_.InitAndEnableFeature(features::kOobeQuickStart);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DemoSetupQuickStartEnabledTest, QuickStartButton) {
  SimulateNetworkDisconnected();

  TriggerDemoModeOnWelcomeScreen();

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

  // Check that QuickStart button is missing from network_selector
  auto kQuickStartEntryPointName = l10n_util::GetStringUTF8(
      IDS_LOGIN_QUICK_START_SETUP_NETWORK_SCREEN_ENTRY_POINT);

  std::string networkElementSelector =
      test::GetOobeElementPath(
          {kNetworkId, "networkSelectLogin", "networkSelect"}) +
      ".getNetworkListItemByNameForTest('" + kQuickStartEntryPointName + "')";

  test::OobeJS().ExpectTrue(networkElementSelector + " == null");
}

/**
 * Test case of Demo Mode setup with Growth Framework enabled.
 */
class DemoSetupGrowthFrameworkEnabledTest : public DemoSetupArcSupportedTest {
 public:
  ~DemoSetupGrowthFrameworkEnabledTest() override = default;

  DemoSetupGrowthFrameworkEnabledTest() {
    feature_list_.InitWithFeatures(
        {features::kGrowthCampaignsInDemoMode, ash::features::kGrowthFramework},
        {});
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    auto extra_parts = std::make_unique<DemoSetupTestMainExtraParts>(
        /*growth_campaigns_enabled=*/true);
    growth_campaigns_mounted_path_ = extra_parts->GetGrowthCampaignsPath();
    static_cast<ChromeBrowserMainParts*>(browser_main_parts)
        ->AddParts(std::move(extra_parts));
    DemoSetupTestBase::CreatedBrowserMainParts(browser_main_parts);
  }

  void CreateTestCampaignsFile(std::string_view data) {
    CHECK(base::CreateDirectory(growth_campaigns_mounted_path_));

    base::FilePath campaigns_file(
        growth_campaigns_mounted_path_.Append(kCampaignsFileName));
    CHECK(base::WriteFile(campaigns_file, data));
  }

  void SimulateSetupFlowAndVerifyComplete() {
    // Simulate successful online setup.
    enrollment_helper_.ExpectEnrollmentMode(
        policy::EnrollmentConfig::MODE_ATTESTATION);
    enrollment_helper_.ExpectAttestationEnrollmentSuccess();
    SimulateNetworkConnected();

    TriggerDemoModeOnWelcomeScreen();

    UseOnlineModeOnNetworkScreen();

    // Test a couple valid inputs, verify the "continue" button is enabled.
    SetAndVerifyValidRetailerNameAndStoreNumber("Ret@iler with $ymb0ls",
                                                "0000");
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
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::FilePath growth_campaigns_mounted_path_;
};

IN_PROC_BROWSER_TEST_F(DemoSetupGrowthFrameworkEnabledTest,
                       OnlineSetupFlowSuccessWithNoCampaignsFile) {
  SimulateSetupFlowAndVerifyComplete();
  // Verify that loading growth component failed silently and fetching campaign
  // returns nullptr.
  EXPECT_EQ(nullptr, growth::CampaignsManager::Get()->GetCampaignBySlot(
                         growth::Slot::kDemoModeApp));
}

IN_PROC_BROWSER_TEST_F(DemoSetupGrowthFrameworkEnabledTest,
                       OnlineSetupFlowSuccessWithCampaigns) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  CreateTestCampaignsFile(R"({
    "0": [
      {
        "id": 3,
        "targetings": [],
        "payload": {
          "demoModeApp": {
            "attractionLoop": {
              "videoSrcLang1": "/asset/peripherals_lang1.mp4",
              "videoSrcLang2": "/asset/peripherals_lang2.mp4"
            }
          }
        }
      }
    ]
  })");

  SimulateSetupFlowAndVerifyComplete();

  // Verify that loading growth component and fetching campaign successfully.
  const auto* campaign = growth::CampaignsManager::Get()->GetCampaignBySlot(
      growth::Slot::kDemoModeApp);
  const auto* payload = campaign->FindDictByDottedPath("payload.demoModeApp");
  ASSERT_EQ("/asset/peripherals_lang1.mp4",
            *payload->FindStringByDottedPath("attractionLoop.videoSrcLang1"));
  ASSERT_EQ("/asset/peripherals_lang2.mp4",
            *payload->FindStringByDottedPath("attractionLoop.videoSrcLang2"));
}

}  // namespace
}  // namespace ash
