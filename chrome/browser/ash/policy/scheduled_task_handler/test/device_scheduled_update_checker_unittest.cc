// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_update_checker.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/os_and_policies_update_checker.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/scheduled_task_test_util.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/policy/core/common/policy_service.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace policy {

namespace {

// Number of days in a week.
constexpr int kDaysInAWeek = 7;

// Time zones that will be used in tests.
constexpr char kISTTimeZoneID[] = "Asia/Kolkata";
constexpr char kPSTTimeZoneID[] = "America/Los_Angeles";

constexpr char kTaskTimeFieldName[] = "update_check_time";

std::string CreateConnectedWifiConfigurationJsonString(
    const std::string& guid) {
  return base::StringPrintf(R"({ "GUID": "%s", "Type": "%s", "State": "%s" })",
                            guid.data(), shill::kTypeWifi, shill::kStateOnline);
}

}  // namespace

class DeviceScheduledUpdateCheckerForTest
    : public DeviceScheduledUpdateChecker {
 public:
  DeviceScheduledUpdateCheckerForTest(
      ash::CrosSettings* cros_settings,
      ash::NetworkStateHandler* network_state_handler,
      std::unique_ptr<ScheduledTaskExecutor> task_executor)
      : DeviceScheduledUpdateChecker(cros_settings,
                                     network_state_handler,
                                     std::move(task_executor)) {}

  DeviceScheduledUpdateCheckerForTest(
      const DeviceScheduledUpdateCheckerForTest&) = delete;
  DeviceScheduledUpdateCheckerForTest& operator=(
      const DeviceScheduledUpdateCheckerForTest&) = delete;

  ~DeviceScheduledUpdateCheckerForTest() override {
    TestingBrowserProcess::GetGlobal()->ShutdownBrowserPolicyConnector();
  }

  int GetUpdateCheckTimerExpirations() const {
    return update_check_timer_expirations_;
  }

  int GetUpdateCheckCompletions() const { return update_check_completions_; }

 private:
  void OnUpdateCheckTimerExpired() override {
    ++update_check_timer_expirations_;
    DeviceScheduledUpdateChecker::OnUpdateCheckTimerExpired();
  }

  void OnUpdateCheckCompletion(ScopedWakeLock scoped_wake_lock,
                               bool result) override {
    if (result)
      ++update_check_completions_;
    DeviceScheduledUpdateChecker::OnUpdateCheckCompletion(
        std::move(scoped_wake_lock), result);
  }

  // Number of calls to |OnUpdateCheckTimerExpired|.
  int update_check_timer_expirations_ = 0;

  // Number of calls to |OnUpdateCheckCompletion| with |result| = true.
  int update_check_completions_ = 0;
};

