// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/ash/child_accounts/time_limit_override.h"
#include "chrome/browser/ash/child_accounts/time_limit_test_utils.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Time delta representing the usage time limit warning time.
constexpr base::TimeDelta kUsageTimeLimitWarningTime = base::Minutes(15);

class TestScreenTimeControllerObserver : public ScreenTimeController::Observer {
 public:
  TestScreenTimeControllerObserver() = default;

  TestScreenTimeControllerObserver(const TestScreenTimeControllerObserver&) =
      delete;
  TestScreenTimeControllerObserver& operator=(
      const TestScreenTimeControllerObserver&) = delete;

  ~TestScreenTimeControllerObserver() override = default;

  int usage_time_limit_warnings() const { return usage_time_limit_warnings_; }

 private:
  void UsageTimeLimitWarning() override { usage_time_limit_warnings_++; }

  int usage_time_limit_warnings_ = 0;
};

}  // namespace

namespace utils = time_limit_test_utils;

class ScreenTimeControllerTest : public MixinBasedInProcessBrowserTest {
 public:
  ScreenTimeControllerTest() = default;

  ScreenTimeControllerTest(const ScreenTimeControllerTest&) = delete;
  ScreenTimeControllerTest& operator=(const ScreenTimeControllerTest&) = delete;

  ~ScreenTimeControllerTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    // A basic starting policy.
    base::Value::Dict policy_content =
        utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
    logged_in_user_mixin_.GetUserPolicyMixin()
        ->RequestPolicyUpdate()
        ->policy_payload()
        ->mutable_usagetimelimit()
        ->set_value(utils::PolicyToString(policy_content));
  }

 protected:
  void LogInChildAndSetupClockWithTime(const char* time) {
    SetupTaskRunnerWithTime(utils::TimeFromString(time));
    logged_in_user_mixin_.LogInUser();
    MockClockForActiveUser();
  }

  void SetupTaskRunnerWithTime(base::Time start_time) {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        start_time, base::TimeTicks::UnixEpoch());
  }

  void MockClockForActiveUser() {
    const user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    EXPECT_EQ(user_manager->GetActiveUser()->GetType(),
              user_manager::UserType::kChild);
    child_profile_ =
        ProfileHelper::Get()->GetProfileByUser(user_manager->GetActiveUser());

    // Mock time for ScreenTimeController.
    ScreenTimeControllerFactory::GetForBrowserContext(child_profile_)
        ->SetClocksForTesting(task_runner_->GetMockClock(),
                              task_runner_->GetMockTickClock(), task_runner_);
  }

  bool IsAuthEnabled() {
    return !ScreenLocker::default_screen_locker()
                ->IsAuthTemporarilyDisabledForUser(
                    logged_in_user_mixin_.GetAccountId());
  }

  const AccountId& GetAccountId() {
    return logged_in_user_mixin_.GetAccountId();
  }

  void MockChildScreenTime(base::TimeDelta used_time) {
    child_profile_->GetPrefs()->SetInteger(prefs::kChildScreenTimeMilliseconds,
                                           used_time.InMilliseconds());
  }

  bool IsLocked() {
    base::RunLoop().RunUntilIdle();
    return session_manager::SessionManager::Get()->IsScreenLocked();
  }

  void SetUsageTimeLimitPolicy(const base::Value::Dict& policy_content) {
    logged_in_user_mixin_.GetUserPolicyMixin()
        ->RequestPolicyUpdate()
        ->policy_payload()
        ->mutable_usagetimelimit()
        ->set_value(utils::PolicyToString(policy_content));
    logged_in_user_mixin_.GetUserPolicyTestHelper()->RefreshPolicyAndWait(
        child_profile_);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  raw_ptr<Profile, DanglingUntriaged> child_profile_ = nullptr;

 private:
  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kChild, /*include_initial_user=*/false};
};

// Tests a simple lock override.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, LockOverride) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 GMT");
  ScreenLockerTester().Lock();

  // Verify user is able to log in.
  EXPECT_TRUE(IsAuthEnabled());

  // Wait one hour.
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_TRUE(IsAuthEnabled());

  // Set new policy.
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddOverride(&policy_content,
                     usage_time_limit::TimeLimitOverride::Action::kLock,
                     task_runner_->Now());
  SetUsageTimeLimitPolicy(policy_content);

  EXPECT_FALSE(IsAuthEnabled());
}

