// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"

#include <string>

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
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/chromeos/login/mock_network_state_helper.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/demo_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
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

constexpr char kIsConfirmationDialogHiddenQuery[] =
    "!document.querySelector('.cr-dialog-container') || "
    "!!document.querySelector('.cr-dialog-container').hidden";

constexpr char kDefaultNetworkServicePath[] = "/service/eth1";
constexpr char kDefaultNetworkName[] = "eth1";

constexpr int kInvokeDemoModeGestureTapsCount = 10;

// How js query is executed.
enum class JSExecution { kSync, kAsync };

// Buttons available on OOBE dialogs.
enum class OobeButton { kBack, kNext, kText };

// Dialogs that are a part of Demo Mode setup screens.
enum class DemoSetupDialog { kNetwork, kEula, kArcTos, kProgress, kError };

// Returns the tag of the given |button| type.
std::string ButtonToTag(OobeButton button) {
  switch (button) {
    case OobeButton::kBack:
      return "oobe-back-button";
    case OobeButton::kNext:
      return "oobe-next-button";
    case OobeButton::kText:
      return "oobe-text-button";
    default:
      NOTREACHED() << "This is not valid OOBE button type";
  }
}

// Returns js id of the given |dialog|.
std::string DialogToStringId(DemoSetupDialog dialog) {
  switch (dialog) {
    case DemoSetupDialog::kNetwork:
      return "networkDialog";
    case DemoSetupDialog::kEula:
      return "eulaDialog";
    case DemoSetupDialog::kArcTos:
      return "arcTosDialog";
    case DemoSetupDialog::kProgress:
      return "demoSetupProgressDialog";
    case DemoSetupDialog::kError:
      return "demoSetupErrorDialog";
    default:
      NOTREACHED() << "This dialog does not belong to Demo Mode setup screens";
  }
}

// Returns query to access the content of the given OOBE |screen| or empty
// string if the |screen| is not a part of Demo Mode setup flow.
std::string ScreenToContentQuery(OobeScreenId screen) {
  if (screen == DemoPreferencesScreenView::kScreenId)
    return "$('demo-preferences')";
  if (screen == NetworkScreenView::kScreenId)
    return "$('network-selection')";
  if (screen == EulaView::kScreenId)
    return "$('oobe-eula-md')";
  if (screen == ArcTermsOfServiceScreenView::kScreenId)
    return "$('arc-tos-root')";
  if (screen == DemoSetupScreenView::kScreenId)
    return "$('demo-setup')";
  NOTREACHED() << "This OOBE screen is not a part of Demo Mode setup flow";
  return std::string();
}