class DeviceScheduledUpdateCheckerTest : public testing::Test {
 public:
  DeviceScheduledUpdateCheckerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::BindRepeating(&device::TestWakeLockProvider::BindReceiver,
                            base::Unretained(&wake_lock_provider_)));
    fake_update_engine_client_ =
        ash::UpdateEngineClient::InitializeFakeForTest();
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());

    network_state_test_helper_ = std::make_unique<ash::NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/true);

    auto task_executor = std::make_unique<FakeScheduledTaskExecutor>(
        task_environment_.GetMockClock());
    scheduled_task_executor_ = task_executor.get();
    device_scheduled_update_checker_ =
        std::make_unique<DeviceScheduledUpdateCheckerForTest>(
            ash::CrosSettings::Get(),
            network_state_test_helper_->network_state_handler(),
            std::move(task_executor));
  }

  DeviceScheduledUpdateCheckerTest(const DeviceScheduledUpdateCheckerTest&) =
      delete;
  DeviceScheduledUpdateCheckerTest& operator=(
      const DeviceScheduledUpdateCheckerTest&) = delete;

  ~DeviceScheduledUpdateCheckerTest() override {
    device_scheduled_update_checker_.reset();
    network_state_test_helper_.reset();
    chromeos::PowerManagerClient::Shutdown();
    ash::UpdateEngineClient::Shutdown();
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::NullCallback());
  }

 protected:
  // Notifies status update from |fake_update_engine_client_| and runs scheduled
  // tasks to ensure that the pending policy refresh completes.
  void NotifyUpdateCheckStatus(
      update_engine::Operation update_status_operation) {
    update_engine::StatusResult status;
    status.set_current_operation(update_status_operation);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
    task_environment_.RunUntilIdle();
  }

  // Returns true only iff all stats match in
  // |device_scheduled_update_checker_|.
  bool CheckStats(int expected_update_checks,
                  int expected_update_check_requests,
                  int expected_update_check_completions) {
    if (device_scheduled_update_checker_->GetUpdateCheckTimerExpirations() !=
        expected_update_checks) {
      LOG(ERROR)
          << "Current update check timer expirations: "
          << device_scheduled_update_checker_->GetUpdateCheckTimerExpirations()
          << " Expected update check timer expirations: "
          << expected_update_checks;
      return false;
    }

    if (fake_update_engine_client_->request_update_check_call_count() !=
        expected_update_check_requests) {
      LOG(ERROR)
          << "Current update check requests: "
          << fake_update_engine_client_->request_update_check_call_count()
          << " Expected update check requests: "
          << expected_update_check_requests;
      return false;
    }

    if (device_scheduled_update_checker_->GetUpdateCheckCompletions() !=
        expected_update_check_completions) {
      LOG(ERROR)
          << "Current update check completions: "
          << device_scheduled_update_checker_->GetUpdateCheckCompletions()
          << " Expected update check completions: "
          << expected_update_check_completions;
      return false;
    }

    return true;
  }

  // Sets a daily update check policy and returns true iff it's scheduled
  // correctly. |hours_from_now| must be > 0.
  bool CheckDailyUpdateCheck(int hours_from_now) {
    DCHECK_GT(hours_from_now, 0);
    // Calculate delay using |hours_from_now| and set the update check policy to
    // happen daily at that time.
    base::TimeDelta delay_from_now = base::Hours(hours_from_now);
    auto policy_and_next_update_check_time =
        scheduled_task_test_util::CreatePolicy(
            scheduled_task_executor_->GetTimeZone(),
            scheduled_task_executor_->GetCurrentTime(), delay_from_now,
            ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);

    // Set a new scheduled update setting, fast forward to right before the
    // expected update and then check if an update check is not scheduled.
    const base::TimeDelta small_delay = base::Milliseconds(1);
    cros_settings_.device_settings()->Set(
        ash::kDeviceScheduledUpdateCheck,
        std::move(policy_and_next_update_check_time.first));
    int expected_update_checks =
        device_scheduled_update_checker_->GetUpdateCheckTimerExpirations();
    int expected_update_check_requests =
        fake_update_engine_client_->request_update_check_call_count();
    int expected_update_check_completions =
        device_scheduled_update_checker_->GetUpdateCheckCompletions();
    task_environment_.FastForwardBy(delay_from_now - small_delay);
    if (!CheckStats(expected_update_checks, expected_update_check_requests,
                    expected_update_check_completions)) {
      return false;
    }

    // Fast forward to the expected update check time and then check if the
    // update check is scheduled.
    expected_update_checks += 1;
    expected_update_check_requests += 1;
    expected_update_check_completions += 1;
    task_environment_.FastForwardBy(small_delay);

    // Simulate update check succeeding.
    NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
    if (!CheckStats(expected_update_checks, expected_update_check_requests,
                    expected_update_check_completions)) {
      return false;
    }

    // An update check should happen every day since the policy is set to daily.
    const int days = 5;
    for (int i = 0; i < days; i++) {
      expected_update_checks += 1;
      expected_update_check_requests += 1;
      expected_update_check_completions += 1;
      task_environment_.FastForwardBy(base::Days(1));

      // Simulate update check succeeding.
      NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
      if (!CheckStats(expected_update_checks, expected_update_check_requests,
                      expected_update_check_completions)) {
        return false;
      }
    }

    return true;
  }

  // Checks if a time zone change to |tz_id| recalculates and sets the correct
  // update check timer. Returns false if |tz_id| is the same as the current
  // time zone or on a scheduling error.
  bool CheckRecalculationOnTimezoneChange(const std::string& new_tz_id) {
    base::Time cur_time = scheduled_task_executor_->GetCurrentTime();
    const icu::TimeZone& cur_tz = scheduled_task_executor_->GetTimeZone();
    auto new_tz = base::WrapUnique(
        icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(new_tz_id)));
    if (cur_tz == *new_tz) {
      ADD_FAILURE() << "New time zone same as current time zone: " << new_tz_id;
      return false;
    }

    base::TimeDelta delay_from_now = base::Hours(1);
    // If the timer is set to expire at 5PM in |cur_tz| then changing time zones
    // means that the new timer would expire at 5PM in |new_tz| as well. This
    // delay is the delay between the new time zone's timer expiration time and
    // |cur_time|.
    std::optional<base::TimeDelta> new_tz_timer_expiration_delay =
        scheduled_task_test_util::
            CalculateTimerExpirationDelayInDailyPolicyForTimeZone(
                cur_time, delay_from_now, cur_tz, *new_tz);
    EXPECT_TRUE(new_tz_timer_expiration_delay.has_value());

    // Set daily policy to start update check one hour from now.
    int expected_update_checks = 0;
    int expected_update_check_requests = 0;
    int expected_update_check_completions = 0;
    auto policy_and_next_update_check_time =
        scheduled_task_test_util::CreatePolicy(
            scheduled_task_executor_->GetTimeZone(),
            scheduled_task_executor_->GetCurrentTime(), delay_from_now,
            ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);
    cros_settings_.device_settings()->Set(
        ash::kDeviceScheduledUpdateCheck,
        std::move(policy_and_next_update_check_time.first));
    if (!CheckStats(expected_update_checks, expected_update_check_requests,
                    expected_update_check_completions)) {
      ADD_FAILURE() << "Incorrect stats after policy set";
      return false;
    }

    // Change the time zone. This should change the time at which the timer
    // should expire.
    scheduled_task_executor_->SetTimeZone(std::move(new_tz));
    device_scheduled_update_checker_->TimezoneChanged(
        scheduled_task_executor_->GetTimeZone());

    // Fast forward right before the new time zone's expected timer expiration
    // time and check if no new events happened.
    const base::TimeDelta small_delay = base::Milliseconds(1);
    task_environment_.FastForwardBy(new_tz_timer_expiration_delay.value() -
                                    small_delay);
    if (!CheckStats(expected_update_checks, expected_update_check_requests,
                    expected_update_check_completions)) {
      ADD_FAILURE()
          << "Incorrect stats just before the new time zone expiration";
      return false;
    }

    // Fast forward to the new time zone's expected timer expiration time and
    // check if the timer expiration and update check happens.
    expected_update_checks += 1;
    expected_update_check_requests += 1;
    expected_update_check_completions += 1;
    task_environment_.FastForwardBy(small_delay);
    // Simulate update check succeeding.
    NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
    if (!CheckStats(expected_update_checks, expected_update_check_requests,
                    expected_update_check_completions)) {
      ADD_FAILURE()
          << "Incorrect stats just after the expected new time zone expiration";
      return false;
    }

    return true;
  }

  base::test::TaskEnvironment task_environment_;
  // Owned by |device_scheduled_update_checker_|
  raw_ptr<FakeScheduledTaskExecutor, DanglingUntriaged>
      scheduled_task_executor_;
  std::unique_ptr<DeviceScheduledUpdateCheckerForTest>
      device_scheduled_update_checker_;
  ash::ScopedTestingCrosSettings cros_settings_;
  raw_ptr<ash::FakeUpdateEngineClient, DanglingUntriaged>
      fake_update_engine_client_;
  std::unique_ptr<ash::NetworkStateTestHelper> network_state_test_helper_;
  device::TestWakeLockProvider wake_lock_provider_;

 private:
  ash::ScopedStubInstallAttributes test_install_attributes_{
      ash::StubInstallAttributes::CreateCloudManaged("fake-domain", "fake-id")};
};