// Tests an unlock override on a bedtime.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, UnlockBedtime) {
  LogInChildAndSetupClockWithTime("5 Jan 2018 22:00:00 BRT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"BRT");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 BRT");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Check that auth is disabled, since the bedtime has already started.
  EXPECT_FALSE(IsAuthEnabled());

  // Create unlock override and update the policy.
  utils::AddOverride(&policy_content,
                     usage_time_limit::TimeLimitOverride::Action::kUnlock,
                     task_runner_->Now());
  SetUsageTimeLimitPolicy(policy_content);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is still enabled.
  task_runner_->FastForwardBy(base::Hours(8));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 9 PM and check that auth is disabled because bedtime started.
  task_runner_->FastForwardBy(base::Hours(15));
  EXPECT_FALSE(IsAuthEnabled());
}

// Tests an override with duration on a bedtime before it's locked.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, OverrideBedtimeWithDuration) {
  LogInChildAndSetupClockWithTime("5 Jan 2018 20:45:00 PST");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"PST");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Check that auth is enable, since the bedtime hasn't started.
  EXPECT_TRUE(IsAuthEnabled());

  // Create unlock override with a duration of 2 hours and update the policy.
  utils::AddOverrideWithDuration(
      &policy_content, usage_time_limit::TimeLimitOverride::Action::kUnlock,
      task_runner_->Now(), base::Hours(2));
  SetUsageTimeLimitPolicy(policy_content);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 10:15 PM and check that auth is still enabled.
  task_runner_->FastForwardBy(base::Minutes(90));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 10:45 PM and check that auth is disabled because the duration is
  // over.
  task_runner_->FastForwardBy(base::Minutes(30));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 11 PM and check that auth is still disabled.
  task_runner_->FastForwardBy(base::Minutes(15));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is still disabled.
  task_runner_->FastForwardBy(base::Hours(7));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 7 AM and check that auth is enable because bedtime is finished.
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 9 PM and check that auth is disabled because bedtime started.
  task_runner_->FastForwardBy(base::Hours(14));
  EXPECT_FALSE(IsAuthEnabled());
}

// Tests an override with duration on a daily limit before it's locked.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest,
                       OverrideDailyLimitWithDuration) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 BRT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"BRT");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 BRT");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday, base::Hours(2),
                           last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Check that auth is enabled at 10 AM with 0 usage time.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12 PM with 1:50 hours of usage time.
  MockChildScreenTime(base::Minutes(110));
  task_runner_->FastForwardBy(base::Hours(2));
  EXPECT_TRUE(IsAuthEnabled());

  // Create unlock override with a duration of 1 hour and update the policy.
  utils::AddOverrideWithDuration(
      &policy_content, usage_time_limit::TimeLimitOverride::Action::kUnlock,
      task_runner_->Now(), base::Hours(1));
  SetUsageTimeLimitPolicy(policy_content);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12:30 PM with 2:20 hours of usage time and check that auth is
  // still enabled.
  MockChildScreenTime(base::Minutes(140));
  task_runner_->FastForwardBy(base::Minutes(30));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 1 PM and check that auth is disabled because the duration is
  // over.
  task_runner_->FastForwardBy(base::Minutes(30));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 5 AM and check that auth is still disabled.
  task_runner_->FastForwardBy(base::Hours(16));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is enabled.
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_TRUE(IsAuthEnabled());
}

// Tests an unlock override with duration on a bedtime.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, UnlockBedtimeWithDuration) {
  LogInChildAndSetupClockWithTime("5 Jan 2018 22:00:00 GMT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 GMT");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Check that auth is disabled, since the bedtime has already started.
  EXPECT_FALSE(IsAuthEnabled());

  // Create unlock override with a duration of 2 hours and update the policy.
  utils::AddOverrideWithDuration(
      &policy_content, usage_time_limit::TimeLimitOverride::Action::kUnlock,
      task_runner_->Now(), base::Hours(2));
  SetUsageTimeLimitPolicy(policy_content);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 11:30 PM and check that auth is still enabled.
  task_runner_->FastForwardBy(base::Minutes(90));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12 AM and check that auth is disabled because the duration is
  // over.
  task_runner_->FastForwardBy(base::Minutes(30));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is still disabled because bedtime ends
  // at 7 AM.
  task_runner_->FastForwardBy(base::Hours(6));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 7 AM and check that auth is enable because bedtime is finished.
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 9 PM and check that auth is disabled because bedtime started.
  task_runner_->FastForwardBy(base::Hours(14));
  EXPECT_FALSE(IsAuthEnabled());
}

