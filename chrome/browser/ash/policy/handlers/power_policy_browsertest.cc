// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/power_manager/policy.pb.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/power/power_api.h"
#include "extensions/common/api/power.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;
namespace pm = power_manager;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;

namespace policy {

namespace {

const char kLoginScreenPowerManagementPolicy[] =
    "{"
    "  \"AC\": {"
    "    \"Delays\": {"
    "      \"ScreenDim\": 5000,"
    "      \"ScreenOff\": 7000,"
    "      \"Idle\": 9000"
    "    },"
    "    \"IdleAction\": \"DoNothing\""
    "  },"
    "  \"Battery\": {"
    "    \"Delays\": {"
    "      \"ScreenDim\": 1000,"
    "      \"ScreenOff\": 3000,"
    "      \"Idle\": 4000"
    "    },"
    "    \"IdleAction\": \"DoNothing\""
    "  },"
    "  \"LidCloseAction\": \"DoNothing\","
    "  \"UserActivityScreenDimDelayScale\": 300"
    "}";

const char kPowerManagementIdleSettingsPolicy[] =
    "{"
    "  \"AC\": {"
    "    \"Delays\": {"
    "      \"ScreenDim\": 5000,"
    "      \"ScreenOff\": 7000,"
    "      \"IdleWarning\": 8000,"
    "      \"Idle\": 9000"
    "    },"
    "    \"IdleAction\": \"Logout\""
    "  },"
    "  \"Battery\": {"
    "    \"Delays\": {"
    "      \"ScreenDim\": 1000,"
    "      \"ScreenOff\": 3000,"
    "      \"IdleWarning\": 4000,"
    "      \"Idle\": 5000"
    "    },"
    "    \"IdleAction\": \"Logout\""
    "  }"
    "}";

const char kScreenLockDelayPolicy[] =
    "{"
    "  \"AC\": 6000,"
    "  \"Battery\": 2000"
    "}";

}  // namespace

class PowerPolicyBrowserTestBase : public DevicePolicyCrosBrowserTest {
 public:
  PowerPolicyBrowserTestBase(const PowerPolicyBrowserTestBase&) = delete;
  PowerPolicyBrowserTestBase& operator=(const PowerPolicyBrowserTestBase&) =
      delete;

 protected:
  PowerPolicyBrowserTestBase();

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override;

  void InstallUserKey();
  void StoreAndReloadUserPolicy();

  void StoreAndReloadDevicePolicyAndWaitForLoginProfileChange();

  // Returns a string describing |policy|.
  std::string GetDebugString(const pm::PowerManagementPolicy& policy);

  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  UserPolicyBuilder user_policy_;

 private:
  // Runs |closure| and waits for |profile|'s user policy to be updated as a
  // result.
  void RunClosureAndWaitForUserPolicyUpdate(base::OnceClosure closure,
                                            Profile* profile);

  // Reloads user policy for |profile| from session manager client.
  void ReloadUserPolicy(Profile* profile);
};

class PowerPolicyLoginScreenBrowserTest : public PowerPolicyBrowserTestBase {
 public:
  PowerPolicyLoginScreenBrowserTest(const PowerPolicyLoginScreenBrowserTest&) =
      delete;
  PowerPolicyLoginScreenBrowserTest& operator=(
      const PowerPolicyLoginScreenBrowserTest&) = delete;

 protected:
  PowerPolicyLoginScreenBrowserTest();

  // PowerPolicyBrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
};

class PowerPolicyInSessionBrowserTest : public PowerPolicyBrowserTestBase {
 public:
  PowerPolicyInSessionBrowserTest(const PowerPolicyInSessionBrowserTest&) =
      delete;
  PowerPolicyInSessionBrowserTest& operator=(
      const PowerPolicyInSessionBrowserTest&) = delete;

 protected:
  PowerPolicyInSessionBrowserTest();