// Waits for js condition to be fulfilled.
void WaitForJsCondition(const std::string& js_condition) {
  test::OobeJS().CreateWaiter(js_condition)->Wait();
}

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
    DisableConfirmationDialogAnimations();
    branded_build_override_ = WizardController::ForceBrandedBuildForTesting();
    DisconnectAllNetworks();
  }

  bool IsScreenShown(OobeScreenId screen) {
    const std::string screen_name = screen.name;
    const std::string query = base::StrCat(
        {"!!document.querySelector('#", screen_name,
         "') && !document.querySelector('#", screen_name, "').hidden"});
    return test::OobeJS().GetBool(query);
  }

  bool IsConfirmationDialogShown() {
    return !test::OobeJS().GetBool(kIsConfirmationDialogHiddenQuery);
  }

  // TODO(michaelpg): Replace this with IsScreenDialogElementVisible, which is
  // more robust because it checks whether the element is actually rendered.
  // Do this after a branch in case it introduces flakiness.
  bool IsScreenDialogElementShown(OobeScreenId screen,
                                  DemoSetupDialog dialog,
                                  const std::string& element_selector) {
    const std::string element = base::StrCat(
        {ScreenToContentQuery(screen), ".$['", DialogToStringId(dialog),
         "'].querySelector('", element_selector, "')"});
    const std::string query =
        base::StrCat({"!!", element, " && !", element, ".hidden"});
    return test::OobeJS().GetBool(query);
  }

  bool IsScreenDialogElementVisible(OobeScreenId screen,
                                    DemoSetupDialog dialog,
                                    const std::string& element_selector) {
    const std::string element = base::StrCat(
        {ScreenToContentQuery(screen), ".$['", DialogToStringId(dialog),
         "'].querySelector('", element_selector, "')"});
    const std::string query =
        base::StrCat({"!!", element, " && ", element, ".offsetHeight > 0"});
    return test::OobeJS().GetBool(query);
  }

  bool IsScreenDialogElementEnabled(OobeScreenId screen,
                                    DemoSetupDialog dialog,
                                    const std::string& element_selector) {
    const std::string element = base::StrCat(
        {ScreenToContentQuery(screen), ".$['", DialogToStringId(dialog),
         "'].querySelector('", element_selector, "')"});
    const std::string query =
        base::StrCat({"!!", element, " && !", element, ".disabled"});
    return test::OobeJS().GetBool(query);
  }

  // Returns whether a custom item with |custom_item_name| is shown as a first
  // element on the network list.
  bool IsCustomNetworkListElementShown(const std::string& custom_item_name) {
    const std::string element_selector = base::StrCat(
        {ScreenToContentQuery(NetworkScreenView::kScreenId),
         ".getNetworkListItemWithQueryForTest('network-list-item')"});
    const std::string query =
        base::StrCat({"!!", element_selector, " && ", element_selector,
                      ".item.customItemName == '", custom_item_name, "' && !",
                      element_selector, ".hidden"});
    return test::OobeJS().GetBool(query);
  }

  // Returns whether error message is shown on demo setup error screen and
  // contains text consisting of strings identified by |error_message_id| and
  // |recovery_message_id|.
  bool IsErrorMessageShown(int error_message_id, int recovery_message_id) {
    const std::string element_selector =
        base::StrCat({ScreenToContentQuery(DemoSetupScreenView::kScreenId),
                      ".$['", DialogToStringId(DemoSetupDialog::kError),
                      "'].querySelector('div[slot=subtitle]')"});
    const std::string query = base::StrCat(
        {"!!", element_selector, " && ", element_selector, ".innerHTML == '",
         l10n_util::GetStringUTF8(error_message_id), " ",
         l10n_util::GetStringUTF8(recovery_message_id), "' && !",
         element_selector, ".hidden"});
    return test::OobeJS().GetBool(query);
  }

  void SetPlayStoreTermsForTesting() {
    test::ExecuteOobeJS(
        R"(login.ArcTermsOfServiceScreen.setTosForTesting(
              'Test Play Store Terms of Service');)");
  }

  void InvokeDemoModeWithAccelerator() {
    WizardController::default_controller()->HandleAccelerator(
        ash::LoginAcceleratorAction::kStartDemoMode);
  }

  void InvokeDemoModeWithTaps() {
    MultiTapOobeContainer(kInvokeDemoModeGestureTapsCount);
  }

  // Simulates multi-tap gesture that consists of |tapCount| clicks on the OOBE
  // outer-container.
  void MultiTapOobeContainer(int tapsCount) {
    const std::string query = base::StrCat(
        {"for (var i = 0; i < ", base::NumberToString(tapsCount), "; ++i)",
         "{ document.querySelector('#outer-container').click(); }"});
    test::ExecuteOobeJS(query);
  }

  void ClickOkOnConfirmationDialog() {
    test::ExecuteOobeJS("document.querySelector('.cr-dialog-ok').click();");
  }

  void ClickCancelOnConfirmationDialog() {
    test::ExecuteOobeJS("document.querySelector('.cr-dialog-cancel').click();");
  }

  // Simulates |button| click on a specified OOBE |screen|. Can be used for
  // screens that consists of one oobe-dialog element.
  void ClickOobeButton(OobeScreenId screen,
                       OobeButton button,
                       JSExecution execution) {
    ClickOobeButtonWithSelector(screen, ButtonToTag(button), execution);
  }

  // Simulates click on a button with |button_selector| on specified OOBE
  // |screen|. Can be used for screens that consists of one oobe-dialog element.
  void ClickOobeButtonWithSelector(OobeScreenId screen,
                                   const std::string& button_selector,
                                   JSExecution execution) {
    const std::string query = base::StrCat(
        {ScreenToContentQuery(screen), ".$$('oobe-dialog').querySelector('",
         button_selector, "').click();"});
    switch (execution) {
      case JSExecution::kAsync:
        test::ExecuteOobeJSAsync(query);
        return;
      case JSExecution::kSync:
        test::ExecuteOobeJS(query);
        return;
      default:
        NOTREACHED();
    }
  }

  // Simulates |button| click on a |dialog| of the specified OOBE |screen|.
  // Can be used for screens that consists of multiple oobe-dialog elements.
  void ClickScreenDialogButton(OobeScreenId screen,
                               DemoSetupDialog dialog,
                               OobeButton button,
                               JSExecution execution) {
    ClickScreenDialogButtonWithSelector(screen, dialog, ButtonToTag(button),
                                        execution);
  }

  // Simulates click on a button with |button_selector| on a |dialog| of the
  // specified OOBE |screen|. Can be used for screens that consist of multiple
  // oobe-dialog elements.
  void ClickScreenDialogButtonWithSelector(OobeScreenId screen,
                                           DemoSetupDialog dialog,
                                           const std::string& button_selector,
                                           JSExecution execution) {
    const std::string query = base::StrCat(
        {ScreenToContentQuery(screen), ".$['", DialogToStringId(dialog),
         "'].querySelector('", button_selector, "').click();"});
    switch (execution) {
      case JSExecution::kAsync:
        test::ExecuteOobeJSAsync(query);
        return;
      case JSExecution::kSync:
        test::ExecuteOobeJS(query);
        return;
      default:
        NOTREACHED();
    }
  }

  // Simulates click on the network list item. |element| should specify
  // the aria-label of the desired network-list-item.
  void ClickNetworkListElement(const std::string& name) {
    const std::string element =
        base::StrCat({ScreenToContentQuery(NetworkScreenView::kScreenId),
                      ".getNetworkListItemByNameForTest('", name, "')"});
    // We are looking up element by localized text. In Polymer v2 we might
    // get to this point when element still not have proper localized string,
    // and getNetworkListItemByNameForTest would return null.
    test::OobeJS().CreateWaiter(element)->Wait();
    test::OobeJS().CreateVisibilityWaiter(true, element)->Wait();

    const std::string query = base::StrCat({element, ".click()"});
    test::ExecuteOobeJSAsync(query);
  }

  void SkipToErrorDialog() {
    // Simulate online setup error.
    enrollment_helper_.ExpectEnrollmentMode(
        policy::EnrollmentConfig::MODE_ATTESTATION);
    enrollment_helper_.ExpectAttestationEnrollmentError(
        policy::EnrollmentStatus::ForRegistrationError(
            policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));

    // Enrollment type is set in the part of the flow that is skipped, That is
    // why we need to set it here.
    auto* const wizard_controller = WizardController::default_controller();
    wizard_controller->SimulateDemoModeSetupForTesting(
        DemoSession::DemoModeConfig::kOnline);
    wizard_controller->AdvanceToScreen(DemoSetupScreenView::kScreenId);

    OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
    // TODO(agawronska): Progress dialog transition is async - extra work is
    // needed to be able to check it reliably.
    WaitForScreenDialog(DemoSetupScreenView::kScreenId,
                        DemoSetupDialog::kError);
  }

  void WaitForScreenDialog(OobeScreenId screen, DemoSetupDialog dialog) {
    const std::string query =
        base::StrCat({"!", ScreenToContentQuery(screen), ".$['",
                      DialogToStringId(dialog), "'].hidden"});
    WaitForJsCondition(query);
  }

  void SkipToScreen(OobeScreenId screen) {
    auto* const wizard_controller = WizardController::default_controller();
    wizard_controller->SimulateDemoModeSetupForTesting();
    wizard_controller->AdvanceToScreen(screen);

    OobeScreenWaiter(screen).Wait();
    EXPECT_TRUE(IsScreenShown(screen));
  }

  DemoSetupScreen* GetDemoSetupScreen() {
    return static_cast<DemoSetupScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            DemoSetupScreenView::kScreenId));
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

 protected:
  test::EnrollmentHelperMixin enrollment_helper_{&mixin_host_};

 private:
  void DisableConfirmationDialogAnimations() {
    test::ExecuteOobeJS(
        "cr.ui.dialogs.BaseDialog.ANIMATE_STABLE_DURATION = 0;");
  }

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
};

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       ShowConfirmationDialogAndProceed) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoModeWithAccelerator();
  EXPECT_TRUE(IsConfirmationDialogShown());

  ClickOkOnConfirmationDialog();

  WaitForJsCondition(kIsConfirmationDialogHiddenQuery);
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       ShowConfirmationDialogAndCancel) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoModeWithAccelerator();
  EXPECT_TRUE(IsConfirmationDialogShown());

  ClickCancelOnConfirmationDialog();

  WaitForJsCondition(kIsConfirmationDialogHiddenQuery);
  EXPECT_FALSE(IsScreenShown(DemoPreferencesScreenView::kScreenId));
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, InvokeWithTaps) {
  // Use fake time to avoid flakiness.
  SetFakeTimeForMultiTapDetector(base::Time::UnixEpoch());
  EXPECT_FALSE(IsConfirmationDialogShown());

  MultiTapOobeContainer(10);
  EXPECT_TRUE(IsConfirmationDialogShown());
}