TEST_F(DeviceScheduledUpdateCheckerTest, CheckIfDailyUpdateCheckIsScheduled) {
  // Check if back to back policies succeed.
  for (int i = 1; i <= 10; i++)
    EXPECT_TRUE(CheckDailyUpdateCheck(i));
}

TEST_F(DeviceScheduledUpdateCheckerTest, CheckIfWeeklyUpdateCheckIsScheduled) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then weekly after.
  base::TimeDelta delay_from_now = base::Hours(49);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kWeekly, kTaskTimeFieldName);

  // Set a new scheduled update setting, fast forward to right before the
  // expected update and then check if an update check is not scheduled.
  int expected_update_checks = 0;
  int expected_update_check_requests = 0;
  int expected_update_check_completions = 0;
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward to the expected update check time and then check if the update
  // check is scheduled.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  task_environment_.FastForwardBy(small_delay);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // An update check should happen weekly since the policy is set to weekly.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  task_environment_.FastForwardBy(base::Days(7));
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

TEST_F(DeviceScheduledUpdateCheckerTest, CheckIfMonthlyUpdateCheckIsScheduled) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then monthly after.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kMonthly, kTaskTimeFieldName);
  auto scheduled_update_check_data = scheduled_task_util::ParseScheduledTask(
      policy_and_next_update_check_time.first, kTaskTimeFieldName);
  ASSERT_TRUE(scheduled_update_check_data);
  ASSERT_TRUE(scheduled_update_check_data->day_of_month);
  auto first_update_check_icu_time =
      std::move(policy_and_next_update_check_time.second);

  // Set a new scheduled update setting, fast forward to right before the
  // expected update and then check if an update check is not scheduled.
  int expected_update_checks = 0;
  int expected_update_check_requests = 0;
  int expected_update_check_completions = 0;
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward to the expected update check time and then check if the update
  // check is scheduled.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  task_environment_.FastForwardBy(small_delay);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // The next update check should happen at the same day of month next month.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  EXPECT_TRUE(scheduled_task_test_util::AdvanceTimeAndSetDayOfMonth(
      scheduled_update_check_data->day_of_month.value(),
      first_update_check_icu_time.get()));
  base::Time second_update_check_time =
      scheduled_task_test_util::IcuToBaseTime(*first_update_check_icu_time);
  std::optional<base::TimeDelta> second_update_check_delay =
      second_update_check_time - scheduled_task_executor_->GetCurrentTime();
  ASSERT_TRUE(second_update_check_delay.has_value());
  task_environment_.FastForwardBy(second_update_check_delay.value());
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