  // PowerPolicyBrowserTestBase:
  void SetUpOnMainThread() override;
};

PowerPolicyBrowserTestBase::PowerPolicyBrowserTestBase() = default;

void PowerPolicyBrowserTestBase::SetUpOnMainThread() {
  DevicePolicyCrosBrowserTest::SetUpOnMainThread();

  // Initialize user policy.
  InstallUserKey();
  user_policy_.policy_data().set_username(
      user_manager::StubAccountId().GetUserEmail());
  user_policy_.policy_data().set_gaia_id(
      user_manager::StubAccountId().GetGaiaId());
}

void PowerPolicyBrowserTestBase::InstallUserKey() {
  base::FilePath user_keys_dir;
  ASSERT_TRUE(base::PathService::Get(chromeos::dbus_paths::DIR_USER_POLICY_KEYS,
                                     &user_keys_dir));
  std::string sanitized_username =
      ash::UserDataAuthClient::GetStubSanitizedUsername(
          cryptohome::CreateAccountIdentifierFromAccountId(
              user_manager::StubAccountId()));
  base::FilePath user_key_file =
      user_keys_dir.AppendASCII(sanitized_username).AppendASCII("policy.pub");
  std::string user_key_bits = user_policy_.GetPublicSigningKeyAsString();
  ASSERT_FALSE(user_key_bits.empty());
  ASSERT_TRUE(base::CreateDirectory(user_key_file.DirName()));
  ASSERT_TRUE(base::WriteFile(user_key_file, user_key_bits));
}

void PowerPolicyBrowserTestBase::StoreAndReloadUserPolicy() {
  // Install the new user policy blob in session manager client.
  user_policy_.Build();
  session_manager_client()->set_user_policy(
      cryptohome::CreateAccountIdentifierFromAccountId(
          AccountId::FromUserEmail(user_policy_.policy_data().username())),
      user_policy_.GetBlob());

  // Reload user policy from session manager client and wait for the update to
  // take effect.
  RunClosureAndWaitForUserPolicyUpdate(
      base::BindOnce(&PowerPolicyBrowserTestBase::ReloadUserPolicy,
                     base::Unretained(this), browser()->profile()),
      browser()->profile());
}

void PowerPolicyBrowserTestBase::
    StoreAndReloadDevicePolicyAndWaitForLoginProfileChange() {
  Profile* profile = ash::ProfileHelper::GetSigninProfile();
  ASSERT_TRUE(profile);

  // Install the new device policy blob in session manager client, reload device
  // policy from session manager client and wait for a change in the login
  // profile's policy to be observed.
  RunClosureAndWaitForUserPolicyUpdate(
      base::BindOnce(&PowerPolicyBrowserTestBase::RefreshDevicePolicy,
                     base::Unretained(this)),
      profile);
}

std::string PowerPolicyBrowserTestBase::GetDebugString(
    const pm::PowerManagementPolicy& policy) {
  return chromeos::PowerPolicyController::GetPolicyDebugString(policy);
}

void PowerPolicyBrowserTestBase::RunClosureAndWaitForUserPolicyUpdate(
    base::OnceClosure closure,
    Profile* profile) {
  base::RunLoop run_loop;
  MockPolicyServiceObserver observer;
  EXPECT_CALL(observer, OnPolicyUpdated(_, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(observer, OnPolicyServiceInitialized(_)).Times(AnyNumber());
  PolicyService* policy_service =
      profile->GetProfilePolicyConnector()->policy_service();
  ASSERT_TRUE(policy_service);
  policy_service->AddObserver(POLICY_DOMAIN_CHROME, &observer);
  std::move(closure).Run();
  run_loop.Run();
  policy_service->RemoveObserver(POLICY_DOMAIN_CHROME, &observer);
}

void PowerPolicyBrowserTestBase::ReloadUserPolicy(Profile* profile) {
  UserCloudPolicyManagerAsh* policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  ASSERT_TRUE(policy_manager);
  policy_manager->core()->store()->Load();
}

PowerPolicyLoginScreenBrowserTest::PowerPolicyLoginScreenBrowserTest() {}

void PowerPolicyLoginScreenBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PowerPolicyBrowserTestBase::SetUpCommandLine(command_line);
  command_line->AppendSwitch(ash::switches::kLoginManager);
  command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
}

void PowerPolicyLoginScreenBrowserTest::SetUpOnMainThread() {
  PowerPolicyBrowserTestBase::SetUpOnMainThread();

  // Wait for the login screen to be shown.
  ash::LoginOrLockScreenVisibleWaiter().Wait();
}

void PowerPolicyLoginScreenBrowserTest::TearDownOnMainThread() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::AttemptExit));
  base::RunLoop().RunUntilIdle();
  PowerPolicyBrowserTestBase::TearDownOnMainThread();
}