// TODO(https://crbug.com/1121422): Flaky on ChromeOS ASAN.
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
  EXPECT_FALSE(IsConfirmationDialogShown());

  MultiTapOobeContainer(5);
  EXPECT_FALSE(IsConfirmationDialogShown());

  // Advance time to make interval in between taps longer than expected by
  // multi-tap gesture detector.
  SetFakeTimeForMultiTapDetector(kFakeTime +
                                 base::TimeDelta::FromMilliseconds(500));

  MultiTapOobeContainer(5);
  EXPECT_FALSE(IsConfirmationDialogShown());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, OnlineSetupFlowSuccess) {
  // Simulate successful online setup.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  SimulateNetworkConnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));

  ClickOobeButton(DemoPreferencesScreenView::kScreenId, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
  EXPECT_TRUE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                           DemoSetupDialog::kNetwork,
                                           ButtonToTag(OobeButton::kNext)));

  ClickOobeButton(NetworkScreenView::kScreenId, OobeButton::kNext,
                  JSExecution::kAsync);

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));

  ClickScreenDialogButton(EulaView::kScreenId, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(ArcTermsOfServiceScreenView::kScreenId));

  SetPlayStoreTermsForTesting();

  EXPECT_TRUE(IsScreenDialogElementVisible(
      ArcTermsOfServiceScreenView::kScreenId, DemoSetupDialog::kArcTos,
      "#arcTosMetricsDemoApps"));

  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosNextButton", JSExecution::kSync);
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosAcceptButton", JSExecution::kAsync);

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
  EXPECT_TRUE(DemoSetupController::GetSubOrganizationEmail().empty());
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

