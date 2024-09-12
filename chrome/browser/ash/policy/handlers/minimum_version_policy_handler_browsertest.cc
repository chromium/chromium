// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"

#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler_delegate_impl.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_test_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

const char kNewVersion[] = "13335.4.2";
const char kUpdatedVersion[] = "13340.0.0";
const char kCurrentVersion[] = "13332.0.25";
const int kNoWarning = 0;
const int kLastDayWarningInDays = 1;
const int kShortWarningInDays = 2;
const int kLongWarningInDays = 10;
const int kVeryLongWarningInDays = 100;
constexpr base::TimeDelta kShortWarning = base::Days(kShortWarningInDays);
constexpr base::TimeDelta kLongWarning = base::Days(kLongWarningInDays);
constexpr base::TimeDelta kVeryLongWarning = base::Days(kVeryLongWarningInDays);
const char kPublicSessionId[] = "demo@example.com";
const char kManagedUserId[] = "user@example.com";
const char kManagedUserGaiaId[] = "11111";
const char kUpdateRequiredNotificationId[] = "policy.update_required";
const char kWifiServicePath[] = "/service/wifi2";
const char kCellularServicePath[] = "/service/cellular1";

// This is a randomly chosen long delay in milliseconds to make sure that the
// timer keeps running for a long time in case it is started.
const int kAutoLoginLoginDelayMilliseconds = 500000;

MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetMinimumVersionPolicyHandler();
}

}  //  namespace

class MinimumVersionPolicyTestBase : public ash::LoginManagerTest {
 public:
  MinimumVersionPolicyTestBase();

  ~MinimumVersionPolicyTestBase() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    fake_update_engine_client_ =
        ash::UpdateEngineClient::InitializeFakeForTest();
    const std::string lsb_release =
        base::StringPrintf("CHROMEOS_RELEASE_VERSION=%s", kCurrentVersion);
    version_info_.emplace(lsb_release, base::Time::Now());
  }

  // Set new value for policy and wait till setting is changed.
  void SetDevicePolicyAndWaitForSettingChange(const base::Value::Dict& value);

  // Set new value for policy.
  void SetAndRefreshMinimumChromeVersionPolicy(const base::Value::Dict& value);

  void SetUpdateEngineStatus(update_engine::Operation operation);

 protected:
  void SetMinimumChromeVersionPolicy(const base::Value::Dict& value);

  DevicePolicyCrosTestHelper helper_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<ash::FakeUpdateEngineClient, DanglingUntriaged>
      fake_update_engine_client_ = nullptr;
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  std::optional<base::test::ScopedChromeOSVersionInfo> version_info_;
};

MinimumVersionPolicyTestBase::MinimumVersionPolicyTestBase() {
  feature_list_.InitAndEnableFeature(ash::features::kMinimumChromeVersion);
}

void MinimumVersionPolicyTestBase::SetMinimumChromeVersionPolicy(
    const base::Value::Dict& value) {
  DevicePolicyBuilder* const device_policy(helper_.device_policy());
  em::ChromeDeviceSettingsProto& proto(device_policy->payload());
  std::string policy_value;
  EXPECT_TRUE(base::JSONWriter::Write(value, &policy_value));
  proto.mutable_device_minimum_version()->set_value(policy_value);
}

void MinimumVersionPolicyTestBase::SetDevicePolicyAndWaitForSettingChange(
    const base::Value::Dict& value) {
  SetMinimumChromeVersionPolicy(value);
  helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
      {ash::kDeviceMinimumVersion});
}

void MinimumVersionPolicyTestBase::SetAndRefreshMinimumChromeVersionPolicy(
    const base::Value::Dict& value) {
  SetMinimumChromeVersionPolicy(value);
  helper_.RefreshDevicePolicy();
}

void MinimumVersionPolicyTestBase::SetUpdateEngineStatus(
    update_engine::Operation operation) {
  update_engine::StatusResult status;
  status.set_current_operation(operation);
  if (operation == update_engine::Operation::UPDATED_NEED_REBOOT) {
    status.set_new_version(kUpdatedVersion);
  }
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
}