PowerPolicyInSessionBrowserTest::PowerPolicyInSessionBrowserTest() {}

void PowerPolicyInSessionBrowserTest::SetUpOnMainThread() {
  PowerPolicyBrowserTestBase::SetUpOnMainThread();
}

// Verifies that device policy is applied on the login screen.
IN_PROC_BROWSER_TEST_F(PowerPolicyLoginScreenBrowserTest, SetDevicePolicy) {
  pm::PowerManagementPolicy power_management_policy =
      power_manager_client()->policy();
  power_management_policy.mutable_ac_delays()->set_screen_dim_ms(5000);
  power_management_policy.mutable_ac_delays()->set_screen_off_ms(7000);
  power_management_policy.mutable_ac_delays()->set_idle_ms(9000);
  power_management_policy.mutable_battery_delays()->set_screen_dim_ms(1000);
  power_management_policy.mutable_battery_delays()->set_screen_off_ms(3000);
  power_management_policy.mutable_battery_delays()->set_idle_ms(4000);
  power_management_policy.set_ac_idle_action(
      pm::PowerManagementPolicy::DO_NOTHING);
  power_management_policy.set_battery_idle_action(
      pm::PowerManagementPolicy::DO_NOTHING);
  power_management_policy.set_lid_closed_action(
      pm::PowerManagementPolicy::DO_NOTHING);
  // Screen-dim scaling factors are disabled by PowerPolicyController when
  // smart-dimming is enabled. Smart-dim is enabled when PowerSmartDimEnabled is
  // set to true.
  power_management_policy.set_user_activity_screen_dim_delay_factor(1.0);

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_login_screen_power_management()
      ->set_login_screen_power_management(kLoginScreenPowerManagementPolicy);
  StoreAndReloadDevicePolicyAndWaitForLoginProfileChange();
  // Spin the run loop to ensure ash sees pref change.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(power_manager_client()->policy()));
}

// Verifies that device policy is ignored during a session.
IN_PROC_BROWSER_TEST_F(PowerPolicyInSessionBrowserTest, SetDevicePolicy) {
  pm::PowerManagementPolicy power_management_policy =
      power_manager_client()->policy();

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_login_screen_power_management()
      ->set_login_screen_power_management(kLoginScreenPowerManagementPolicy);
  StoreAndReloadDevicePolicyAndWaitForLoginProfileChange();
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(power_manager_client()->policy()));
}

