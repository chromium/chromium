// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"

#include <string>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time_to_iso8601.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/mock_network_state_helper.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/demo_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/screen_exit_code.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_util.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::test::DemoModeSetupResult;
using chromeos::test::MockDemoModeOfflineEnrollmentHelperCreator;
using chromeos::test::MockDemoModeOnlineEnrollmentHelperCreator;
using chromeos::test::SetupDummyOfflinePolicyDir;

namespace chromeos {

namespace {

constexpr char kIsConfirmationDialogHiddenQuery[] =
    "!document.querySelector('.cr-dialog-container') || "
    "!!document.querySelector('.cr-dialog-container').hidden";

constexpr char kDefaultNetworkServicePath[] = "/service/eth1";
constexpr char kDefaultNetworkName[] = "eth1";

constexpr base::TimeDelta kJsConditionCheckFrequency =
    base::TimeDelta::FromMilliseconds(10);

constexpr int kInvokeDemoModeGestureTapsCount = 10;

// How js query is executed.
enum class JSExecution { kSync, kAsync };

// Buttons available on OOBE dialogs.
enum class OobeButton { kBack, kNext, kText };

// Dialogs that are a part of Demo Mode setup screens.
enum class DemoSetupDialog { kNetwork, kEula, kProgress, kError };

// Returns js id of the given |button| type.
std::string ButtonToStringId(OobeButton button) {
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
std::string ScreenToContentQuery(OobeScreen screen) {
  switch (screen) {
    case OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES:
      return "$('demo-preferences-content')";
    case OobeScreen::SCREEN_OOBE_NETWORK:
      return "$('oobe-network-md')";
    case OobeScreen::SCREEN_OOBE_EULA:
      return "$('oobe-eula-md')";
    case OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE:
      return "$('arc-tos-root')";
    case OobeScreen::SCREEN_OOBE_UPDATE:
      return "$('oobe-update-md')";
    case OobeScreen::SCREEN_OOBE_DEMO_SETUP:
      return "$('demo-setup-content')";
    default: {
      NOTREACHED() << "This OOBE screen is not a part of Demo Mode setup flow";
      return std::string();
    }
  }
}

// Waits for js condition to be fulfilled.
class JsConditionWaiter {
 public:
  JsConditionWaiter(const test::JSChecker& js_checker,
                    const std::string& js_condition)
      : js_checker_(js_checker), js_condition_(js_condition) {}

  ~JsConditionWaiter() = default;

  void Wait() {
    if (IsConditionFulfilled())
      return;

    timer_.Start(FROM_HERE, kJsConditionCheckFrequency, this,
                 &JsConditionWaiter::CheckCondition);
    run_loop_.Run();
  }

 private:
  bool IsConditionFulfilled() { return js_checker_.GetBool(js_condition_); }

  void CheckCondition() {
    if (IsConditionFulfilled()) {
      run_loop_.Quit();
      timer_.Stop();
    }
  }

  test::JSChecker js_checker_;
  const std::string js_condition_;

  base::RepeatingTimer timer_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(JsConditionWaiter);
};

}  // namespace

// Basic tests for demo mode setup flow.
class DemoSetupTest : public LoginManagerTest {
 public:
  DemoSetupTest()
      : LoginManagerTest(false, true /* should_initialize_webui */) {}
  ~DemoSetupTest() override = default;

  // LoginTestManager:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kEnableOfflineDemoMode);
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    ASSERT_TRUE(arc::IsArcAvailable());
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    DisableConfirmationDialogAnimations();
    WizardController::default_controller()->is_official_build_ = true;
    DisconnectAllNetworks();
  }

  bool IsScreenShown(OobeScreen screen) {
    const std::string screen_name = GetOobeScreenName(screen);
    const std::string query = base::StrCat(
        {"!!document.querySelector('#", screen_name,
         "') && !document.querySelector('#", screen_name, "').hidden"});
    return js_checker().GetBool(query);
  }

  bool IsConfirmationDialogShown() {
    return !js_checker().GetBool(kIsConfirmationDialogHiddenQuery);
  }

  bool IsDialogShown(OobeScreen screen, DemoSetupDialog dialog) {
    const std::string query =
        base::StrCat({"!", ScreenToContentQuery(screen), ".$.",
                      DialogToStringId(dialog), ".hidden"});
    return js_checker().GetBool(query);
  }