class MinimumVersionPolicyTest : public MinimumVersionPolicyTestBase {
 public:
  MinimumVersionPolicyTest() { login_manager_.AppendRegularUsers(1); }
  ~MinimumVersionPolicyTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MinimumVersionPolicyTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(chromeos::switches::kShillStub,
                                    "clear=1,cellular=0,wifi=1");
  }

  void SetUpOnMainThread() override {
    MinimumVersionPolicyTestBase::SetUpOnMainThread();
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(nullptr /*profile*/);
    network_state_test_helper_ = std::make_unique<ash::NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    network_state_test_helper_->manager_test()->SetupDefaultEnvironment();
    tray_test_api_ = ash::SystemTrayTestApi::Create();
  }

  void TearDownOnMainThread() override {
    network_state_test_helper_.reset();
    MinimumVersionPolicyTestBase::TearDownOnMainThread();
  }

  void DisconectAllNetworks() { network_state_test_helper_->ClearServices(); }

  void ConnectCellularNetwork() {
    network_state_test_helper_->service_test()->AddService(
        kCellularServicePath, kCellularServicePath,
        kCellularServicePath /* name */, shill::kTypeCellular,
        shill::kStateOnline, true /* visible */);
  }

  void LoginManagedUser();
  void LoginUnmanagedUser();

 protected:
  const ash::LoginManagerMixin::TestUserInfo managed_user{
      AccountId::FromUserEmailGaiaId(kManagedUserId, kManagedUserGaiaId)};
  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_,
                                          managed_user.account_id};
  ash::LoginManagerMixin login_manager_{&mixin_host_, {managed_user}};
  std::unique_ptr<ash::NetworkStateTestHelper> network_state_test_helper_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
  std::unique_ptr<ash::SystemTrayTestApi> tray_test_api_;
};

void MinimumVersionPolicyTest::LoginManagedUser() {
  user_policy_mixin_.RequestPolicyUpdate();
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  auto context = ash::LoginManagerMixin::CreateDefaultUserContext(managed_user);
  login_manager_.LoginAndWaitForActiveSession(context);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
}