// TODO(https://crbug.com/1121422): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OnlineSetupFlowSuccessWithCountryCustomization \
  DISABLED_OnlineSetupFlowSuccessWithCountryCustomization
#else
#define MAYBE_OnlineSetupFlowSuccessWithCountryCustomization \
  OnlineSetupFlowSuccessWithCountryCustomization
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_OnlineSetupFlowSuccessWithCountryCustomization) {
  // Simulate successful online setup.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  SimulateNetworkConnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));

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
    const std::string query = base::StrCat(
        {ScreenToContentQuery(DemoPreferencesScreenView::kScreenId),
         ".$$('oobe-dialog').querySelector('#countrySelect').$$('option[value="
         "\"",
         country_code, "\"]').innerHTML"});
    EXPECT_EQ(it->second, test::OobeJS().GetString(query));
  }

  // Select France as the Demo Mode country.
  const std::string select_country = base::StrCat(
      {ScreenToContentQuery(DemoPreferencesScreenView::kScreenId),
       ".$$('oobe-dialog').querySelector('#countrySelect').onSelected_('fr')"
       ";"});
  test::ExecuteOobeJSAsync(select_country);

  ClickOobeButton(DemoPreferencesScreenView::kScreenId, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
  EXPECT_TRUE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                           DemoSetupDialog::kNetwork,
                                           ButtonToTag(OobeButton::kNext)));

  ClickOobeButton(NetworkScreenView::kScreenId, OobeButton::kNext,
                  JSExecution::kAsync);

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));

  ClickScreenDialogButton(EulaView::kScreenId, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(ArcTermsOfServiceScreenView::kScreenId));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosNextButton", JSExecution::kSync);
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosAcceptButton", JSExecution::kAsync);

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
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

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));

  ClickOobeButton(DemoPreferencesScreenView::kScreenId, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
  EXPECT_TRUE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                           DemoSetupDialog::kNetwork,
                                           ButtonToTag(OobeButton::kNext)));

  ClickOobeButton(NetworkScreenView::kScreenId, OobeButton::kNext,
                  JSExecution::kAsync);

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));

  ClickScreenDialogButton(EulaView::kScreenId, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(ArcTermsOfServiceScreenView::kScreenId));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosNextButton", JSExecution::kSync);
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosAcceptButton", JSExecution::kAsync);

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(DemoSetupScreenView::kScreenId, DemoSetupDialog::kError);
  // Default error returned by MockDemoModeOnlineEnrollmentHelperCreator.
  EXPECT_TRUE(IsErrorMessageShown(IDS_DEMO_SETUP_TEMPORARY_ERROR,
                                  IDS_DEMO_SETUP_RECOVERY_RETRY));
  EXPECT_TRUE(IsScreenDialogElementShown(
      DemoSetupScreenView::kScreenId, DemoSetupDialog::kError, "#retryButton"));
  EXPECT_FALSE(IsScreenDialogElementShown(DemoSetupScreenView::kScreenId,
                                          DemoSetupDialog::kError,
                                          "#powerwashButton"));
  EXPECT_TRUE(IsScreenDialogElementEnabled(DemoSetupScreenView::kScreenId,
                                           DemoSetupDialog::kError,
                                           ButtonToTag(OobeButton::kBack)));

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
          chromeos::InstallAttributes::LOCK_ALREADY_LOCKED));
  SimulateNetworkConnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));

  ClickOobeButton(DemoPreferencesScreenView::kScreenId, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
  EXPECT_TRUE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                           DemoSetupDialog::kNetwork,
                                           ButtonToTag(OobeButton::kNext)));

  ClickOobeButton(NetworkScreenView::kScreenId, OobeButton::kNext,
                  JSExecution::kAsync);

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));

  ClickScreenDialogButton(EulaView::kScreenId, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(ArcTermsOfServiceScreenView::kScreenId));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosNextButton", JSExecution::kSync);
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosAcceptButton", JSExecution::kAsync);

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(DemoSetupScreenView::kScreenId, DemoSetupDialog::kError);
  EXPECT_TRUE(IsErrorMessageShown(IDS_DEMO_SETUP_ALREADY_LOCKED_ERROR,
                                  IDS_DEMO_SETUP_RECOVERY_POWERWASH));
  EXPECT_FALSE(IsScreenDialogElementShown(
      DemoSetupScreenView::kScreenId, DemoSetupDialog::kError, "#retryButton"));
  EXPECT_TRUE(IsScreenDialogElementShown(DemoSetupScreenView::kScreenId,
                                         DemoSetupDialog::kError,
                                         "#powerwashButton"));
  EXPECT_FALSE(IsScreenDialogElementEnabled(DemoSetupScreenView::kScreenId,
                                            DemoSetupDialog::kError,
                                            ButtonToTag(OobeButton::kBack)));

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       OnlineSetupFlowCrosComponentFailure) {
  // Simulate failure to load demo resources CrOS component.
  // There is no enrollment attempt, as process fails earlier.
  enrollment_helper_.ExpectNoEnrollment();
  SimulateNetworkConnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  // Set the component to fail to install when requested.
  WizardController::default_controller()
      ->demo_setup_controller()
      ->SetCrOSComponentLoadErrorForTest(
          component_updater::CrOSComponentManager::Error::INSTALL_FAILURE);

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));

  ClickOobeButton(DemoPreferencesScreenView::kScreenId, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
  EXPECT_TRUE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                           DemoSetupDialog::kNetwork,
                                           ButtonToTag(OobeButton::kNext)));

  ClickOobeButton(NetworkScreenView::kScreenId, OobeButton::kNext,
                  JSExecution::kAsync);

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));

  ClickScreenDialogButton(EulaView::kScreenId, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(ArcTermsOfServiceScreenView::kScreenId));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosNextButton", JSExecution::kSync);
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosAcceptButton", JSExecution::kAsync);

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(DemoSetupScreenView::kScreenId, DemoSetupDialog::kError);
  EXPECT_TRUE(IsErrorMessageShown(IDS_DEMO_SETUP_COMPONENT_ERROR,
                                  IDS_DEMO_SETUP_RECOVERY_CHECK_NETWORK));
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

