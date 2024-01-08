// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_reboot_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor_impl.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_reboot_notifications_scheduler.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/scheduled_task_test_util.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
constexpr char kRebootTaskTimeFieldName[] = "reboot_time";
constexpr base::TimeDelta kExternalRebootDelay = base::Seconds(100);
constexpr char kESTTimeZoneID[] = "America/New_York";
constexpr char kTestName[] = "test@test";
constexpr char kKioskName[] = "test@kiosk-apps.device-local.localhost";
}  // namespace

using ::testing::_;
#define EXPECT_ERROR_LOG(matcher)                                    \
  if (DLOG_IS_ON(ERROR)) {                                           \
    EXPECT_CALL(log_, Log(logging::LOGGING_ERROR, _, _, _, matcher)) \
        .WillOnce(testing::Return(true)); /* suppress logging */     \
  }

class DeviceScheduledRebootHandlerForTest
    : public DeviceScheduledRebootHandler {
 public:
  using DeviceScheduledRebootHandler::GetBootTimeCallback;

  template <class... Args>
  explicit DeviceScheduledRebootHandlerForTest(Args... args)
      : DeviceScheduledRebootHandler(std::forward<Args>(args)...) {}

  DeviceScheduledRebootHandlerForTest(
      const DeviceScheduledRebootHandlerForTest&) = delete;
  DeviceScheduledRebootHandlerForTest& operator=(
      const DeviceScheduledRebootHandlerForTest&) = delete;

  ~DeviceScheduledRebootHandlerForTest() override {
    TestingBrowserProcess::GetGlobal()->ShutdownBrowserPolicyConnector();
  }

  int GetRebootTimerExpirations() const { return reboot_timer_expirations_; }
  int GetPolicyChangesProcessedCount() const {
    return policy_changes_processed_;
  }

 private:
  void OnRebootTimerExpired() override {
    ++reboot_timer_expirations_;
    DeviceScheduledRebootHandler::OnRebootTimerExpired();
  }

  void OnScheduledRebootDataChanged() override {
    ++policy_changes_processed_;
    DeviceScheduledRebootHandler::OnScheduledRebootDataChanged();
  }

  // Number of calls to |OnRebootTimerExpired|.
  int reboot_timer_expirations_ = 0;

  // Number of calls to |OnScheduledRebootDataChanged|.
  int policy_changes_processed_ = 0;
};