void MinimumVersionPolicyTest::LoginUnmanagedUser() {
  const auto& users = login_manager_.users();
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);

  LoginUser(users[1].account_id);
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::ACTIVE);
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, CriticalUpdateOnLoginScreen) {
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 2);
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Set new value for policy and check update required screen is shown on the
  // login screen.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kNoWarning, kNoWarning,
          false /* unmanaged_user_restricted */));
  ash::OobeScreenWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Revoke policy and check update required screen is hidden.
  SetDevicePolicyAndWaitForSettingChange(base::Value::Dict());
  ash::OobeScreenExitWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest,
                       CriticalUpdateOnLoginScreenForEolDevice) {
  // Configure "Enforce updates" in "Auto-update settings" in Admin Console:
  // - Choose the latest version in "if they are not running at least version"
  // - Set "Extend this period where devices which are not receiving automatic
  //   updates are not yet blocked to" to "No warning"
  // - Then ensure the user is logged out of a AUE device running an older
  //   version and can no longer log in.
  // COM_FOUND_CUJ23_TASK5_WF2
  base::AddTagToTestResult("feature_id",
                           "screenplay-6267b5f4-e674-4b94-920a-d99c83f701eb");

  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  fake_update_engine_client_->set_eol_date(
      base::DefaultClock::GetInstance()->Now() - base::Days(1));

  // Set new value for policy and check update required screen is shown on the
  // login screen.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kNoWarning, kNoWarning,
          false /* unmanaged_user_restricted */));
  ash::OobeScreenWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Revoke policy and check update required screen is hidden.
  SetDevicePolicyAndWaitForSettingChange(base::Value::Dict());
  ash::OobeScreenExitWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, PRE_CriticalUpdateInSession) {
  // Login the user into the session and mark as managed.
  LoginManagedUser();

  base::RunLoop run_loop;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(run_loop.QuitClosure());

  // Set new value for policy and check that user is logged out of the session.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kNoWarning, kNoWarning,
          false /* unmanaged_user_restricted */));
  run_loop.Run();
  EXPECT_TRUE(chrome::IsSendingStopRequestToSessionManager());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, CriticalUpdateInSession) {
  // Check login screen is shown post chrome restart due to critical update
  // required in session.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 2);
  // TODO(https://crbug.com/1048607): Show update required screen after user is
  // logged out of session due to critical update required by policy.
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, NonCriticalUpdateGoodNetwork) {
  // Login the user into the session.
  LoginManagedUser();

  // Check deadline timer is not running and local state is not set.
  PrefService* prefs = g_browser_process->local_state();
  base::Time timer_start_time =
      prefs->GetTime(prefs::kUpdateRequiredTimerStartTime);
  EXPECT_TRUE(timer_start_time.is_null());
  EXPECT_FALSE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());

  // Create and set policy value with short warning time.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kShortWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));
  // Policy handler sets the local state and starts the deadline timer.
  timer_start_time = prefs->GetTime(prefs::kUpdateRequiredTimerStartTime);
  EXPECT_FALSE(timer_start_time.is_null());
  EXPECT_EQ(prefs->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod),
            kShortWarning);
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Create and set policy value with long warning time.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kLongWarningInDays, kLongWarningInDays,
          false /* unmanaged_user_restricted */));
  // Warning time is increased but timer start time does not change.
  EXPECT_EQ(prefs->GetTime(prefs::kUpdateRequiredTimerStartTime),
            timer_start_time);
  EXPECT_EQ(prefs->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod),
            kLongWarning);
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Create and set policy value with no warning time.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kNoWarning, kNoWarning,
          false /* unmanaged_user_restricted */));
  // Warning time is not reduced as policy does not allow to reduce deadline.
  EXPECT_EQ(prefs->GetTime(prefs::kUpdateRequiredTimerStartTime),
            timer_start_time);
  EXPECT_EQ(prefs->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod),
            kLongWarning);
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetState());

  // Simulate update installed from update_engine_client and check that timer
  // is reset but local state is not.
  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_FALSE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetState());
  EXPECT_FALSE(prefs->GetTime(prefs::kUpdateRequiredTimerStartTime).is_null());

  // New policy after update is downloaded does not restart the timer but just
  // updates the local state with longer warning period.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kVeryLongWarningInDays, kNoWarning,
          false /* unmanaged_user_restricted */));
  EXPECT_EQ(prefs->GetTime(prefs::kUpdateRequiredTimerStartTime),
            timer_start_time);
  EXPECT_EQ(prefs->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod),
            kVeryLongWarning);
  EXPECT_FALSE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetState());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, DeviceUpdateStatusChange) {
  // Login the user into the session.
  LoginUnmanagedUser();

  // Set policy value with warning time and check timer is started.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kShortWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetState());

  // Simulate channel switch rollback from update_engine_client and check that
  // timer is not reset.
  update_engine::StatusResult rollback_status;
  rollback_status.set_current_operation(
      update_engine::Operation::UPDATED_NEED_REBOOT);
  rollback_status.set_will_powerwash_after_reboot(true);
  fake_update_engine_client_->set_default_status(rollback_status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(rollback_status);
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetState());

  // Simulate enterprise rollback from update_engine_client and check that timer
  // is not reset.
  rollback_status.set_is_enterprise_rollback(true);
  fake_update_engine_client_->set_default_status(rollback_status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(rollback_status);
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetState());

  // Simulate update installed from update_engine_client and check that timer is
  // reset.
  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_FALSE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetState());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest,
                       CriticalUpdateInSessionUnmanagedUser) {
  // Login the user into the session.
  LoginUnmanagedUser();
  // Set new value for pref and check that user session is not terminated.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kNoWarning, kNoWarning,
          false /* unmanaged_user_restricted */));
  EXPECT_FALSE(chrome::IsSendingStopRequestToSessionManager());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest,
                       CriticalUpdateInSessionUnmanagedUserEnabled) {
  LoginUnmanagedUser();
  // Create and set policy value.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kNoWarning, kNoWarning,
          true /* unmanaged_user_restricted */));
  EXPECT_TRUE(chrome::IsSendingStopRequestToSessionManager());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, NoNetworkNotificationClick) {
  // Login the user into the session.
  DisconectAllNetworks();
  LoginManagedUser();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  EXPECT_FALSE(tray_test_api_->IsTrayBubbleOpen());

  // Set new policy value and check that update required notification is shown.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kShortWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Clicking on notification button opens the network settings and hides the
  // notification.
  display_service_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                         kUpdateRequiredNotificationId,
                                         0 /*action_index*/, std::nullopt);
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  EXPECT_TRUE(tray_test_api_->IsTrayBubbleOpen());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest,
                       HideNotificationOnGoodNetwork) {
  // Login the user into the session.
  DisconectAllNetworks();
  LoginManagedUser();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Set new policy value and check that update required notification is shown.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kShortWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Connecting to WiFi should hide the update required notification.
  base::RunLoop run_loop;
  display_service_tester_->SetNotificationClosedClosure(run_loop.QuitClosure());
  network_state_test_helper_->service_test()->AddService(
      kWifiServicePath, kWifiServicePath, kWifiServicePath /* name */,
      shill::kTypeWifi, shill::kStateOnline, true /* visible */);
  run_loop.Run();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, LastDayNotificationOnLogin) {
  // Configure "Enforce updates" in "Auto-update settings" in Admin Console:
  // - Choose the latest version in "if they are not running at least version"
  // - Set "Block devices & user sessions after" to "1 week"
  // - Then ensure that the device running an older version receives a
  //   notification.
  // COM_FOUND_CUJ23_TASK3_WF1
  base::AddTagToTestResult("feature_id",
                           "screenplay-62ab245b-b322-43df-8b01-370688e2c228");

  DisconectAllNetworks();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kLastDayWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));

  // Login the user into the session and check that notification is shown.
  LoginManagedUser();
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  EXPECT_FALSE(tray_test_api_->IsTrayBubbleOpen());

  // Clicking on the no network update required notification button opens the
  // network settings and hides the notification.
  display_service_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                         kUpdateRequiredNotificationId,
                                         0 /*action_index*/, std::nullopt);
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  EXPECT_TRUE(tray_test_api_->IsTrayBubbleOpen());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest,
                       LastDayNotificationOnLoginUnmanagedUser) {
  DisconectAllNetworks();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kLastDayWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));

  // Login the user into the session and check that notification is not shown
  // for unmanaged user.
  LoginUnmanagedUser();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest,
                       NotificationOnUnmanagedUserEnabled) {
  // Configure "Enforce updates" in "Auto-update settings" in Admin Console:
  // - Choose the latest version in "if they are not running at least version"
  // - Set "Extend this period where devices which are not receiving automatic
  //   updates are not yet blocked to" to "1 week".
  // - Then ensure that the AUE device running an older version receives a
  //   notification and user is not logged out.
  // COM_FOUND_CUJ23_TASK5_WF1
  base::AddTagToTestResult("feature_id",
                           "screenplay-7a1101a5-03c1-4cfd-a0c9-495145151a8b");

  fake_update_engine_client_->set_eol_date(
      base::DefaultClock::GetInstance()->Now() - base::Days(1));
  LoginUnmanagedUser();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Set policy and check that notification is shown to unmanaged user as it has
  // been set in the policy.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kShortWarningInDays, kShortWarningInDays,
          true /* unmanaged_user_restricted */));
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->ShouldShowUpdateRequiredEolBanner());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, NotificationsOnLogin) {
  DisconectAllNetworks();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kShortWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));

  // Login the user into the session and check that notification is not shown as
  // it is not the last day to update device.
  LoginManagedUser();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest,
                       MeteredNetworkNotificationClick) {
  // Connect to metered network and login as managed user.
  DisconectAllNetworks();
  ConnectCellularNetwork();
  LoginManagedUser();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Set new policy value and check that update required notification is shown.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kShortWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Clicking on notification button starts update and hides the notification.
  display_service_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                         kUpdateRequiredNotificationId,
                                         0 /*action_index*/, std::nullopt);
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);
  EXPECT_EQ(fake_update_engine_client_
                ->update_over_cellular_one_time_permission_count(),
            0);
  // Simulate update over metered connection.
  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);
  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  SetUpdateEngineStatus(update_engine::Operation::NEED_PERMISSION_TO_UPDATE);
  EXPECT_GE(fake_update_engine_client_
                ->update_over_cellular_one_time_permission_count(),
            1);
  EXPECT_GT(fake_update_engine_client_->request_update_check_call_count(), 1);
  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_FALSE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->GetState());
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, EolNotificationClick) {
  // Mark device end of life and login as managed user.
  fake_update_engine_client_->set_eol_date(
      base::DefaultClock::GetInstance()->Now() - base::Days(1));
  LoginManagedUser();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Set new policy value and check that update required notification is shown.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kShortWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));
  EXPECT_TRUE(
      GetMinimumVersionPolicyHandler()->IsDeadlineTimerRunningForTesting());
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));

  // Clicking on notification button opens settings page and hides notification.
  display_service_tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                         kUpdateRequiredNotificationId,
                                         0 /*action_index*/, std::nullopt);
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kUpdateRequiredNotificationId));
  Browser* settings_browser = chrome::FindLastActive();
  ASSERT_TRUE(settings_browser);
  EXPECT_EQ(
      settings_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
      "chrome://management/");
}

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyTest, RelaunchNotificationOverride) {
  LoginManagedUser();
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kShortWarningInDays, kShortWarningInDays,
          false /* unmanaged_user_restricted */));
  base::Time deadline =
      GetMinimumVersionPolicyHandler()->update_required_deadline_for_testing();

  // Simulate device updated.
  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  // Relaunch notifications are shown and the relaunch deadline is configured as
  // per the policy deadline.
  UpgradeDetector* upgrade_detector = UpgradeDetector::GetInstance();
  EXPECT_EQ(upgrade_detector->upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  EXPECT_EQ(upgrade_detector->GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            deadline);

  // Revoking update required should reset the overridden the relaunch
  // notifications.
  SetDevicePolicyAndWaitForSettingChange(base::Value::Dict());
  EXPECT_NE(upgrade_detector->GetAnnoyanceLevelDeadline(
                UpgradeDetector::UPGRADE_ANNOYANCE_HIGH),
            deadline);
}