// TODO(https://crbug.com/1121422): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OfflineDemoModeUnavailable DISABLED_OfflineDemoModeUnavailable
#else
#define MAYBE_OfflineDemoModeUnavailable OfflineDemoModeUnavailable
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_OfflineDemoModeUnavailable) {
  SimulateNetworkDisconnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));

  ClickOobeButton(DemoPreferencesScreenView::kScreenId, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
  EXPECT_FALSE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                            DemoSetupDialog::kNetwork,
                                            ButtonToTag(OobeButton::kNext)));

  // Offline Demo Mode is not available when there are no preinstalled demo
  // resources.
  EXPECT_FALSE(IsCustomNetworkListElementShown("offlineDemoSetupListItemName"));
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, OfflineSetupFlowSuccess) {
  // Simulate offline setup success.
  enrollment_helper_.ExpectOfflineEnrollmentSuccess();
  SimulateNetworkDisconnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));

  ClickOobeButton(DemoPreferencesScreenView::kScreenId, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
  EXPECT_FALSE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                            DemoSetupDialog::kNetwork,
                                            ButtonToTag(OobeButton::kNext)));

  const std::string offline_setup_item_name =
      l10n_util::GetStringUTF8(IDS_NETWORK_OFFLINE_DEMO_SETUP_LIST_ITEM_NAME);
  ClickNetworkListElement(offline_setup_item_name);

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));

  ClickScreenDialogButton(EulaView::kScreenId, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kSync);

  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(ArcTermsOfServiceScreenView::kScreenId));

  SetPlayStoreTermsForTesting();

  EXPECT_TRUE(IsScreenDialogElementVisible(
      ArcTermsOfServiceScreenView::kScreenId, DemoSetupDialog::kArcTos,
      "#arcTosMetricsDemoApps"));

  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosNextButton", JSExecution::kSync);
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosAcceptButton", JSExecution::kAsync);

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