TEST_F(DeviceScheduledUpdateCheckerTest, CheckMonthlyRolloverLogic) {
  // The default time at the beginning is 31st December, 1969, 19:00:00.000
  // America/New_York. Move it to 31st January, 1970 to test the rollover logic.
  task_environment_.FastForwardBy(
      base::Days(scheduled_task_test_util::GetDaysInMonthInEpochYear(
          static_cast<UCalendarMonths>(UCAL_JANUARY))));

  // Set the first update check time to be at 31st January, 1970, 20:00:00.000
  // America/New_York.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kMonthly, kTaskTimeFieldName);
  auto scheduled_update_check_data = scheduled_task_util::ParseScheduledTask(
      policy_and_next_update_check_time.first, kTaskTimeFieldName);
  ASSERT_TRUE(scheduled_update_check_data);
  ASSERT_TRUE(scheduled_update_check_data->day_of_month);
  auto update_check_icu_time =
      std::move(policy_and_next_update_check_time.second);

  // Set a new scheduled update setting. Fast forward to the expected update
  // check time and then check if the update check is scheduled.
  int expected_update_checks = 1;
  int expected_update_check_requests = 1;
  int expected_update_check_completions = 1;
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  task_environment_.FastForwardBy(delay_from_now);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Check that an update check happens at the last day of every month.
  for (int month = UCAL_FEBRUARY; month <= UCAL_DECEMBER; month++) {
    EXPECT_TRUE(scheduled_task_test_util::AdvanceTimeAndSetDayOfMonth(
        scheduled_update_check_data->day_of_month.value(),
        update_check_icu_time.get()));
    base::Time expected_next_update_check_time =
        scheduled_task_test_util::IcuToBaseTime(*update_check_icu_time);
    std::optional<base::TimeDelta> expected_next_update_check_delay =
        expected_next_update_check_time -
        scheduled_task_executor_->GetCurrentTime();
    // This should be always set in a virtual time environment.
    ASSERT_TRUE(expected_next_update_check_delay.has_value());
    const base::TimeDelta small_delay = base::Milliseconds(1);
    task_environment_.FastForwardBy(expected_next_update_check_delay.value() -
                                    small_delay);
    EXPECT_TRUE(CheckStats(expected_update_checks,
                           expected_update_check_requests,
                           expected_update_check_completions));

    expected_update_checks += 1;
    expected_update_check_requests += 1;
    expected_update_check_completions += 1;
    task_environment_.FastForwardBy(small_delay);
    // Simulate update check succeeding.
    NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
    EXPECT_TRUE(CheckStats(expected_update_checks,
                           expected_update_check_requests,
                           expected_update_check_completions));
  }
}