  bool IsScreenDialogElementShown(OobeScreen screen,
                                  DemoSetupDialog dialog,
                                  const std::string& element) {
    const std::string element_selector = base::StrCat(
        {ScreenToContentQuery(screen), ".$.", DialogToStringId(dialog),
         ".querySelector('", element, "')"});
    const std::string query = base::StrCat(
        {"!!", element_selector, " && !", element_selector, ".hidden"});
    return js_checker().GetBool(query);
  }

  bool IsScreenDialogElementEnabled(OobeScreen screen,
                                    DemoSetupDialog dialog,
                                    const std::string& element) {
    const std::string element_selector = base::StrCat(
        {ScreenToContentQuery(screen), ".$.", DialogToStringId(dialog),
         ".querySelector('", element, "')"});
    const std::string query = base::StrCat(
        {"!!", element_selector, " && !", element_selector, ".disabled"});
    return js_checker().GetBool(query);
  }

  // Returns whether a custom item with |custom_item_name| is shown as a first
  // element on the network list.
  bool IsCustomNetworkListElementShown(const std::string& custom_item_name) {
    const std::string element_selector = base::StrCat(
        {ScreenToContentQuery(OobeScreen::SCREEN_OOBE_NETWORK),
         ".getNetworkListItemWithQueryForTest('cr-network-list-item')"});
    const std::string query =
        base::StrCat({"!!", element_selector, " && ", element_selector,
                      ".item.customItemName == '", custom_item_name, "' && !",
                      element_selector, ".hidden"});
    return js_checker().GetBool(query);
  }

  // Returns whether error message is shown on demo setup error screen and
  // contains text consisting of strings identified by |error_message_id| and
  // |recovery_message_id|.
  bool IsErrorMessageShown(int error_message_id, int recovery_message_id) {
    const std::string element_selector =
        base::StrCat({ScreenToContentQuery(OobeScreen::SCREEN_OOBE_DEMO_SETUP),
                      ".$.", DialogToStringId(DemoSetupDialog::kError),
                      ".querySelector('div[slot=subtitle]')"});
    const std::string query = base::StrCat(
        {"!!", element_selector, " && ", element_selector, ".innerHTML == '",
         l10n_util::GetStringUTF8(error_message_id), " ",
         l10n_util::GetStringUTF8(recovery_message_id), "' && !",
         element_selector, ".hidden"});
    return js_checker().GetBool(query);
  }

  void SetPlayStoreTermsForTesting() {
    EXPECT_TRUE(
        JSExecute("login.ArcTermsOfServiceScreen.setTosForTesting('Test "
                  "Play Store Terms of Service');"));
  }

  void InvokeDemoModeWithAccelerator() {
    EXPECT_TRUE(JSExecute("cr.ui.Oobe.handleAccelerator('demo_mode');"));
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
    EXPECT_TRUE(JSExecute(query));
  }

  void ClickOkOnConfirmationDialog() {
    EXPECT_TRUE(JSExecute("document.querySelector('.cr-dialog-ok').click();"));
  }

  void ClickCancelOnConfirmationDialog() {
    EXPECT_TRUE(
        JSExecute("document.querySelector('.cr-dialog-cancel').click();"));
  }

  // Simulates |button| click on a specified OOBE |screen|. Can be used for
  // screens that consists of one oobe-dialog element.
  void ClickOobeButton(OobeScreen screen,
                       OobeButton button,
                       JSExecution execution) {
    ClickOobeButtonWithId(screen, ButtonToStringId(button), execution);
  }

  // Simulates click on a button with |button_id| on specified OOBE |screen|.
  // Can be used for screens that consists of one oobe-dialog element.
  void ClickOobeButtonWithId(OobeScreen screen,
                             const std::string& button_id,
                             JSExecution execution) {
    const std::string query = base::StrCat(
        {ScreenToContentQuery(screen), ".$$('oobe-dialog').querySelector('",
         button_id, "').click();"});
    switch (execution) {
      case JSExecution::kAsync:
        JSExecuteAsync(query);
        return;
      case JSExecution::kSync:
        EXPECT_TRUE(JSExecute(query));
        return;
      default:
        NOTREACHED();
    }
  }