// TODO(https://crbug.com/1121422): Flaky on ChromeOS ASAN.
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

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));

  ClickOobeButton(DemoPreferencesScreenView::kScreenId, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
  EXPECT_FALSE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                            DemoSetupDialog::kNetwork,
                                            ButtonToTag(OobeButton::kNext)));

  const std::string offline_setup_item_name =
      l10n_util::GetStringUTF8(IDS_NETWORK_OFFLINE_DEMO_SETUP_LIST_ITEM_NAME);
  ClickNetworkListElement(offline_setup_item_name);

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));

  ClickScreenDialogButton(EulaView::kScreenId, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kSync);

  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(ArcTermsOfServiceScreenView::kScreenId));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosNextButton", JSExecution::kSync);
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosAcceptButton", JSExecution::kAsync);

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(DemoSetupScreenView::kScreenId, DemoSetupDialog::kError);
  // Default error returned by MockDemoModeOfflineEnrollmentHelperCreator.
  EXPECT_TRUE(IsErrorMessageShown(IDS_DEMO_SETUP_OFFLINE_POLICY_ERROR,
                                  IDS_DEMO_SETUP_RECOVERY_OFFLINE_FATAL));
  EXPECT_TRUE(IsScreenDialogElementShown(
      DemoSetupScreenView::kScreenId, DemoSetupDialog::kError, "#retryButton"));
  EXPECT_FALSE(IsScreenDialogElementShown(DemoSetupScreenView::kScreenId,
                                          DemoSetupDialog::kError,
                                          "#powerwashButton"));
  EXPECT_TRUE(IsScreenDialogElementEnabled(DemoSetupScreenView::kScreenId,
                                           DemoSetupDialog::kError,
                                           ButtonToTag(OobeButton::kBack)));

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

// TODO(https://crbug.com/1121422): Flaky on ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OfflineSetupFlowErrorPowerwashRequired \
  DISABLED_OfflineSetupFlowErrorPowerwashRequired
#else
#define MAYBE_OfflineSetupFlowErrorPowerwashRequired \
  OfflineSetupFlowErrorPowerwashRequired
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_OfflineSetupFlowErrorPowerwashRequired) {
  // Simulate offline setup failure.
  enrollment_helper_.ExpectOfflineEnrollmentError(
      policy::EnrollmentStatus::ForLockError(
          chromeos::InstallAttributes::LOCK_READBACK_ERROR));
  SimulateNetworkDisconnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));

  ClickOobeButton(DemoPreferencesScreenView::kScreenId, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
  EXPECT_FALSE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                            DemoSetupDialog::kNetwork,
                                            ButtonToTag(OobeButton::kNext)));

  const std::string offline_setup_item_name =
      l10n_util::GetStringUTF8(IDS_NETWORK_OFFLINE_DEMO_SETUP_LIST_ITEM_NAME);
  ClickNetworkListElement(offline_setup_item_name);

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));

  ClickScreenDialogButton(EulaView::kScreenId, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kSync);

  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(ArcTermsOfServiceScreenView::kScreenId));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosNextButton", JSExecution::kSync);
  ClickOobeButtonWithSelector(ArcTermsOfServiceScreenView::kScreenId,
                              "#arcTosAcceptButton", JSExecution::kAsync);

  OobeScreenWaiter(DemoSetupScreenView::kScreenId).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(DemoSetupScreenView::kScreenId, DemoSetupDialog::kError);
  EXPECT_TRUE(IsErrorMessageShown(IDS_DEMO_SETUP_LOCK_ERROR,
                                  IDS_DEMO_SETUP_RECOVERY_POWERWASH));
  EXPECT_FALSE(IsScreenDialogElementShown(
      DemoSetupScreenView::kScreenId, DemoSetupDialog::kError, "#retryButton"));
  EXPECT_TRUE(IsScreenDialogElementShown(DemoSetupScreenView::kScreenId,
                                         DemoSetupDialog::kError,
                                         "#powerwashButton"));
  EXPECT_FALSE(IsScreenDialogElementEnabled(DemoSetupScreenView::kScreenId,
                                            DemoSetupDialog::kError,
                                            ButtonToTag(OobeButton::kBack)));

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, NextDisabledOnNetworkScreen) {
  SimulateNetworkDisconnected();
  SkipToScreen(NetworkScreenView::kScreenId);
  EXPECT_FALSE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                            DemoSetupDialog::kNetwork,
                                            ButtonToTag(OobeButton::kNext)));

  ClickOobeButton(NetworkScreenView::kScreenId, OobeButton::kNext,
                  JSExecution::kSync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(NetworkScreenView::kScreenId));
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, ClickNetworkOnNetworkScreen) {
  SkipToScreen(NetworkScreenView::kScreenId);
  EXPECT_FALSE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                            DemoSetupDialog::kNetwork,
                                            ButtonToTag(OobeButton::kNext)));

  ClickNetworkListElement(kDefaultNetworkName);
  SimulateNetworkConnected();

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));
}