// Tests an unlock override with duration on a daily limit.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, UnlockDailyLimitWithDuration) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 PST");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"PST");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday, base::Hours(2),
                           last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Check that auth is enabled at 10 AM with 0 usage time.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12 PM with 2 hours of usage time and check if auth is disabled.
  MockChildScreenTime(base::Hours(2));
  task_runner_->FastForwardBy(base::Hours(2));
  EXPECT_FALSE(IsAuthEnabled());

  // Create unlock override with a duration of 1 hour and update the policy.
  utils::AddOverrideWithDuration(
      &policy_content, usage_time_limit::TimeLimitOverride::Action::kUnlock,
      task_runner_->Now(), base::Hours(1));
  SetUsageTimeLimitPolicy(policy_content);

  // Check that the unlock worked and auth is enabled.
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 12:30 PM with 2:30 hours of usage time and check that auth is
  // still enabled.
  MockChildScreenTime(base::Minutes(150));
  task_runner_->FastForwardBy(base::Minutes(30));
  EXPECT_TRUE(IsAuthEnabled());

  // Forward to 1 PM and check that auth is disabled because the duration is
  // over.
  task_runner_->FastForwardBy(base::Minutes(30));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 5 AM and check that auth is still disabled.
  task_runner_->FastForwardBy(base::Hours(16));
  EXPECT_FALSE(IsAuthEnabled());

  // Forward to 6 AM and check that auth is enabled.
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_TRUE(IsAuthEnabled());
}

// Tests the default time window limit.
// TODO(crbug.com/1358216): Flaky on Linux
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_DefaultBedtime DISABLED_DefaultBedtime
#else
#define MAYBE_DefaultBedtime DefaultBedtime
#endif
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, MAYBE_DefaultBedtime) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 GMT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 GMT");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kMonday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kTuesday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kWednesday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kThursday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kFriday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSaturday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  utils::AddTimeWindowLimit(&policy_content, utils::kSunday,
                            utils::CreateTime(21, 0), utils::CreateTime(7, 0),
                            last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Iterate over a week checking that the device is locked properly everyday.
  for (int i = 0; i < 7; i++) {
    // Verify that auth is enabled at 10 AM.
    EXPECT_TRUE(IsAuthEnabled());

    // Verify that auth is enabled at 8 PM.
    task_runner_->FastForwardBy(base::Hours(10));
    EXPECT_TRUE(IsAuthEnabled());

    // Verify that the auth was disabled at 9 PM (start of bedtime).
    task_runner_->FastForwardBy(base::Hours(1));
    EXPECT_FALSE(IsAuthEnabled());

    // Forward to 7 AM and check that auth was re-enabled (end of bedtime).
    task_runner_->FastForwardBy(base::Hours(10));
    EXPECT_TRUE(IsAuthEnabled());

    // Forward to 10 AM.
    task_runner_->FastForwardBy(base::Hours(3));
  }
}

// Tests the default time window limit.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, DefaultDailyLimit) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 GMT");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 GMT");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday, base::Hours(3),
                           last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kTuesday, base::Hours(3),
                           last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kWednesday, base::Hours(3),
                           last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kThursday, base::Hours(3),
                           last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kFriday, base::Hours(3),
                           last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kSaturday, base::Hours(3),
                           last_updated);
  utils::AddTimeUsageLimit(&policy_content, utils::kSunday, base::Hours(3),
                           last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Iterate over a week checking that the device is locked properly
  // every day.
  for (int i = 0; i < 7; i++) {
    // Check that auth is enabled at 10 AM with 0 usage time.
    EXPECT_TRUE(IsAuthEnabled());

    // Check that auth is enabled after forwarding to 1 PM and using the device
    // for 2 hours.
    MockChildScreenTime(base::Hours(2));
    task_runner_->FastForwardBy(base::Hours(3));
    EXPECT_TRUE(IsAuthEnabled());

    // Check that auth is enabled after forwarding to 2 PM with no extra usage.
    task_runner_->FastForwardBy(base::Hours(1));
    EXPECT_TRUE(IsAuthEnabled());

    // Check that auth is disabled after forwarding to 3 PM and using the device
    // for 3 hours.
    MockChildScreenTime(base::Hours(3));
    task_runner_->FastForwardBy(base::Hours(1));
    EXPECT_FALSE(IsAuthEnabled());

    // Forward to 6 AM, reset the usage time and check that auth was re-enabled.
    MockChildScreenTime(base::Hours(0));
    task_runner_->FastForwardBy(base::Hours(15));
    EXPECT_TRUE(IsAuthEnabled());

    // Forward to 10 AM.
    task_runner_->FastForwardBy(base::Hours(4));
  }
}