// Verifies that legacy user policy is applied during a session.
IN_PROC_BROWSER_TEST_F(PowerPolicyInSessionBrowserTest, SetLegacyUserPolicy) {
  pm::PowerManagementPolicy power_management_policy =
      power_manager_client()->policy();
  power_management_policy.mutable_ac_delays()->set_screen_dim_ms(5000);
  power_management_policy.mutable_ac_delays()->set_screen_lock_ms(6000);
  power_management_policy.mutable_ac_delays()->set_screen_off_ms(7000);
  power_management_policy.mutable_ac_delays()->set_idle_warning_ms(8000);
  power_management_policy.mutable_ac_delays()->set_idle_ms(9000);
  power_management_policy.mutable_battery_delays()->set_screen_dim_ms(1000);
  power_management_policy.mutable_battery_delays()->set_screen_lock_ms(2000);
  power_management_policy.mutable_battery_delays()->set_screen_off_ms(3000);
  power_management_policy.mutable_battery_delays()->set_idle_warning_ms(4000);
  power_management_policy.mutable_battery_delays()->set_idle_ms(5000);
  power_management_policy.set_use_audio_activity(false);
  power_management_policy.set_use_video_activity(false);
  power_management_policy.set_ac_idle_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  power_management_policy.set_battery_idle_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  power_management_policy.set_lid_closed_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  // Screen-dim scaling factors are disabled by PowerPolicyController when
  // smart-dimming is enabled. Smart-dim is enabled when PowerSmartDimEnabled is
  // set to true.
  power_management_policy.set_presentation_screen_dim_delay_factor(1.0);
  power_management_policy.set_user_activity_screen_dim_delay_factor(1.0);
  power_management_policy.set_wait_for_initial_user_activity(true);

  user_policy_.payload().mutable_screendimdelayac()->set_value(5000);
  user_policy_.payload().mutable_screenlockdelayac()->set_value(6000);
  user_policy_.payload().mutable_screenoffdelayac()->set_value(7000);
  user_policy_.payload().mutable_idlewarningdelayac()->set_value(8000);
  user_policy_.payload().mutable_idledelayac()->set_value(9000);
  user_policy_.payload().mutable_screendimdelaybattery()->set_value(1000);
  user_policy_.payload().mutable_screenlockdelaybattery()->set_value(2000);
  user_policy_.payload().mutable_screenoffdelaybattery()->set_value(3000);
  user_policy_.payload().mutable_idlewarningdelaybattery()->set_value(4000);
  user_policy_.payload().mutable_idledelaybattery()->set_value(5000);
  user_policy_.payload().mutable_powermanagementusesaudioactivity()->set_value(
      false);
  user_policy_.payload().mutable_powermanagementusesvideoactivity()->set_value(
      false);
  user_policy_.payload().mutable_idleactionac()->set_value(
      chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  user_policy_.payload().mutable_idleactionbattery()->set_value(
      chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  user_policy_.payload().mutable_lidcloseaction()->set_value(
      chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  user_policy_.payload().mutable_presentationscreendimdelayscale()->set_value(
      300);
  user_policy_.payload().mutable_useractivityscreendimdelayscale()->set_value(
      300);
  user_policy_.payload().mutable_waitforinitialuseractivity()->set_value(true);
  StoreAndReloadUserPolicy();
  // Spin the run loop to ensure ash sees pref change.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(power_manager_client()->policy()));
}

// Verifies that legacy user policy is applied during a session.
// Same as SetLegacyUserPolicy above, except that smart-dim is disabled.
IN_PROC_BROWSER_TEST_F(PowerPolicyInSessionBrowserTest,
                       SetLegacyUserPolicySmartDimDisabled) {
  pm::PowerManagementPolicy power_management_policy =
      power_manager_client()->policy();
  power_management_policy.mutable_ac_delays()->set_screen_dim_ms(5000);
  power_management_policy.mutable_ac_delays()->set_screen_lock_ms(6000);
  power_management_policy.mutable_ac_delays()->set_screen_off_ms(7000);
  power_management_policy.mutable_ac_delays()->set_idle_warning_ms(8000);
  power_management_policy.mutable_ac_delays()->set_idle_ms(9000);
  power_management_policy.mutable_battery_delays()->set_screen_dim_ms(1000);
  power_management_policy.mutable_battery_delays()->set_screen_lock_ms(2000);
  power_management_policy.mutable_battery_delays()->set_screen_off_ms(3000);
  power_management_policy.mutable_battery_delays()->set_idle_warning_ms(4000);
  power_management_policy.mutable_battery_delays()->set_idle_ms(5000);
  power_management_policy.set_use_audio_activity(false);
  power_management_policy.set_use_video_activity(false);
  power_management_policy.set_ac_idle_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  power_management_policy.set_battery_idle_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  power_management_policy.set_lid_closed_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  power_management_policy.set_presentation_screen_dim_delay_factor(3.0);
  power_management_policy.set_user_activity_screen_dim_delay_factor(3.0);
  power_management_policy.set_wait_for_initial_user_activity(true);

  user_policy_.payload().mutable_screendimdelayac()->set_value(5000);
  user_policy_.payload().mutable_screenlockdelayac()->set_value(6000);
  user_policy_.payload().mutable_screenoffdelayac()->set_value(7000);
  user_policy_.payload().mutable_idlewarningdelayac()->set_value(8000);
  user_policy_.payload().mutable_idledelayac()->set_value(9000);
  user_policy_.payload().mutable_screendimdelaybattery()->set_value(1000);
  user_policy_.payload().mutable_screenlockdelaybattery()->set_value(2000);
  user_policy_.payload().mutable_screenoffdelaybattery()->set_value(3000);
  user_policy_.payload().mutable_idlewarningdelaybattery()->set_value(4000);
  user_policy_.payload().mutable_idledelaybattery()->set_value(5000);
  user_policy_.payload().mutable_powermanagementusesaudioactivity()->set_value(
      false);
  user_policy_.payload().mutable_powermanagementusesvideoactivity()->set_value(
      false);
  user_policy_.payload().mutable_idleactionac()->set_value(
      chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  user_policy_.payload().mutable_idleactionbattery()->set_value(
      chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  user_policy_.payload().mutable_lidcloseaction()->set_value(
      chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  user_policy_.payload().mutable_presentationscreendimdelayscale()->set_value(
      300);
  user_policy_.payload().mutable_useractivityscreendimdelayscale()->set_value(
      300);
  user_policy_.payload().mutable_waitforinitialuseractivity()->set_value(true);
  // Disable smart-dim.
  user_policy_.payload().mutable_powersmartdimenabled()->set_value(false);
  StoreAndReloadUserPolicy();
  // Spin the run loop to ensure ash sees pref change.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(power_manager_client()->policy()));
}

// Verifies that user policy is applied during a session.
IN_PROC_BROWSER_TEST_F(PowerPolicyInSessionBrowserTest, SetUserPolicy) {
  pm::PowerManagementPolicy power_management_policy =
      power_manager_client()->policy();
  power_management_policy.mutable_ac_delays()->set_screen_dim_ms(5000);
  power_management_policy.mutable_ac_delays()->set_screen_lock_ms(6000);
  power_management_policy.mutable_ac_delays()->set_screen_off_ms(7000);
  power_management_policy.mutable_ac_delays()->set_idle_warning_ms(8000);
  power_management_policy.mutable_ac_delays()->set_idle_ms(9000);
  power_management_policy.mutable_battery_delays()->set_screen_dim_ms(1000);
  power_management_policy.mutable_battery_delays()->set_screen_lock_ms(2000);
  power_management_policy.mutable_battery_delays()->set_screen_off_ms(3000);
  power_management_policy.mutable_battery_delays()->set_idle_warning_ms(4000);
  power_management_policy.mutable_battery_delays()->set_idle_ms(5000);
  power_management_policy.set_use_audio_activity(false);
  power_management_policy.set_use_video_activity(false);
  power_management_policy.set_ac_idle_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  power_management_policy.set_battery_idle_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  power_management_policy.set_lid_closed_action(
      pm::PowerManagementPolicy::STOP_SESSION);
  // Screen-dim scaling factors are disabled by PowerPolicyController when
  // smart-dimming is enabled. Smart-dim is enabled when PowerSmartDimEnabled is
  // set to true.
  power_management_policy.set_presentation_screen_dim_delay_factor(1.0);
  power_management_policy.set_user_activity_screen_dim_delay_factor(1.0);
  power_management_policy.set_wait_for_initial_user_activity(true);

  // Set legacy policies which are expected to be ignored.
  user_policy_.payload().mutable_screendimdelayac()->set_value(5555);
  user_policy_.payload().mutable_screenlockdelayac()->set_value(6666);
  user_policy_.payload().mutable_screenoffdelayac()->set_value(7777);
  user_policy_.payload().mutable_idlewarningdelayac()->set_value(8888);
  user_policy_.payload().mutable_idledelayac()->set_value(9999);
  user_policy_.payload().mutable_screendimdelaybattery()->set_value(1111);
  user_policy_.payload().mutable_screenlockdelaybattery()->set_value(2222);
  user_policy_.payload().mutable_screenoffdelaybattery()->set_value(3333);
  user_policy_.payload().mutable_idlewarningdelaybattery()->set_value(4444);
  user_policy_.payload().mutable_idledelaybattery()->set_value(5555);
  user_policy_.payload().mutable_idleactionac()->set_value(
      chromeos::PowerPolicyController::ACTION_SHUT_DOWN);
  user_policy_.payload().mutable_idleactionbattery()->set_value(
      chromeos::PowerPolicyController::ACTION_DO_NOTHING);

  // Set current policies which are expected to be honored.
  user_policy_.payload().mutable_powermanagementusesaudioactivity()->set_value(
      false);
  user_policy_.payload().mutable_powermanagementusesvideoactivity()->set_value(
      false);
  user_policy_.payload().mutable_lidcloseaction()->set_value(
      chromeos::PowerPolicyController::ACTION_STOP_SESSION);
  user_policy_.payload().mutable_presentationscreendimdelayscale()->set_value(
      300);
  user_policy_.payload().mutable_useractivityscreendimdelayscale()->set_value(
      300);
  user_policy_.payload().mutable_waitforinitialuseractivity()->set_value(true);

  user_policy_.payload().mutable_powermanagementidlesettings()->set_value(
      kPowerManagementIdleSettingsPolicy);
  user_policy_.payload().mutable_screenlockdelays()->set_value(
      kScreenLockDelayPolicy);

  StoreAndReloadUserPolicy();
  // Spin the run loop to ensure ash sees pref change.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetDebugString(power_management_policy),
            GetDebugString(power_manager_client()->policy()));
}

// Verifies that screen wake locks can be enabled and disabled by extensions and
// user policy during a session.
IN_PROC_BROWSER_TEST_F(PowerPolicyInSessionBrowserTest, AllowScreenWakeLocks) {
  pm::PowerManagementPolicy baseline_policy = power_manager_client()->policy();

  // Default settings should not report any wake locks.
  pm::PowerManagementPolicy power_management_policy = baseline_policy;
  EXPECT_FALSE(baseline_policy.screen_wake_lock());
  EXPECT_FALSE(baseline_policy.dim_wake_lock());
  EXPECT_FALSE(baseline_policy.system_wake_lock());

  // Pretend an extension grabs a screen wake lock.
  const char kExtensionId[] = "abcdefghijklmnopabcdefghijlkmnop";
  extensions::PowerAPI::Get(browser()->profile())
      ->AddRequest(kExtensionId, extensions::api::power::Level::kDisplay);

  // The PowerAPI requests system wake lock asynchronously.
  base::RunLoop run_loop;
  power_manager_client()->SetPowerPolicyQuitClosure(run_loop.QuitClosure());
  run_loop.Run();

  // Check that the lock is in effect (ignoring ac_idle_action,
  // battery_idle_action and reason).
  pm::PowerManagementPolicy policy = baseline_policy;
  policy.set_screen_wake_lock(true);
  policy.set_ac_idle_action(power_manager_client()->policy().ac_idle_action());
  policy.set_battery_idle_action(
      power_manager_client()->policy().battery_idle_action());
  policy.set_reason(power_manager_client()->policy().reason());
  EXPECT_EQ(GetDebugString(policy),
            GetDebugString(power_manager_client()->policy()));

  // Engage the user policy and verify that the screen wake lock is downgraded
  // to be a system wake lock.
  user_policy_.payload().mutable_allowscreenwakelocks()->set_value(false);
  StoreAndReloadUserPolicy();
  policy = baseline_policy;
  policy.set_system_wake_lock(true);
  policy.set_ac_idle_action(power_manager_client()->policy().ac_idle_action());
  policy.set_battery_idle_action(
      power_manager_client()->policy().battery_idle_action());
  policy.set_reason(power_manager_client()->policy().reason());
  // Spin the run loop to ensure ash sees pref change.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetDebugString(policy),
            GetDebugString(power_manager_client()->policy()));
}

}  // namespace policy