// TODO(https://crbug.com/1121422): Flaky on ChromeOS ASAN.
#if defined(OS_CHROMEOS)
#define MAYBE_ClickConnectedNetworkOnNetworkScreen \
  DISABLED_ClickConnectedNetworkOnNetworkScreen
#else
#define MAYBE_ClickConnectedNetworkOnNetworkScreen \
  ClickConnectedNetworkOnNetworkScreen
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       MAYBE_ClickConnectedNetworkOnNetworkScreen) {
  SimulateNetworkConnected();
  SkipToScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(IsScreenDialogElementEnabled(NetworkScreenView::kScreenId,
                                           DemoSetupDialog::kNetwork,
                                           ButtonToTag(OobeButton::kNext)));

  ClickNetworkListElement(kDefaultNetworkName);

  OobeScreenWaiter(EulaView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(EulaView::kScreenId));
}

// TODO(https://crbug.com/1121422): Flaky on ChromeOS ASAN.
#if defined(OS_CHROMEOS)
#define MAYBE_BackOnNetworkScreen DISABLED_BackOnNetworkScreen
#else
#define MAYBE_BackOnNetworkScreen BackOnNetworkScreen
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, MAYBE_BackOnNetworkScreen) {
  SimulateNetworkConnected();
  SkipToScreen(NetworkScreenView::kScreenId);

  ClickOobeButton(NetworkScreenView::kScreenId, OobeButton::kBack,
                  JSExecution::kAsync);

  OobeScreenWaiter(DemoPreferencesScreenView::kScreenId).Wait();
  EXPECT_TRUE(IsScreenShown(DemoPreferencesScreenView::kScreenId));
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, BackOnArcTermsScreen) {
  // User cannot go to ARC ToS screen without accepting eula - simulate that.
  StartupUtils::MarkEulaAccepted();

  SkipToScreen(ArcTermsOfServiceScreenView::kScreenId);

  ClickOobeButton(ArcTermsOfServiceScreenView::kScreenId, OobeButton::kBack,
                  JSExecution::kSync);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, BackOnErrorScreen) {
  SkipToErrorDialog();

  ClickScreenDialogButton(DemoSetupScreenView::kScreenId,
                          DemoSetupDialog::kError, OobeButton::kBack,
                          JSExecution::kAsync);

  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest, RetryOnErrorScreen) {
  SkipToErrorDialog();

  // We need to create another mock after showing error dialog.
  enrollment_helper_.ResetMock();
  // Simulate successful online setup on retry.
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  ClickScreenDialogButtonWithSelector(DemoSetupScreenView::kScreenId,
                                      DemoSetupDialog::kError, "#retryButton",
                                      JSExecution::kAsync);
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
}

// Test is flaky: crbug.com/1099402
IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       DISABLED_ShowOfflineSetupOptionOnNetworkList) {
  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting();
  SimulateOfflineEnvironment();
  SkipToScreen(NetworkScreenView::kScreenId);

  EXPECT_TRUE(IsCustomNetworkListElementShown("offlineDemoSetupListItemName"));
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcSupportedTest,
                       NoOfflineSetupOptionOnNetworkList) {
  SkipToScreen(NetworkScreenView::kScreenId);
  EXPECT_FALSE(IsCustomNetworkListElementShown("offlineDemoSetupListItemName"));
}