class DeviceScheduledRebootHandlerTest : public testing::Test {
 public:
  DeviceScheduledRebootHandlerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()),
        prefs_(std::make_unique<TestingPrefServiceSimple>()),
        notifications_scheduler_(task_environment_.GetMockClock(),
                                 task_environment_.GetMockTickClock(),
                                 prefs_.get()),
        start_time_(task_environment_.GetMockClock()->Now()) {
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::BindRepeating(&device::TestWakeLockProvider::BindReceiver,
                            base::Unretained(&wake_lock_provider_)));
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());

    auto task_executor = std::make_unique<FakeScheduledTaskExecutor>(
        task_environment_.GetMockClock());
    scheduled_task_executor_ = task_executor.get();
    RebootNotificationsScheduler::RegisterProfilePrefs(prefs_->registry());
    device_scheduled_reboot_handler_ =
        std::make_unique<DeviceScheduledRebootHandlerForTest>(
            ash::CrosSettings::Get(), std::move(task_executor),
            &notifications_scheduler_,
            /*get_boot_time_callback=*/base::BindLambdaForTesting([this]() {
              return start_time_;
            }));
    // Set 0 delay for tests.
    device_scheduled_reboot_handler_->SetRebootDelayForTest(base::TimeDelta());
  }

  ~DeviceScheduledRebootHandlerTest() override {
    device_scheduled_reboot_handler_.reset();
    chromeos::PowerManagerClient::Shutdown();
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::NullCallback());
  }

 protected:
  bool CheckStats(int expected_scheduled_reboots,
                  int expected_reboot_requests) {
    if (device_scheduled_reboot_handler_->GetRebootTimerExpirations() !=
        expected_scheduled_reboots) {
      LOG(ERROR)
          << "Current reboot timer expirations: "
          << device_scheduled_reboot_handler_->GetRebootTimerExpirations()
          << " Expected reboot timer expirations: "
          << expected_scheduled_reboots;
      return false;
    }

    if (chromeos::FakePowerManagerClient::Get()->num_request_restart_calls() !=
        expected_reboot_requests) {
      LOG(ERROR) << "Current reboot requests: "
                 << chromeos::FakePowerManagerClient::Get()
                        ->num_request_restart_calls()
                 << " Expected reboot requests: " << expected_reboot_requests;
      return false;
    }

    return true;
  }

  bool CheckNotificationStats(int notifications_shown, int dialogs_shown) {
    if (notifications_scheduler_.GetShowNotificationCalls() !=
        notifications_shown) {
      LOG(ERROR) << "Current notifications shown count: "
                 << notifications_scheduler_.GetShowNotificationCalls()
                 << " Expected notifications shown count: "
                 << notifications_shown;
      return false;
    }

    if (notifications_scheduler_.GetShowDialogCalls() != dialogs_shown) {
      LOG(ERROR) << "Current dialogs shown count: "
                 << notifications_scheduler_.GetShowDialogCalls()
                 << " Expected dialogs shown count: " << dialogs_shown;
      return false;
    }

    return true;
  }

  const base::TimeDelta GetRebootDelay() const {
    return (scheduled_task_executor_->GetScheduledTaskTime() -
            task_environment_.GetMockClock()->Now());
  }

  void InitWithFeatureFlag(bool enable_force_scheduled_reboots) {
    if (enable_force_scheduled_reboots) {
      scoped_feature_list_.InitWithFeatures(
          /* enabled_features */ {ash::features::kDeviceForceScheduledReboot},
          /* disabled_features */ {});
      return;
    }
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {},
        /* disabled_features */ {ash::features::kDeviceForceScheduledReboot});
  }

  ash::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  base::test::TaskEnvironment task_environment_;
  user_manager::ScopedUserManager user_manager_enabler_;
  raw_ptr<FakeScheduledTaskExecutor, DanglingUntriaged>
      scheduled_task_executor_;
  std::unique_ptr<DeviceScheduledRebootHandlerForTest>
      device_scheduled_reboot_handler_;
  ash::ScopedTestingCrosSettings cros_settings_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  device::TestWakeLockProvider wake_lock_provider_;
  FakeRebootNotificationsScheduler notifications_scheduler_;
  base::test::ScopedFeatureList scoped_feature_list_;
  const base::Time start_time_;
};

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfDailyRebootIsScheduledForKiosk) {
  InitWithFeatureFlag(false /* enable_force_scheduled_reboots */);
  auto* user_manager = GetFakeUserManager();
  auto* user =
      user_manager->AddKioskAppUser(AccountId::FromUserEmail(kKioskName));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);

  // Calculate time from one hour from now and set the reboot policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then check if an reboot is not scheduled.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot is scheduled.
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // After the reboot, the current handler is destroyed and the new one is
  // created which will schedule reboot for the next day. Check that current
  // handler is not scheduling any more reboots.
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfDailyRebootIsScheduledForNonKiosk) {
  InitWithFeatureFlag(false /* enable_force_scheduled_reboots */);
  auto* user_manager = GetFakeUserManager();
  auto* user = user_manager->AddUser(AccountId::FromUserEmail(kTestName));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);

  // Calculate time from one hour from now and set the reboot policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then check if an reboot is not scheduled.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot is scheduled, but not executed since we are not in the kiosk mode.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the next day and then check if the reboot is scheduled
  // again.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Switch to the kiosk mode, fast forward to the next day and check that the
  // reboot is scheduled and executed.
  auto* kiosk_user =
      user_manager->AddKioskAppUser(AccountId::FromUserEmail(kKioskName));
  user_manager->UserLoggedIn(kiosk_user->GetAccountId(),
                             kiosk_user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  user_manager->SwitchActiveUser(kiosk_user->GetAccountId());
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfWeeklyUpdateCheckIsScheduledForKiosk) {
  InitWithFeatureFlag(false /* enable_force_scheduled_reboots */);
  auto* user_manager = GetFakeUserManager();
  auto* user = user_manager->AddUser(AccountId::FromUserEmail(kTestName));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  // Set the first reboot to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then weekly after.
  base::TimeDelta delay_from_now = base::Hours(49);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kWeekly, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot setting, fast forward to right before the
  // expected reboot and then check if a reboot is not scheduled.
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the reboot
  // is scheduled, but not executed, since we are not in the kiosk mode.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Switch to the kiosk mode, fast forward to the next week and check that the
  // reboot is scheduled and executed.
  auto* kiosk_user =
      user_manager->AddKioskAppUser(AccountId::FromUserEmail(kKioskName));
  user_manager->UserLoggedIn(kiosk_user->GetAccountId(),
                             kiosk_user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  user_manager->SwitchActiveUser(kiosk_user->GetAccountId());
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(base::Days(7));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfMonthlyRebootIsScheduledForKiosk) {
  InitWithFeatureFlag(false /* enable_force_scheduled_reboots */);
  auto* user_manager = GetFakeUserManager();
  auto* user = user_manager->AddUser(AccountId::FromUserEmail(kTestName));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);

  // Set the first reboot to happen 1 hour from now.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kMonthly, kRebootTaskTimeFieldName);
  auto scheduled_reboot_data = scheduled_task_util::ParseScheduledTask(
      policy_and_next_reboot_time.first, kRebootTaskTimeFieldName);
  ASSERT_TRUE(scheduled_reboot_data);
  ASSERT_TRUE(scheduled_reboot_data->day_of_month);
  auto first_reboot_icu_time = std::move(policy_and_next_reboot_time.second);

  // Set a new scheduled reboot setting, fast forward to right before the
  // expected reboot and then check if a reboot is not scheduled.
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the reboot
  // is scheduled, but not executed since we are not in the kiosk mode.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // The next reboot should happen at the same day of month next month. Switch
  // to the kiosk mode and verify the reboot is executed.
  auto* kiosk_user =
      user_manager->AddKioskAppUser(AccountId::FromUserEmail(kKioskName));
  user_manager->UserLoggedIn(kiosk_user->GetAccountId(),
                             kiosk_user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  user_manager->SwitchActiveUser(kiosk_user->GetAccountId());
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  EXPECT_TRUE(scheduled_task_test_util::AdvanceTimeAndSetDayOfMonth(
      scheduled_reboot_data->day_of_month.value(),
      first_reboot_icu_time.get()));
  base::Time second_reboot_time =
      scheduled_task_test_util::IcuToBaseTime(*first_reboot_icu_time);
  std::optional<base::TimeDelta> second_reboot_delay =
      second_reboot_time - scheduled_task_executor_->GetCurrentTime();
  ASSERT_TRUE(second_reboot_delay.has_value());
  task_environment_.FastForwardBy(second_reboot_delay.value());
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfDailyRebootIsScheduledWithExternalDelay) {
  // Login user and disable kDeviceScheduledReboot flag. The reboot should not
  // occur.
  InitWithFeatureFlag(false /* enable_force_scheduled_reboots */);
  auto* user_manager = GetFakeUserManager();
  auto* user = user_manager->AddUser(AccountId::FromUserEmail(kTestName));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  device_scheduled_reboot_handler_->SetRebootDelayForTest(kExternalRebootDelay);

  // Calculate time from one hour from now and set the reboot policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then check if an reboot is not scheduled.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;

  // Verify that final delay is equal to delay_from_now + external_delay.
  base::TimeDelta final_delay = GetRebootDelay();
  EXPECT_EQ(final_delay, delay_from_now + kExternalRebootDelay);
  task_environment_.FastForwardBy(final_delay - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot is scheduled, but not executed since we are not in the kiosk mode.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the next day and then check if the reboot is scheduled
  // again.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfDailyRebootIsScheduledForLoginScreen) {
  InitWithFeatureFlag(true /* enable_force_scheduled_reboots */);

  // Set device uptime to 10 minutes and schedule reboot in 30 minutes. Apply
  // grace time - reboot should not occur.
  task_environment_.FastForwardBy(base::Minutes(10));
  base::TimeDelta delay_from_now = base::Minutes(30);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  // Fast forward without an actual time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then verify reboot timer has not yet expired.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot timer has expired and the reboot is not executed.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the next day at the same time and verify reboot is
  // executed.
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // After the reboot, the current handler is destroyed and the new one is
  // created which will schedule reboot for the next day. Check that current
  // handler is not scheduling any more reboots.
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest,
       VerifyLoginScreenRebootIsGuardedByFeatureFlag) {
  // Disable the feature. Reboot should not occur.
  InitWithFeatureFlag(false /* enable_force_scheduled_reboots */);

  // Schedule reboot in 30 minutes. Reboot should not occur because the flag is
  // disabled.
  base::TimeDelta delay_from_now = base::Minutes(30);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then verify reboot timer has not yet expired.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot timer has expired and the reboot is not executed.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the next day and then check if the reboot is scheduled
  // again, but not executed.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest, EnableForceRebootFeatureInKiosk) {
  InitWithFeatureFlag(true /* enable_force_scheduled_reboots */);

  // Set device uptime to 10 minutes and enable kiosk mode. We don't apply grace
  // period to kiosks, so reboot should occur.
  task_environment_.FastForwardBy(base::Minutes(10));
  auto* user_manager = GetFakeUserManager();
  auto* user =
      user_manager->AddKioskAppUser(AccountId::FromUserEmail(kKioskName));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);

  // Calculate time 30 minutes from now and set the reboot policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::Minutes(30);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and and then verify reboot timer has not yet expired.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot timer has expired and the reboot is executed.
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest,
       EnableForceRebootFeatureNonKioskSession) {
  InitWithFeatureFlag(true /* enable_force_scheduled_reboots */);
  auto* user_manager = GetFakeUserManager();
  auto* user = user_manager->AddUser(AccountId::FromUserEmail(kTestName));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  EXPECT_FALSE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));

  // Set device uptime to 10 minutes and schedule reboot in 30 minutes. Apply
  // grace time - reboot should not occur.
  task_environment_.FastForwardBy(base::Minutes(10));
  base::TimeDelta delay_from_now = base::Minutes(30);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then check if the reboot timer has not yet expired.
  // Verify notifications are not shown.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  int expected_notification_count = 0;
  int expected_dialog_count = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
  EXPECT_TRUE(CheckNotificationStats(expected_notification_count,
                                     expected_dialog_count));

  // Fast forward to the expected reboot time and then check if the
  // reboot timer has expred but the reboot is not executed.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the next day at the same time and verify reboot is executed
  // and notifications are shown.
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  expected_notification_count += 1;
  expected_dialog_count += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckNotificationStats(expected_notification_count,
                                     expected_dialog_count));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Verify post reboot notification flag is set.
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
}