class MinimumVersionNoUsersLoginTest : public MinimumVersionPolicyTestBase {
 public:
  MinimumVersionNoUsersLoginTest() = default;
  ~MinimumVersionNoUsersLoginTest() override = default;

 protected:
  ash::LoginManagerMixin login_manager_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(MinimumVersionNoUsersLoginTest,
                       CriticalUpdateOnLoginScreen) {
  ash::OobeScreenWaiter(ash::OobeBaseTest::GetFirstSigninScreen()).Wait();
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 0);
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kNoWarning, kNoWarning,
          false /* unmanaged_user_restricted */));

  // Check update required screen is shown on the login screen.
  ash::OobeScreenWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  // Revoke policy and check update required screen is hidden and gaia screen is
  // shown.
  SetDevicePolicyAndWaitForSettingChange(base::Value::Dict());
  ash::OobeScreenExitWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  ash::OobeScreenWaiter(ash::OobeBaseTest::GetFirstSigninScreen()).Wait();
}

class MinimumVersionPolicyPresentTest : public MinimumVersionPolicyTestBase {
 public:
  MinimumVersionPolicyPresentTest() {}
  ~MinimumVersionPolicyPresentTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MinimumVersionPolicyTestBase::SetUpInProcessBrowserTestFixture();
    // Create and set policy value.
    SetAndRefreshMinimumChromeVersionPolicy(
        CreateMinimumVersionSingleRequirementPolicyValue(
            kNewVersion, kNoWarning, kNoWarning,
            false /* unmanaged_user_restricted */));
  }
};

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyPresentTest,
                       DeadlineReachedNoUsers) {
  // Checks update required screen is shown at startup if there is no user in
  // the device.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  ash::OobeScreenWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class MinimumVersionExistingUserTest : public MinimumVersionPolicyPresentTest {
 public:
  MinimumVersionExistingUserTest() {
    // Start with user pods.
    login_mixin_.AppendManagedUsers(1);
  }

 protected:
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(MinimumVersionExistingUserTest, DeadlineReached) {
  // Checks update required screen is shown at startup if user is existing in
  // the device.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  ash::OobeScreenWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class MinimumVersionBeforeLoginHost : public MinimumVersionExistingUserTest {
 public:
  MinimumVersionBeforeLoginHost() {}
  ~MinimumVersionBeforeLoginHost() override = default;

  bool SetUpUserDataDirectory() override {
    // LoginManagerMixin sets up command line in the SetUpUserDataDirectory.
    if (!MinimumVersionPolicyTestBase::SetUpUserDataDirectory()) {
      return false;
    }
    // Postpone login host creation.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        ash::switches::kForceLoginManagerInTests);
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(MinimumVersionBeforeLoginHost, DeadlineReached) {
  // Checks update required screen is shown at startup if the policy handler is
  // invoked before login display host is created.
  EXPECT_EQ(ash::LoginDisplayHost::default_host(), nullptr);
  EXPECT_TRUE(GetMinimumVersionPolicyHandler());
  EXPECT_TRUE(GetMinimumVersionPolicyHandler()->DeadlineReached());
  ShowLoginWizard(ash::OOBE_SCREEN_UNKNOWN);
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  ash::OobeScreenWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class MinimumVersionPublicSessionAutoLoginTest
    : public MinimumVersionExistingUserTest {
 public:
  MinimumVersionPublicSessionAutoLoginTest() {}
  ~MinimumVersionPublicSessionAutoLoginTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MinimumVersionExistingUserTest::SetUpInProcessBrowserTestFixture();
    AddPublicSessionToDevicePolicy(kPublicSessionId);
  }

  void AddPublicSessionToDevicePolicy(const std::string& user) {
    em::ChromeDeviceSettingsProto& proto(helper_.device_policy()->payload());
    DeviceLocalAccountTestHelper::AddPublicSession(&proto, user);
    helper_.RefreshDevicePolicy();
    em::DeviceLocalAccountsProto* device_local_accounts =
        proto.mutable_device_local_accounts();
    device_local_accounts->set_auto_login_id(user);
    device_local_accounts->set_auto_login_delay(
        kAutoLoginLoginDelayMilliseconds);
    helper_.RefreshDevicePolicy();
  }
};

IN_PROC_BROWSER_TEST_F(MinimumVersionPublicSessionAutoLoginTest,
                       BlockAutoLogin) {
  // Checks public session auto login is blocked if update is required on
  // reboot.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  ash::OobeScreenWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_FALSE(
      ash::ExistingUserController::current_controller()->IsSigninInProgress());
  EXPECT_FALSE(ash::ExistingUserController::current_controller()
                   ->IsAutoLoginTimerRunningForTesting());
}

class MinimumVersionKioskAutoLoginTest : public MinimumVersionExistingUserTest {
 public:
  MinimumVersionKioskAutoLoginTest() = default;
  ~MinimumVersionKioskAutoLoginTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MinimumVersionExistingUserTest::SetUpInProcessBrowserTestFixture();

    // Set up kiosk auto-launch mode.
    em::ChromeDeviceSettingsProto& proto(helper_.device_policy()->payload());
    ash::KioskAppsMixin::AppendAutoLaunchKioskAccount(&proto);
    helper_.RefreshDevicePolicy();
  }

 private:
  base::AutoReset<bool> block_kiosk_launcher_exit_on_failure_ =
      ash::KioskTestHelper::BlockExitOnFailure();
};

// Checks kiosk auto launch is not blocked even if immediate update is required
// by DeviceMinimumVersion policy.
IN_PROC_BROWSER_TEST_F(MinimumVersionKioskAutoLoginTest, AllowAutoLaunch) {
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  ash::OobeScreenWaiter(ash::AppLaunchSplashScreenView::kScreenId).Wait();
  // Policy handler returns early when device is setup in auto launch kiosk
  // mode.
  PrefService* prefs = g_browser_process->local_state();
  const base::Time timer_start_time =
      prefs->GetTime(prefs::kUpdateRequiredTimerStartTime);
  const base::TimeDelta warning_time =
      prefs->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod);
  EXPECT_TRUE(timer_start_time.is_null());
  EXPECT_TRUE(warning_time.is_zero());
  EXPECT_FALSE(GetMinimumVersionPolicyHandler()->DeadlineReached());
}