  // Simulates |button| click on a |dialog| of the specified OOBE |screen|.
  // Can be used for screens that consists of multiple oobe-dialog elements.
  void ClickScreenDialogButton(OobeScreen screen,
                               DemoSetupDialog dialog,
                               OobeButton button,
                               JSExecution execution) {
    ClickScreenDialogButtonWithId(screen, dialog, ButtonToStringId(button),
                                  execution);
  }

  // Simulates click on a button with |button_id| on a |dialog| of the specified
  // OOBE |screen|. Can be used for screens that consists of multiple
  // oobe-dialog elements.
  void ClickScreenDialogButtonWithId(OobeScreen screen,
                                     DemoSetupDialog dialog,
                                     const std::string& button_id,
                                     JSExecution execution) {
    const std::string query = base::StrCat(
        {ScreenToContentQuery(screen), ".$.", DialogToStringId(dialog),
         ".querySelector('", button_id, "').click();"});
    switch (execution) {
      case JSExecution::kAsync:
        JSExecuteAsync(query);
        return;
      case JSExecution::kSync:
        EXPECT_TRUE(JSExecute(query));
        return;
      default:
        NOTREACHED();
    }
  }

  // Simulates click on the network list item. |element| should specify
  // the aria-label of the desired cr-network-list-item.
  void ClickNetworkListElement(const std::string& element) {
    const std::string query =
        base::StrCat({ScreenToContentQuery(OobeScreen::SCREEN_OOBE_NETWORK),
                      ".getNetworkListItemWithQueryForTest('[aria-label=\"",
                      element, "\"]').click()"});
    JSExecuteAsync(query);
  }

  void SkipToErrorDialog() {
    // Simulate online setup error.
    EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
        &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::ERROR>);

    // Enrollment type is set in the part of the flow that is skipped, That is
    // why we need to set it here.
    auto* const wizard_controller = WizardController::default_controller();
    wizard_controller->SimulateDemoModeSetupForTesting(
        DemoSession::DemoModeConfig::kOnline);
    wizard_controller->AdvanceToScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP);

    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
    // TODO(agawronska): Progress dialog transition is async - extra work is
    // needed to be able to check it reliably.
    WaitForScreenDialog(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                        DemoSetupDialog::kError);
  }

  void WaitForScreenDialog(OobeScreen screen, DemoSetupDialog dialog) {
    const std::string query =
        base::StrCat({"!", ScreenToContentQuery(screen), ".$.",
                      DialogToStringId(dialog), ".hidden"});
    JsConditionWaiter(js_checker(), query).Wait();
  }

  void SkipToScreen(OobeScreen screen) {
    auto* const wizard_controller = WizardController::default_controller();
    wizard_controller->SimulateDemoModeSetupForTesting();
    wizard_controller->AdvanceToScreen(screen);

    OobeScreenWaiter(screen).Wait();
    EXPECT_TRUE(IsScreenShown(screen));
  }

  DemoSetupScreen* GetDemoSetupScreen() {
    return static_cast<DemoSetupScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            OobeScreen::SCREEN_OOBE_DEMO_SETUP));
  }

  void SimulateOfflineEnvironment() {
    DemoSetupController* controller =
        WizardController::default_controller()->demo_setup_controller();

    // Simulate offline data directory.
    ASSERT_TRUE(SetupDummyOfflinePolicyDir("test", &fake_policy_dir_));
    controller->SetOfflineDataDirForTest(fake_policy_dir_.GetPath());

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

  bool JSExecute(const std::string& script) {
    return content::ExecuteScript(web_contents(), script);
  }

  void JSExecuteAsync(const std::string& script) {
    content::ExecuteScriptAsync(web_contents(), script);
  }

  // Sets fake time in MultiTapDetector to remove dependency on real time in
  // test environment.
  void SetFakeTimeForMultiTapDetector(base::Time fake_time) {
    const std::string query =
        base::StrCat({"MultiTapDetector.FAKE_TIME_FOR_TESTS = new Date('",
                      base::TimeToISO8601(fake_time), "');"});
    EXPECT_TRUE(JSExecute(query));
  }

 private:
  void DisableConfirmationDialogAnimations() {
    EXPECT_TRUE(
        JSExecute("cr.ui.dialogs.BaseDialog.ANIMATE_STABLE_DURATION = 0;"));
  }

  // TODO(agawronska): Maybe create a separate test fixture for offline setup.
  base::ScopedTempDir fake_policy_dir_;
  policy::MockCloudPolicyStore mock_policy_store_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupTest);
};

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ShowConfirmationDialogAndProceed) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoModeWithAccelerator();
  EXPECT_TRUE(IsConfirmationDialogShown());

  ClickOkOnConfirmationDialog();

  JsConditionWaiter(js_checker(), kIsConfirmationDialogHiddenQuery).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));
}