TEST_F(DeviceScheduledRebootHandlerTest, SimulateNotificationButtonClick) {
  InitWithFeatureFlag(true /* enable_force_scheduled_reboots */);
  auto* user_manager = GetFakeUserManager();
  auto* user = user_manager->AddUser(AccountId::FromUserEmail(kTestName));
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);

  /// Schedule reboot to happen in 3 hours.
  base::TimeDelta delay_from_now = base::Hours(3);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to 5 minutes before the
  // expected reboot and then check that notification are shown, but the reboot
  // timer has not yet expired.
  const base::TimeDelta small_delay = base::Minutes(5);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  int expected_notification_count = 1;
  int expected_dialog_count = 1;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
  EXPECT_TRUE(CheckNotificationStats(expected_notification_count,
                                     expected_dialog_count));

  // Simulate reboot button click on the notification. This should execute the
  // reboot.
  notifications_scheduler_.SimulateRebootButtonClick();
  expected_reboot_requests += 1;
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Verify post reboot notification flag is not set.
  EXPECT_FALSE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
}

class ScheduledRebootTimerFailureTest : public testing::Test {
 public:
  ScheduledRebootTimerFailureTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        notifications_scheduler_(task_environment_.GetMockClock(),
                                 task_environment_.GetMockTickClock(),
                                 nullptr),
        start_time_(task_environment_.GetMockClock()->Now()) {
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::BindRepeating(&device::TestWakeLockProvider::BindReceiver,
                            base::Unretained(&wake_lock_provider_)));
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());
    auto task_executor =
        std::make_unique<ScheduledTaskExecutorImpl>("test_tag");
    scheduled_task_executor_ = task_executor.get();
    device_scheduled_reboot_handler_ =
        std::make_unique<DeviceScheduledRebootHandlerForTest>(
            ash::CrosSettings::Get(), std::move(task_executor),
            &notifications_scheduler_,
            /*get_boot_time_callback=*/base::BindLambdaForTesting([this]() {
              return start_time_;
            }));
  }

  ~ScheduledRebootTimerFailureTest() override {
    device_scheduled_reboot_handler_.reset();
    chromeos::PowerManagerClient::Shutdown();
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::NullCallback());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<ScheduledTaskExecutorImpl, DanglingUntriaged>
      scheduled_task_executor_;
  std::unique_ptr<DeviceScheduledRebootHandlerForTest>
      device_scheduled_reboot_handler_;
  ash::ScopedTestingCrosSettings cros_settings_;
  device::TestWakeLockProvider wake_lock_provider_;
  FakeRebootNotificationsScheduler notifications_scheduler_;
  base::test::MockLog log_;
  const base::Time start_time_;
};