class MinimumVersionTimerExpiredOnLogin
    : public MinimumVersionPolicyTestBase,
      public ash::LocalStateMixin::Delegate {
 public:
  MinimumVersionTimerExpiredOnLogin() = default;
  ~MinimumVersionTimerExpiredOnLogin() override = default;

  // ash::LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    // Set up local state to reflect that update required deadline has passed
    // when device is rebooted.
    const base::TimeDelta delta = base::Days(5);
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetTime(prefs::kUpdateRequiredTimerStartTime,
                   base::Time::Now() - delta);
    prefs->SetTimeDelta(prefs::kUpdateRequiredWarningPeriod, kShortWarning);
  }

  // MinimumVersionPolicyTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    MinimumVersionPolicyTestBase::SetUpInProcessBrowserTestFixture();
    SetAndRefreshMinimumChromeVersionPolicy(
        CreateMinimumVersionSingleRequirementPolicyValue(
            kNewVersion, kShortWarningInDays, kShortWarningInDays,
            false /* unmanaged_user_restricted */));
  }

 private:
  ash::LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(MinimumVersionTimerExpiredOnLogin, DeadlinePassed) {
  // Show update required screen as deadline to update the device has passed.
  EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
            session_manager::SessionState::LOGIN_PRIMARY);
  ash::OobeScreenWaiter(ash::UpdateRequiredView::kScreenId).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class MinimumVersionPolicyChildUser : public MinimumVersionPolicyTestBase {
 public:
  MinimumVersionPolicyChildUser() = default;
  ~MinimumVersionPolicyChildUser() override = default;

  void LoginChildUser() {
    login_manager_.SkipPostLoginScreens();
    user_policy_mixin_.RequestPolicyUpdate();
    login_manager_.LoginAsNewChildUser();
    login_manager_.WaitForActiveSession();
    EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
    EXPECT_EQ(user_manager::UserManager::Get()->GetActiveUser()->GetType(),
              user_manager::UserType::kChild);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::ACTIVE);
  }

 private:
  const ash::LoginManagerMixin::TestUserInfo child_user{
      AccountId::FromUserEmailGaiaId(ash::test::kTestEmail,
                                     ash::test::kTestGaiaId)};
  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_, child_user.account_id};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  ash::LoginManagerMixin login_manager_{&mixin_host_, {}, &fake_gaia_};
};

IN_PROC_BROWSER_TEST_F(MinimumVersionPolicyChildUser,
                       CriticalUpdateInSessionChild) {
  LoginChildUser();

  // Child user is not enterprise managed and should not be signed out as
  // unmanaged users are not restricted by policy.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kNoWarning, kNoWarning,
          false /* unmanaged_user_restricted */));
  EXPECT_FALSE(chrome::IsSendingStopRequestToSessionManager());

  // Reset the policy so that it can be applied again.
  SetDevicePolicyAndWaitForSettingChange(base::Value::Dict());

  // Child user should be signout out as policy now restricts unmanaged users.
  SetDevicePolicyAndWaitForSettingChange(
      CreateMinimumVersionSingleRequirementPolicyValue(
          kNewVersion, kNoWarning, kNoWarning,
          true /* unmanaged_user_restricted */));
  EXPECT_TRUE(chrome::IsSendingStopRequestToSessionManager());
}

}  // namespace policy