#if defined(OS_CHROMEOS)
// Flaky on ChromeOS. crbug.com/895120
#define MAYBE_ShowConfirmationDialogAndCancel \
  DISABLED_ShowConfirmationDialogAndCancel
#else
#define MAYBE_ShowConfirmationDialogAndCancel ShowConfirmationDialogAndCancel
#endif
IN_PROC_BROWSER_TEST_F(DemoSetupTest, MAYBE_ShowConfirmationDialogAndCancel) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoModeWithAccelerator();
  EXPECT_TRUE(IsConfirmationDialogShown());

  ClickCancelOnConfirmationDialog();

  JsConditionWaiter(js_checker(), kIsConfirmationDialogHiddenQuery).Wait();
  EXPECT_FALSE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, InvokeWithTaps) {
  // Use fake time to avoid flakiness.
  SetFakeTimeForMultiTapDetector(base::Time::UnixEpoch());
  EXPECT_FALSE(IsConfirmationDialogShown());

  MultiTapOobeContainer(10);
  EXPECT_TRUE(IsConfirmationDialogShown());
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, DoNotInvokeWithNonConsecutiveTaps) {
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

IN_PROC_BROWSER_TEST_F(DemoSetupTest, OnlineSetupFlowSuccess) {
  // Simulate successful online setup.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::SUCCESS>);
  SimulateNetworkConnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_NETWORK));
  EXPECT_TRUE(IsScreenDialogElementEnabled(
      OobeScreen::SCREEN_OOBE_NETWORK, DemoSetupDialog::kNetwork,
      ButtonToStringId(OobeButton::kNext)));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_NETWORK, OobeButton::kNext,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_EULA));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_EULA, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-next-button", JSExecution::kSync);
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-accept-button", JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_UPDATE).Wait();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, OnlineSetupFlowError) {
  // Simulate online setup failure.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::ERROR>);
  SimulateNetworkConnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_NETWORK));
  EXPECT_TRUE(IsScreenDialogElementEnabled(
      OobeScreen::SCREEN_OOBE_NETWORK, DemoSetupDialog::kNetwork,
      ButtonToStringId(OobeButton::kNext)));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_NETWORK, OobeButton::kNext,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_EULA));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_EULA, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-next-button", JSExecution::kSync);
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-accept-button", JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_UPDATE).Wait();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                      DemoSetupDialog::kError);
  // Default error returned by MockDemoModeOnlineEnrollmentHelperCreator.
  EXPECT_TRUE(IsErrorMessageShown(IDS_DEMO_SETUP_TEMPORARY_ERROR,
                                  IDS_DEMO_SETUP_RECOVERY_RETRY));
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, OnlineSetupFlowCrosComponentFailure) {
  // Simulate failure to load demo resources CrOS component.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::SUCCESS>);
  SimulateNetworkConnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  // Set the component to fail to install when requested.
  WizardController::default_controller()
      ->demo_setup_controller()
      ->SetCrOSComponentLoadErrorForTest(
          component_updater::CrOSComponentManager::Error::INSTALL_FAILURE);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_NETWORK));
  EXPECT_TRUE(IsScreenDialogElementEnabled(
      OobeScreen::SCREEN_OOBE_NETWORK, DemoSetupDialog::kNetwork,
      ButtonToStringId(OobeButton::kNext)));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_NETWORK, OobeButton::kNext,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_EULA));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_EULA, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-next-button", JSExecution::kSync);
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-accept-button", JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_UPDATE).Wait();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                      DemoSetupDialog::kError);
  EXPECT_TRUE(IsErrorMessageShown(IDS_DEMO_SETUP_COMPONENT_ERROR,
                                  IDS_DEMO_SETUP_RECOVERY_CHECK_NETWORK));
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, OfflineSetupFlowSuccess) {
  // Simulate offline setup success.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<
          DemoModeSetupResult::SUCCESS>);
  SimulateNetworkDisconnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_NETWORK));
  EXPECT_FALSE(IsScreenDialogElementEnabled(
      OobeScreen::SCREEN_OOBE_NETWORK, DemoSetupDialog::kNetwork,
      ButtonToStringId(OobeButton::kNext)));

  const std::string offline_setup_item_name =
      l10n_util::GetStringUTF8(IDS_NETWORK_OFFLINE_DEMO_SETUP_LIST_ITEM_NAME);
  ClickNetworkListElement(offline_setup_item_name);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_EULA));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_EULA, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kSync);

  OobeScreenWaiter(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-next-button", JSExecution::kSync);
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-accept-button", JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, OfflineSetupFlowError) {
  // Simulate offline setup failure.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<DemoModeSetupResult::ERROR>);
  SimulateNetworkDisconnected();

  InvokeDemoModeWithAccelerator();
  ClickOkOnConfirmationDialog();

  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_NETWORK));
  EXPECT_FALSE(IsScreenDialogElementEnabled(
      OobeScreen::SCREEN_OOBE_NETWORK, DemoSetupDialog::kNetwork,
      ButtonToStringId(OobeButton::kNext)));

  const std::string offline_setup_item_name =
      l10n_util::GetStringUTF8(IDS_NETWORK_OFFLINE_DEMO_SETUP_LIST_ITEM_NAME);
  ClickNetworkListElement(offline_setup_item_name);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_EULA));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_EULA, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kSync);

  OobeScreenWaiter(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-next-button", JSExecution::kSync);
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-accept-button", JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                      DemoSetupDialog::kError);
  // Default error returned by MockDemoModeOfflineEnrollmentHelperCreator.
  EXPECT_TRUE(IsErrorMessageShown(IDS_DEMO_SETUP_LOCK_ERROR,
                                  IDS_DEMO_SETUP_RECOVERY_POWERWASH));

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, NextDisabledOnNetworkScreen) {
  SimulateNetworkDisconnected();
  SkipToScreen(OobeScreen::SCREEN_OOBE_NETWORK);

  EXPECT_FALSE(IsScreenDialogElementEnabled(
      OobeScreen::SCREEN_OOBE_NETWORK, DemoSetupDialog::kNetwork,
      ButtonToStringId(OobeButton::kNext)));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_NETWORK, OobeButton::kNext,
                  JSExecution::kSync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_NETWORK));
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ClickNetworkOnNetworkScreen) {
  SkipToScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_FALSE(IsScreenDialogElementEnabled(
      OobeScreen::SCREEN_OOBE_NETWORK, DemoSetupDialog::kNetwork,
      ButtonToStringId(OobeButton::kNext)));

  ClickNetworkListElement(kDefaultNetworkName);
  SimulateNetworkConnected();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_EULA));
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ClickConnectedNetworkOnNetworkScreen) {
  SimulateNetworkConnected();
  SkipToScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_TRUE(IsScreenDialogElementEnabled(
      OobeScreen::SCREEN_OOBE_NETWORK, DemoSetupDialog::kNetwork,
      ButtonToStringId(OobeButton::kNext)));

  ClickNetworkListElement(kDefaultNetworkName);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_EULA));
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, BackOnNetworkScreen) {
  SimulateNetworkConnected();
  SkipToScreen(OobeScreen::SCREEN_OOBE_NETWORK);

  ClickOobeButton(OobeScreen::SCREEN_OOBE_NETWORK, OobeButton::kBack,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, BackOnArcTermsScreen) {
  // User cannot go to ARC ToS screen without accepting eula - simulate that.
  StartupUtils::MarkEulaAccepted();

  SkipToScreen(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE);

  ClickOobeButton(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE, OobeButton::kBack,
                  JSExecution::kSync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, BackOnErrorScreen) {
  SkipToErrorDialog();

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                          DemoSetupDialog::kError, OobeButton::kBack,
                          JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, RetryOnErrorScreen) {
  SkipToErrorDialog();

  // Simulate successful online setup on retry.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::SUCCESS>);

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                          DemoSetupDialog::kError, OobeButton::kText,
                          JSExecution::kAsync);
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ShowOfflineSetupOptionOnNetworkList) {
  SkipToScreen(OobeScreen::SCREEN_OOBE_NETWORK);

  EXPECT_TRUE(IsCustomNetworkListElementShown("offlineDemoSetupListItemName"));
}

class DemoSetupOfflineDisabledTest : public DemoSetupTest {
 public:
  DemoSetupOfflineDisabledTest() = default;
  ~DemoSetupOfflineDisabledTest() override = default;

  // DemoSetupTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Don't add kEnableOfflineDemoMode.
    LoginManagerTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoSetupOfflineDisabledTest);
};