// Tests that the bedtime locks an active session when it is reached.
// TODO(crbug.com/334319436): Flaky test.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest,
                       DISABLED_ActiveSessionBedtime) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 PST");

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"PST");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kMonday,
                            utils::CreateTime(23, 0), utils::CreateTime(8, 0),
                            last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Verify that device is unlocked at 10 AM.
  EXPECT_FALSE(IsLocked());

  // Verify that device is still unlocked at 10 PM.
  task_runner_->FastForwardBy(base::Hours(12));
  EXPECT_FALSE(IsLocked());

  // Verify that device is locked at 11 PM (start of bedtime).
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_TRUE(IsLocked());

  // Forward to 8 AM and check that auth was re-enabled (end of bedtime).
  task_runner_->FastForwardBy(base::Hours(9));
  EXPECT_TRUE(IsAuthEnabled());
}

// Tests that the daily limit locks the device when it is reached.
// TODO(crbug.com/334304756): Flaky on CrOS.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest,
                       DISABLED_ActiveSessionDailyLimit) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 PST");

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"PST");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday, base::Hours(1),
                           last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Verify that device is unlocked at 10 AM.
  EXPECT_FALSE(IsLocked());

  // Forward 1 hour to 11 AM and add 1 hour of usage and verify that device is
  // locked (start of daily limit).
  MockChildScreenTime(base::Hours(1));
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_TRUE(IsLocked());

  // Forward to 6 AM, reset the usage time and check that auth was re-enabled.
  MockChildScreenTime(base::Hours(0));
  task_runner_->FastForwardBy(base::Hours(19));
  EXPECT_TRUE(IsAuthEnabled());
}

// Tests bedtime during timezone changes.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, BedtimeOnTimezoneChange) {
  LogInChildAndSetupClockWithTime("3 Jan 2018 10:00:00 GMT-0600");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT-0600");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("3 Jan 2018 0:00 GMT-0600");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kWednesday,
                            utils::CreateTime(19, 0), utils::CreateTime(7, 0),
                            last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Verify that auth is enabled at 10 AM.
  EXPECT_TRUE(IsAuthEnabled());

  // Verify that auth is enabled at 6 PM.
  task_runner_->FastForwardBy(base::Hours(8));
  EXPECT_TRUE(IsAuthEnabled());

  // Verify that the auth is disabled at 7 PM (start of bedtime).
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(IsAuthEnabled());

  // Change timezone, so that local time goes back to 6 PM and check that auth
  // is enabled since bedtime has not started yet.
  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT-0700");
  EXPECT_TRUE(IsAuthEnabled());

  // Verify that auth is disabled at 7 PM (start of bedtime).
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(IsAuthEnabled());

  // Change timezone, so that local time goes forward to 7 AM and check that
  // auth is enabled since bedtime has ended in the new local time.
  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT+0500");
  EXPECT_TRUE(IsAuthEnabled());
}

IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, BedtimeLockScreen24HourClock) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 22:00:00 GMT");

  // Set preference of using 24 hour clock to be true.
  child_profile_->GetPrefs()->SetBoolean(prefs::kUse24HourClock, true);

  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 GMT");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kMonday,
                            utils::CreateTime(21, 0), utils::CreateTime(17, 0),
                            last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Check that auth is disabled, since the bedtime has already started.
  EXPECT_FALSE(IsAuthEnabled());

  EXPECT_EQ(u"Come back at 17:00.",
            LoginScreenTestApi::GetDisabledAuthMessage(GetAccountId()));
}