class DemoSetupProgressStepsTest : public DemoSetupTestBase {
 public:
  DemoSetupProgressStepsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kShowStepsInDemoModeSetup);
  }
  ~DemoSetupProgressStepsTest() override = default;

  // Checks how many steps have been rendered in the demo setup screen.
  int CountNumberOfStepsInUi() {
    const std::string query =
        "$('demo-setup').$$('oobe-dialog').querySelectorAll('progress-"
        "list-item').length";

    return test::OobeJS().GetInt(query);
  }

  // Checks how many steps are marked as pending in the demo setup screen.
  int CountPendingStepsInUi() {
    const std::string query =
        "Object.values($('demo-setup').$$('oobe-dialog')."
        "querySelectorAll('progress-list-item')).filter(node => "
        "node.shadowRoot.querySelector('#icon-pending:not([hidden])')).length";

    return test::OobeJS().GetInt(query);
  }

  // Checks how many steps are marked as active in the demo setup screen.
  int CountActiveStepsInUi() {
    const std::string query =
        "Object.values($('demo-setup').$$('oobe-dialog')."
        "querySelectorAll('progress-list-item')).filter(node => "
        "node.shadowRoot.querySelector('#icon-active:not([hidden])')).length";

    return test::OobeJS().GetInt(query);
  }

  // Checks how many steps are marked as complete in the demo setup screen.
  int CountCompletedStepsInUi() {
    const std::string query =
        "Object.values($('demo-setup').$$('oobe-dialog')."
        "querySelectorAll('progress-list-item')).filter(node => "
        "node.shadowRoot.querySelector('#icon-completed:not([hidden])'))."
        "length";

    return test::OobeJS().GetInt(query);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(DemoSetupProgressStepsTest);
};

IN_PROC_BROWSER_TEST_F(DemoSetupProgressStepsTest,
                       SetupProgessStepsDisplayCorrectly) {
  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting(
      DemoSession::DemoModeConfig::kOnline);
  SimulateNetworkConnected();
  SkipToScreen(DemoSetupScreenView::kScreenId);

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
    ASSERT_EQ(CountPendingStepsInUi(), numSteps - i - 1);
    ASSERT_EQ(CountActiveStepsInUi(), 1);
    ASSERT_EQ(CountCompletedStepsInUi(), i);
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
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoModeWithAccelerator();

  EXPECT_FALSE(IsConfirmationDialogShown());
}

IN_PROC_BROWSER_TEST_F(DemoSetupArcUnsupportedTest, DoNotInvokeWithTaps) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoModeWithTaps();

  EXPECT_FALSE(IsConfirmationDialogShown());
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

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting();
  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  // Enrollment type is set in the part of the flow that is skipped, That is
  // why we need to set it here.
  wizard_controller->demo_setup_controller()->set_demo_config(
      DemoSession::DemoModeConfig::kOffline);

  SkipToScreen(NetworkScreenView::kScreenId);
  SkipToScreen(DemoSetupScreenView::kScreenId);
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

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

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting();
  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  // Enrollment type is set in the part of the flow that is skipped, That is
  // why we need to set it here.
  wizard_controller->demo_setup_controller()->set_demo_config(
      DemoSession::DemoModeConfig::kOffline);

  SkipToScreen(NetworkScreenView::kScreenId);
  SkipToScreen(DemoSetupScreenView::kScreenId);
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

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

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting();
  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  // Enrollment type is set in the part of the flow that is skipped, That is
  // why we need to set it here.
  wizard_controller->demo_setup_controller()->set_demo_config(
      DemoSession::DemoModeConfig::kOffline);

  SkipToScreen(NetworkScreenView::kScreenId);
  SkipToScreen(DemoSetupScreenView::kScreenId);
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

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

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting();
  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  // Enrollment type is set in the part of the flow that is skipped, That is
  // why we need to set it here.
  wizard_controller->demo_setup_controller()->set_demo_config(
      DemoSession::DemoModeConfig::kOffline);

  SkipToScreen(NetworkScreenView::kScreenId);
  SkipToScreen(DemoSetupScreenView::kScreenId);
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(DemoSetupScreenView::kScreenId, DemoSetupDialog::kError);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

}  // namespace chromeos