// Checks if an update check timer can't be started, retries are scheduled to
// recover from transient errors.
TEST_F(DeviceScheduledUpdateCheckerTest, CheckRetryLogicEventualSuccess) {
  // This will simulate an error while calculating the next update check time
  // and will result in no update checks happening till its set.
  scheduled_task_executor_->SimulateCalculateNextScheduledTaskFailure(true);

  // Calculate time from one hour from now and set the update check policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);

  // Fast forward time by less than (max retries * retry period) and check that
  // no update has occurred due to failure being simulated.
  int expected_update_checks = 0;
  int expected_update_check_requests = 0;
  int expected_update_check_completions = 0;
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  const base::TimeDelta failure_delay =
      (update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations - 2) *
      update_checker_internal::kStartUpdateCheckTimerRetryTime;
  task_environment_.FastForwardBy(failure_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Reset failure mode and fast forward by the retry period. This time it
  // should succeed in setting an update check timer. No update checks should
  // happen yet but a check has just been scheduled.
  scheduled_task_executor_->SimulateCalculateNextScheduledTaskFailure(false);
  task_environment_.FastForwardBy(
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Check if update checks happen daily from now on.
  base::TimeDelta delay_till_next_update_check =
      delay_from_now - failure_delay -
      update_checker_internal::kStartUpdateCheckTimerRetryTime;
  const int days = 2;
  for (int i = 0; i < days; i++) {
    // Fast forward to right before the next update check and ensure that no
    // update checks happened.
    base::TimeDelta small_delay = base::Milliseconds(1);
    task_environment_.FastForwardBy(delay_till_next_update_check - small_delay);
    EXPECT_TRUE(CheckStats(expected_update_checks,
                           expected_update_check_requests,
                           expected_update_check_completions));

    expected_update_checks += 1;
    expected_update_check_requests += 1;
    expected_update_check_completions += 1;
    task_environment_.FastForwardBy(small_delay);
    // Simulate update check succeeding.
    NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
    EXPECT_TRUE(CheckStats(expected_update_checks,
                           expected_update_check_requests,
                           expected_update_check_completions));
    delay_till_next_update_check = base::Days(1);
  }
}

// Checks if an update check timer can't be started due to a calculation
// failure, retries are capped.
TEST_F(DeviceScheduledUpdateCheckerTest,
       CheckRetryLogicCapWithCalculationFailure) {
  // This will simulate an error while calculating the next update check time
  // and will result in no update checks happening till its set.
  scheduled_task_executor_->SimulateCalculateNextScheduledTaskFailure(true);
  EXPECT_FALSE(CheckDailyUpdateCheck(1 /* hours_from_now */));

  // Fast forward by max retries * retry period and check that no update has
  // happened since failure mode is still set.
  task_environment_.FastForwardBy(
      update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations *
      update_checker_internal::kStartUpdateCheckTimerRetryTime);
  EXPECT_EQ(device_scheduled_update_checker_->GetUpdateCheckTimerExpirations(),
            0);
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 0);

  // At this point all state has been reset. Reset failure mode and check if
  // daily update checks happen.
  scheduled_task_executor_->SimulateCalculateNextScheduledTaskFailure(false);
  EXPECT_TRUE(CheckDailyUpdateCheck(1 /* hours_from_now */));
}

// Checks when an update check is unsuccessful retries are scheduled.
TEST_F(DeviceScheduledUpdateCheckerTest, CheckRetryLogicUpdateCheckFailure) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then weekly after.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kWeekly, kTaskTimeFieldName);

  // Set a new scheduled update setting, fast forward to expected update check
  // time and check if it happpens. Update check completion shouldn't happen as
  // an error is simulated.
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  int expected_update_checks = 1;
  int expected_update_check_requests = 1;
  int expected_update_check_completions = 0;
  task_environment_.FastForwardBy(delay_from_now);
  NotifyUpdateCheckStatus(update_engine::Operation::ERROR);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward for (max retries allowed) and check if each retry increases
  // the update check requests while we simulate an error.
  for (int i = 0;
       i <
       update_checker_internal::kMaxOsAndPoliciesUpdateCheckerRetryIterations;
       i++) {
    expected_update_check_requests += 1;
    task_environment_.FastForwardBy(
        update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime);
    // Simulate update check failing.
    NotifyUpdateCheckStatus(update_engine::Operation::ERROR);
    EXPECT_TRUE(CheckStats(expected_update_checks,
                           expected_update_check_requests,
                           expected_update_check_completions));
  }

  // No retries should be scheduled till the next update check timer fires. Fast
  // forward to just before the timer firing and check.
  const base::TimeDelta delay_till_next_update_check_timer =
      base::Days(kDaysInAWeek) -
      (update_checker_internal::kMaxOsAndPoliciesUpdateCheckerRetryIterations *
       update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime);
  const base::TimeDelta small_delay = base::Milliseconds(1);
  task_environment_.FastForwardBy(delay_till_next_update_check_timer -
                                  small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Check if the next update check timer fires and an update check is
  // initiated.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

// Checks if an update check is successful after retries.
TEST_F(DeviceScheduledUpdateCheckerTest,
       CheckUpdateCheckFailureEventualSuccess) {
  // Set the first update check to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then weekly after.
  base::TimeDelta delay_from_now = base::Hours(49);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kWeekly, kTaskTimeFieldName);

  // Set a new scheduled update setting, fast forward to expected update check
  // time and check if it happpens. Update check completion shouldn't happen as
  // an error is simulated.
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  int expected_update_checks = 1;
  int expected_update_check_requests = 1;
  int expected_update_check_completions = 0;
  task_environment_.FastForwardBy(delay_from_now);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::ERROR);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward for (max retries allowed - 1) and check if each retry
  // increases the update check requests while we simulate an error.
  for (int i = 0;
       i <
       (update_checker_internal::kMaxOsAndPoliciesUpdateCheckerRetryIterations -
        1);
       i++) {
    expected_update_check_requests += 1;
    task_environment_.FastForwardBy(
        update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime);
    NotifyUpdateCheckStatus(update_engine::Operation::ERROR);
    EXPECT_TRUE(CheckStats(expected_update_checks,
                           expected_update_check_requests,
                           expected_update_check_completions));
  }

  // Simulate success on the last retry attempt. This time the update check
  // should complete.
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  task_environment_.FastForwardBy(
      update_checker_internal::kOsAndPoliciesUpdateCheckerRetryTime);
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