TEST_F(ScheduledRebootTimerFailureTest, SimulateTimerStartFailure) {
  // Schedule reboot in 30 minutes.
  base::TimeDelta delay_from_now = base::Minutes(30);
  auto time_zone_ = base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8(kESTTimeZoneID)));
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      *time_zone_.get(), task_environment_.GetMockClock()->Now(),
      delay_from_now, ScheduledTaskExecutor::Frequency::kDaily,
      kRebootTaskTimeFieldName);

  // Simulate timer creation failure.
  auto scoped_timer_failure =
      chromeos::NativeTimer::ScopedFailureSimulatorForTesting();

  // Verify timer start failure once the policy is set. Notification scheduler
  // should close all pending notifications and reset state.
  EXPECT_ERROR_LOG(testing::HasSubstr("Failed to start reboot timer"));
  log_.StartCapturingLogs();
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));

  // Verify that the notifications were not shown.
  EXPECT_EQ(notifications_scheduler_.GetShowNotificationCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetShowDialogCalls(), 0);
  EXPECT_EQ(notifications_scheduler_.GetCloseNotificationCalls(), 0);
  // Verify that the state is reset.
  EXPECT_EQ(device_scheduled_reboot_handler_->GetScheduledRebootDataForTest(),
            std::nullopt);
  EXPECT_EQ(device_scheduled_reboot_handler_->IsRebootSkippedForTest(), false);
}