IN_PROC_BROWSER_TEST_F(DemoSetupOfflineDisabledTest,
                       NoOfflineSetupOptionOnNetworkList) {
  SkipToScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_FALSE(IsCustomNetworkListElementShown("offlineDemoSetupListItemName"));
}

class DemoSetupArcUnsupportedTest : public DemoSetupTest {
 public:
  DemoSetupArcUnsupportedTest() = default;
  ~DemoSetupArcUnsupportedTest() override = default;

  // DemoSetupTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kEnableOfflineDemoMode);
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
class DemoSetupFRETest : public DemoSetupTest {
 protected:
  DemoSetupFRETest() {
    statistics_provider_.SetMachineStatistic(system::kSerialNumberKeyForTest,
                                             "testserialnumber");
  }
  ~DemoSetupFRETest() override = default;

  void SetUpOnMainThread() override { DemoSetupTest::SetUpOnMainThread(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DemoSetupTest::SetUpCommandLine(command_line);

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
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<
          DemoModeSetupResult::SUCCESS>);
  SimulateNetworkDisconnected();

  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting();
  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  // Enrollment type is set in the part of the flow that is skipped, That is
  // why we need to set it here.
  wizard_controller->demo_setup_controller()->set_demo_config(
      DemoSession::DemoModeConfig::kOffline);
  wizard_controller->AdvanceToScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
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
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<
          DemoModeSetupResult::SUCCESS>);
  SimulateNetworkDisconnected();

  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting();
  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  // Enrollment type is set in the part of the flow that is skipped, That is
  // why we need to set it here.
  wizard_controller->demo_setup_controller()->set_demo_config(
      DemoSession::DemoModeConfig::kOffline);
  wizard_controller->AdvanceToScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
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
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<
          DemoModeSetupResult::SUCCESS>);
  SimulateNetworkDisconnected();

  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting();
  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  // Enrollment type is set in the part of the flow that is skipped, That is
  // why we need to set it here.
  wizard_controller->demo_setup_controller()->set_demo_config(
      DemoSession::DemoModeConfig::kOffline);
  wizard_controller->AdvanceToScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(DemoSetupFRETest, DeviceWithFRE) {
  // Simulating device that requires FRE. "check_enrollment", "block_devmode"
  // and "ActivateDate" flags are set.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2018-01");
  statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey, "1");
  statistics_provider_.SetMachineStatistic(system::kBlockDevModeKey, "1");

  // Simulate offline setup success.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<
          DemoModeSetupResult::SUCCESS>);
  SimulateNetworkDisconnected();

  auto* const wizard_controller = WizardController::default_controller();
  wizard_controller->SimulateDemoModeSetupForTesting();
  // It needs to be done after demo setup controller was created (demo setup
  // flow was started).
  SimulateOfflineEnvironment();
  // Enrollment type is set in the part of the flow that is skipped, That is
  // why we need to set it here.
  wizard_controller->demo_setup_controller()->set_demo_config(
      DemoSession::DemoModeConfig::kOffline);
  wizard_controller->AdvanceToScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  // TODO(agawronska): Progress dialog transition is async - extra work is
  // needed to be able to check it reliably.
  WaitForScreenDialog(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                      DemoSetupDialog::kError);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

}  // namespace chromeos