// Checks if an update check timer can't be started, any previous pending update
// completion will still be completed.
TEST_F(DeviceScheduledUpdateCheckerTest, CheckNewPolicyWithPendingUpdateCheck) {
  // Calculate time from one hour from now and set the update check policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);

  // Set a new scheduled update setting, fast forward to the expected time and
  // and then check if an update check is scheduled.
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  int expected_update_checks = 1;
  int expected_update_check_requests = 1;
  int expected_update_check_completions = 0;
  task_environment_.FastForwardBy(delay_from_now);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Set a new scheduled update setting, this won't start a update check timer
  // but will wait for the existing update check to complete and start the timer
  // based on the new policy.
  delay_from_now = base::Minutes(30);
  policy_and_next_update_check_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  expected_update_check_completions += 1;
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Verify the timer was started based on the new policy by checking if the
  // update check happens at the new policy's time.
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  task_environment_.FastForwardBy(delay_from_now);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

// Checks if a time zone change successfully recalculates update check timer
// expiration delays when time zone moves forward.
TEST_F(DeviceScheduledUpdateCheckerTest,
       CheckRecalculationOnForwardTimezoneChange) {
  EXPECT_TRUE(CheckRecalculationOnTimezoneChange(kISTTimeZoneID));
}

// Checks if a time zone change successfully recalculates update check timer
// expiration delays when time zone moves backward.
TEST_F(DeviceScheduledUpdateCheckerTest,
       CheckRecalculationOnBackwardTimezoneChange) {
  EXPECT_TRUE(CheckRecalculationOnTimezoneChange(kPSTTimeZoneID));
}

// Check if no network is present for more than |kWaitForNetworkTimeout|, an
// update check fails. When the network comes back again, the next update check
// succeeds.
TEST_F(DeviceScheduledUpdateCheckerTest, CheckNoNetworkTimeoutScenario) {
  // Go offline to cause update check failures.
  network_state_test_helper_->ClearServices();

  // Create and set daily policy starting from one hour from now.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));

  // Fast forward to right before the expected update and then check if an
  // update check is not scheduled.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  int expected_update_checks = 0;
  int expected_update_check_requests = 0;
  int expected_update_check_completions = 0;
  device_scheduled_update_checker_->GetUpdateCheckCompletions();
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward to the expected update check time + |kMaxNetworkTimeout|. Due
  // to no network being connected but no update check requests or completions
  // should happens.
  expected_update_checks += 1;
  task_environment_.FastForwardBy(
      small_delay + update_checker_internal::kWaitForNetworkTimeout);

  // Go online again. This time the next scheduled update check should complete.
  network_state_test_helper_->ConfigureService(
      CreateConnectedWifiConfigurationJsonString("fake-wifi-network"));
  expected_update_checks += 1;
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  task_environment_.FastForwardBy(
      base::Days(1) - update_checker_internal::kWaitForNetworkTimeout);
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

// Check if no network is present for < than |kWaitForNetworkTimeout|, and then
// there is a valid network present, update check will succeed.
TEST_F(DeviceScheduledUpdateCheckerTest, CheckNoNetworkDelayScenario) {
  // Go offline to cause update check failures.
  network_state_test_helper_->ClearServices();

  // Create and set daily policy starting from one hour from now.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));

  // Fast forward to right before the expected update and then check if an
  // update check is not scheduled.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  int expected_update_checks = 0;
  int expected_update_check_requests = 0;
  int expected_update_check_completions = 0;
  device_scheduled_update_checker_->GetUpdateCheckCompletions();
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // Fast forward to the expected update check time + |kMaxNetworkTimeout| -
  // |small_delay|. Due to no network being connected no update check requests
  // or completions should happen.
  const base::TimeDelta network_not_present_delay =
      update_checker_internal::kWaitForNetworkTimeout - small_delay;
  expected_update_checks += 1;
  task_environment_.FastForwardBy(small_delay + network_not_present_delay);

  // Go online again. The existing update check should complete.
  network_state_test_helper_->ConfigureService(
      CreateConnectedWifiConfigurationJsonString("fake-wifi-network"));
  expected_update_check_requests += 1;
  expected_update_check_completions += 1;
  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

// Checks if only one wake lock is acquired when the update check timer fires
// and released when an update check and policy refresh is completed.
TEST_F(DeviceScheduledUpdateCheckerTest, CheckWakeLockAcquireAndRelease) {
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);

  // Fast forward to update check timer expiration. This should result in a wake
  // lock being acquired.
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  task_environment_.FastForwardBy(delay_from_now);

  std::optional<int> active_wake_locks_before_update_check;
  wake_lock_provider_.GetActiveWakeLocksForTests(
      device::mojom::WakeLockType::kPreventAppSuspension,
      base::BindOnce([](std::optional<int>* result,
                        int32_t wake_lock_count) { *result = wake_lock_count; },
                     &active_wake_locks_before_update_check));
  EXPECT_TRUE(active_wake_locks_before_update_check);
  EXPECT_EQ(active_wake_locks_before_update_check.value(), 1);
  // Run until idle to run the wake lock count callback.
  task_environment_.RunUntilIdle();

  // Simulate update check succeeding.
  NotifyUpdateCheckStatus(update_engine::Operation::UPDATED_NEED_REBOOT);

  std::optional<int> active_wake_locks_after_update_check;
  wake_lock_provider_.GetActiveWakeLocksForTests(
      device::mojom::WakeLockType::kPreventAppSuspension,
      base::BindOnce([](std::optional<int>* result,
                        int32_t wake_lock_count) { *result = wake_lock_count; },
                     &active_wake_locks_after_update_check));
  // After all steps are completed the wake lock should be released.
  EXPECT_TRUE(active_wake_locks_after_update_check);
  EXPECT_EQ(active_wake_locks_after_update_check.value(), 0);
}

// Checks if an update check is aborted after the stipulated hard timeout.
TEST_F(DeviceScheduledUpdateCheckerTest, CheckUpdateCheckHardTimeout) {
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);

  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  task_environment_.FastForwardBy(delay_from_now);

  // Don't simulate update check succeeding, the update check should abort after
  // |kOsAndPoliciesUpdateCheckHardTimeout|.
  int expected_update_checks = 1;
  int expected_update_check_requests = 1;
  int expected_update_check_completions = 0;
  task_environment_.FastForwardBy(
      update_checker_internal::kOsAndPoliciesUpdateCheckHardTimeout);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));

  // The next update check timer should be scheduled regardless of the previous
  // update check failure.
  expected_update_checks = 2;
  expected_update_check_requests = 2;
  task_environment_.FastForwardBy(
      base::Days(1) -
      update_checker_internal::kOsAndPoliciesUpdateCheckHardTimeout);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

// Check that the facility is disabled when the RTC wake support feature is
// disabled.
TEST_F(DeviceScheduledUpdateCheckerTest, DisabledFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      ::features::kSupportsRtcWakeOver24Hours);

  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_update_check_time =
      scheduled_task_test_util::CreatePolicy(
          scheduled_task_executor_->GetTimeZone(),
          scheduled_task_executor_->GetCurrentTime(), delay_from_now,
          ScheduledTaskExecutor::Frequency::kDaily, kTaskTimeFieldName);

  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledUpdateCheck,
      std::move(policy_and_next_update_check_time.first));
  task_environment_.FastForwardBy(delay_from_now);

  // No check should happen when kSupportsRtcWakeOver24Hours is off.
  int expected_update_checks = 0;
  int expected_update_check_requests = 0;
  int expected_update_check_completions = 0;

  task_environment_.FastForwardBy(
      update_checker_internal::kOsAndPoliciesUpdateCheckHardTimeout);
  EXPECT_TRUE(CheckStats(expected_update_checks, expected_update_check_requests,
                         expected_update_check_completions));
}

}  // namespace policy