class ScheduledRebootDelayedServiceTest : public testing::Test {
 public:
  ScheduledRebootDelayedServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        notifications_scheduler_(task_environment_.GetMockClock(),
                                 task_environment_.GetMockTickClock(),
                                 nullptr),
        start_time_(task_environment_.GetMockClock()->Now()) {
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::BindRepeating(&device::TestWakeLockProvider::BindReceiver,
                            base::Unretained(&wake_lock_provider_)));
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->SetServiceAvailability(
        /*availability=*/std::nullopt);
    auto task_executor = std::make_unique<FakeScheduledTaskExecutor>(
        task_environment_.GetMockClock());
    scheduled_task_executor_ = task_executor.get();
    device_scheduled_reboot_handler_ =
        std::make_unique<DeviceScheduledRebootHandlerForTest>(
            ash::CrosSettings::Get(), std::move(task_executor),
            &notifications_scheduler_,
            /*get_boot_time_callback=*/base::BindLambdaForTesting([this]() {
              return start_time_;
            }));
  }

  ~ScheduledRebootDelayedServiceTest() override {
    device_scheduled_reboot_handler_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<FakeScheduledTaskExecutor, DanglingUntriaged>
      scheduled_task_executor_;
  std::unique_ptr<DeviceScheduledRebootHandlerForTest>
      device_scheduled_reboot_handler_;
  ash::ScopedTestingCrosSettings cros_settings_;
  FakeRebootNotificationsScheduler notifications_scheduler_;
  device::TestWakeLockProvider wake_lock_provider_;
  const base::Time start_time_;
};

TEST_F(ScheduledRebootDelayedServiceTest, SimulateServiceIsAvailableLaterTest) {
  int expected_policy_processed_count = 0;
  EXPECT_EQ(device_scheduled_reboot_handler_->GetPolicyChangesProcessedCount(),
            expected_policy_processed_count);
  chromeos::FakePowerManagerClient::Get()->SetServiceAvailability(
      /*availability=*/true);
  expected_policy_processed_count += 1;
  EXPECT_EQ(device_scheduled_reboot_handler_->GetPolicyChangesProcessedCount(),
            expected_policy_processed_count);
}

TEST_F(ScheduledRebootDelayedServiceTest,
       SimulateServiceStopsBeingAvailableTest) {
  // Schedule reboot in 30 minutes.
  base::TimeDelta delay_from_now = base::Minutes(30);
  auto time_zone_ = base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8(kESTTimeZoneID)));
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      *time_zone_.get(), task_environment_.GetMockClock()->Now(),
      delay_from_now, ScheduledTaskExecutor::Frequency::kDaily,
      kRebootTaskTimeFieldName);
  int expected_policy_processed_count =
      device_scheduled_reboot_handler_->GetPolicyChangesProcessedCount();

  // Notify that the service is available and expect processing policy.
  chromeos::FakePowerManagerClient::Get()->SetServiceAvailability(
      /*availability=*/true);
  expected_policy_processed_count += 1;
  EXPECT_EQ(device_scheduled_reboot_handler_->GetPolicyChangesProcessedCount(),
            expected_policy_processed_count);

  // Notify that the service stopped being available and verify that the state
  // is reset and that the policy is not processed.
  chromeos::FakePowerManagerClient::Get()->SetServiceAvailability(
      /*availability=*/false);
  EXPECT_EQ(device_scheduled_reboot_handler_->GetScheduledRebootDataForTest(),
            std::nullopt);
  EXPECT_EQ(device_scheduled_reboot_handler_->IsRebootSkippedForTest(), false);
  EXPECT_EQ(device_scheduled_reboot_handler_->GetPolicyChangesProcessedCount(),
            expected_policy_processed_count);
}
}  // namespace policy