// Tests bedtime during timezone changes that make the clock go back in time.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest,
                       BedtimeOnEastToWestTimezoneChanges) {
  LogInChildAndSetupClockWithTime("3 Jan 2018 8:00:00 GMT+1300");
  ScreenLockerTester().Lock();

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT+1300");

  // Set new policy.
  base::Time last_updated = utils::TimeFromString("3 Jan 2018 0:00 GMT+1300");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeWindowLimit(&policy_content, utils::kTuesday,
                            utils::CreateTime(20, 0), utils::CreateTime(7, 0),
                            last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  // Verify that auth is disabled at 8 AM.
  EXPECT_TRUE(IsAuthEnabled());

  // Change timezone so that local time goes back to 6 AM and check that auth is
  // disable, since the tuesday's bedtime is not over yet.
  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT+1100");
  EXPECT_FALSE(IsAuthEnabled());

  // Change timezone so that local time goes back to 7 PM on Tuesday and check
  // that auth is enabled, because the bedtime has not started yet in the
  // new local time.
  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT");
  EXPECT_TRUE(IsAuthEnabled());

  // Verify that auth is disabled at 8 PM (start of bedtime).
  task_runner_->FastForwardBy(base::Hours(1));
  EXPECT_FALSE(IsAuthEnabled());
}

// Tests if call the observers for usage time limit warning.
// TODO(crbug.com/334160972): Flaky on CrOS.
IN_PROC_BROWSER_TEST_F(ScreenTimeControllerTest, DISABLED_CallObservers) {
  LogInChildAndSetupClockWithTime("1 Jan 2018 10:00:00 PST");

  system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"PST");

  // Set new policy with 3 hours of time usage limit.
  base::Time last_updated = utils::TimeFromString("1 Jan 2018 0:00 PST");
  base::Value::Dict policy_content =
      utils::CreateTimeLimitPolicy(utils::CreateTime(6, 0));
  utils::AddTimeUsageLimit(&policy_content, utils::kMonday, base::Hours(3),
                           last_updated);
  SetUsageTimeLimitPolicy(policy_content);

  TestScreenTimeControllerObserver observer;
  ScreenTimeControllerFactory::GetForBrowserContext(child_profile_)
      ->AddObserver(&observer);

  base::TimeDelta current_screen_time;
  base::TimeDelta last_screen_time;

  // Check that observer was not called at 10 AM.
  EXPECT_EQ(0, observer.usage_time_limit_warnings());

  // Check that observer was not called after child used device for 2 hours and
  // forward to 12 AM.
  last_screen_time = base::TimeDelta();
  current_screen_time = base::Hours(2);
  MockChildScreenTime(current_screen_time);
  task_runner_->FastForwardBy(current_screen_time - last_screen_time);
  EXPECT_EQ(0, observer.usage_time_limit_warnings());

  // Check that observer was not called after using the device for
  // 3 hours - |kUsageTimeLimitWarningTime| - 1 second. Forward to
  // 1 PM - |kUsageTimeLimitWarningTime| - 1 second.
  last_screen_time = current_screen_time;
  current_screen_time =
      base::Hours(3) - kUsageTimeLimitWarningTime - base::Seconds(1);
  MockChildScreenTime(current_screen_time);
  task_runner_->FastForwardBy(current_screen_time - last_screen_time);
  EXPECT_EQ(0, observer.usage_time_limit_warnings());

  // Check that observer was called after using the device for
  // 3 hours - |kUsageTimeLimitWarningTime| + 1 second. Forward to
  // 1 PM - |kUsageTimeLimitWarningTime| + 1 second.
  last_screen_time = current_screen_time;
  current_screen_time =
      base::Hours(3) - kUsageTimeLimitWarningTime + base::Seconds(1);
  MockChildScreenTime(current_screen_time);
  task_runner_->FastForwardBy(current_screen_time - last_screen_time);
  EXPECT_EQ(1, observer.usage_time_limit_warnings());

  // Check that observer was not called after using the device for 3 hours.
  // Forward to 1 PM.
  last_screen_time = current_screen_time;
  current_screen_time = base::Hours(3);
  MockChildScreenTime(current_screen_time);
  task_runner_->FastForwardBy(current_screen_time - last_screen_time);
  EXPECT_EQ(1, observer.usage_time_limit_warnings());

  // Forward to 6 AM, reset the usage time.
  MockChildScreenTime(base::Hours(0));
  task_runner_->FastForwardBy(base::Hours(17));

  // Forward to 10 AM.
  task_runner_->FastForwardBy(base::Hours(4));
  EXPECT_EQ(1, observer.usage_time_limit_warnings());

  ScreenTimeControllerFactory::GetForBrowserContext(child_profile_)
      ->RemoveObserver(&observer);
}

}  // namespace ash
