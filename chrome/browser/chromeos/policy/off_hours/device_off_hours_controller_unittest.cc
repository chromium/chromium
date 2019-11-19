// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/off_hours/device_off_hours_controller.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace em = enterprise_management;

namespace policy {
namespace off_hours {

using base::TimeDelta;

namespace {

constexpr em::WeeklyTimeProto_DayOfWeek kWeekdays[] = {
    em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED,
    em::WeeklyTimeProto::MONDAY,
    em::WeeklyTimeProto::TUESDAY,
    em::WeeklyTimeProto::WEDNESDAY,
    em::WeeklyTimeProto::THURSDAY,
    em::WeeklyTimeProto::FRIDAY,
    em::WeeklyTimeProto::SATURDAY,
    em::WeeklyTimeProto::SUNDAY};

constexpr TimeDelta kHour = TimeDelta::FromHours(1);
constexpr TimeDelta kDay = TimeDelta::FromDays(1);

const char kUtcTimezone[] = "UTC";

const int kDeviceAllowNewUsersPolicyTag = 3;
const int kDeviceGuestModeEnabledPolicyTag = 8;

struct OffHoursPolicy {
  std::string timezone;
  std::vector<WeeklyTimeInterval> intervals;
  std::vector<int> ignored_policy_proto_tags;

  OffHoursPolicy(const std::string& timezone,
                 const std::vector<WeeklyTimeInterval>& intervals,
                 const std::vector<int>& ignored_policy_proto_tags)
      : timezone(timezone),
        intervals(intervals),
        ignored_policy_proto_tags(ignored_policy_proto_tags) {}

  OffHoursPolicy(const std::string& timezone,
                 const std::vector<WeeklyTimeInterval>& intervals)
      : timezone(timezone),
        intervals(intervals),
        ignored_policy_proto_tags({kDeviceAllowNewUsersPolicyTag,
                                   kDeviceGuestModeEnabledPolicyTag}) {}
};

em::WeeklyTimeIntervalProto ConvertWeeklyTimeIntervalToProto(
    const WeeklyTimeInterval& weekly_time_interval) {
  em::WeeklyTimeIntervalProto interval_proto;
  em::WeeklyTimeProto* start = interval_proto.mutable_start();
  em::WeeklyTimeProto* end = interval_proto.mutable_end();
  start->set_day_of_week(kWeekdays[weekly_time_interval.start().day_of_week()]);
  start->set_time(weekly_time_interval.start().milliseconds());
  end->set_day_of_week(kWeekdays[weekly_time_interval.end().day_of_week()]);
  end->set_time(weekly_time_interval.end().milliseconds());
  return interval_proto;
}

void RemoveOffHoursPolicyFromProto(em::ChromeDeviceSettingsProto* proto) {
  proto->clear_device_off_hours();
}

void SetOffHoursPolicyToProto(em::ChromeDeviceSettingsProto* proto,
                              const OffHoursPolicy& off_hours_policy) {
  RemoveOffHoursPolicyFromProto(proto);
  auto* off_hours = proto->mutable_device_off_hours();
  for (auto interval : off_hours_policy.intervals) {
    auto interval_proto = ConvertWeeklyTimeIntervalToProto(interval);
    auto* cur = off_hours->add_intervals();
    *cur = interval_proto;
  }
  off_hours->set_timezone(off_hours_policy.timezone);
  for (auto p : off_hours_policy.ignored_policy_proto_tags) {
    off_hours->add_ignored_policy_proto_tags(p);
  }
}

}  // namespace

class DeviceOffHoursControllerSimpleTest
    : public chromeos::DeviceSettingsTestBase {
 protected:
  DeviceOffHoursControllerSimpleTest() = default;
  ~DeviceOffHoursControllerSimpleTest() override = default;

  void SetUp() override {
    chromeos::DeviceSettingsTestBase::SetUp();
    chromeos::SystemClockClient::InitializeFake();
    system_clock_client()->SetServiceIsAvailable(false);

    device_settings_service_->SetDeviceOffHoursControllerForTesting(
        std::make_unique<policy::off_hours::DeviceOffHoursController>());
    device_off_hours_controller_ =
        device_settings_service_->device_off_hours_controller();
  }

  void TearDown() override {
    chromeos::SystemClockClient::Shutdown();
    chromeos::DeviceSettingsTestBase::TearDown();
  }

  void UpdateDeviceSettings() {
    device_policy_->Build();
    session_manager_client_.set_device_policy(device_policy_->GetBlob());
    ReloadDeviceSettings();
  }

  // Return number of weekday from 1 to 7 in |input_time|. (1 = Monday etc.)
  int ExtractDayOfWeek(base::Time input_time) {
    base::Time::Exploded exploded;
    input_time.UTCExplode(&exploded);
    int current_day_of_week = exploded.day_of_week;
    if (current_day_of_week == 0)
      current_day_of_week = 7;
    return current_day_of_week;
  }

  // Return next day of week. |day_of_week| and return value are from 1 to 7. (1
  // = Monday etc.)
  int NextDayOfWeek(int day_of_week) { return day_of_week % 7 + 1; }

  chromeos::SystemClockClient::TestInterface* system_clock_client() {
    return chromeos::SystemClockClient::Get()->GetTestInterface();
  }

  policy::off_hours::DeviceOffHoursController* device_off_hours_controller() {
    return device_off_hours_controller_;
  }

  // The object is owned by DeviceSettingsService class.
  policy::off_hours::DeviceOffHoursController* device_off_hours_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceOffHoursControllerSimpleTest);
};

TEST_F(DeviceOffHoursControllerSimpleTest, CheckOffHoursUnset) {
  system_clock_client()->SetServiceIsAvailable(true);
  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  UpdateDeviceSettings();
  EXPECT_FALSE(device_settings_service_->device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
  RemoveOffHoursPolicyFromProto(&proto);
  UpdateDeviceSettings();
  EXPECT_FALSE(device_settings_service_->device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
}

TEST_F(DeviceOffHoursControllerSimpleTest, CheckOffHoursModeOff) {
  system_clock_client()->SetServiceIsAvailable(true);
  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  UpdateDeviceSettings();
  EXPECT_FALSE(device_settings_service_->device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
  int current_day_of_week = ExtractDayOfWeek(base::Time::Now());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(
          kUtcTimezone,
          {WeeklyTimeInterval(
              WeeklyTime(NextDayOfWeek(current_day_of_week),
                         TimeDelta::FromHours(10).InMilliseconds(), 0),
              WeeklyTime(NextDayOfWeek(current_day_of_week),
                         TimeDelta::FromHours(15).InMilliseconds(), 0))}));
  UpdateDeviceSettings();
  EXPECT_FALSE(device_settings_service_->device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
}

TEST_F(DeviceOffHoursControllerSimpleTest, CheckOffHoursModeOn) {
  system_clock_client()->SetServiceIsAvailable(true);
  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  UpdateDeviceSettings();
  EXPECT_FALSE(device_settings_service_->device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
  int current_day_of_week = ExtractDayOfWeek(base::Time::Now());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(
          kUtcTimezone,
          {WeeklyTimeInterval(
              WeeklyTime(current_day_of_week, 0, 0),
              WeeklyTime(NextDayOfWeek(current_day_of_week),
                         TimeDelta::FromHours(10).InMilliseconds(), 0))}));
  UpdateDeviceSettings();
  EXPECT_TRUE(device_settings_service_->device_settings()
                  ->guest_mode_enabled()
                  .guest_mode_enabled());
}

TEST_F(DeviceOffHoursControllerSimpleTest,
       CheckOffHoursEnabledBeforeSystemClockUpdated) {
  system_clock_client()->SetServiceIsAvailable(false);
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  UpdateDeviceSettings();
  int current_day_of_week = ExtractDayOfWeek(base::Time::Now());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(
          kUtcTimezone,
          {WeeklyTimeInterval(
              WeeklyTime(current_day_of_week, 0, 0),
              WeeklyTime(NextDayOfWeek(current_day_of_week),
                         TimeDelta::FromHours(10).InMilliseconds(), 0))}));
  UpdateDeviceSettings();
  // Trust the time until response from SystemClock is received.
  EXPECT_TRUE(device_off_hours_controller()->is_off_hours_mode());

  // SystemClock is updated.
  system_clock_client()->SetServiceIsAvailable(true);
  system_clock_client()->SetNetworkSynchronized(false);
  system_clock_client()->NotifyObserversSystemClockUpdated();
  UpdateDeviceSettings();

  // Response from SystemClock arrived, stop trusting the time.
  EXPECT_FALSE(device_off_hours_controller()->is_off_hours_mode());
}

TEST_F(DeviceOffHoursControllerSimpleTest, NoNetworkSynchronization) {
  system_clock_client()->SetServiceIsAvailable(true);
  system_clock_client()->SetNetworkSynchronized(false);
  system_clock_client()->NotifyObserversSystemClockUpdated();
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  UpdateDeviceSettings();
  EXPECT_FALSE(device_settings_service_->device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
  int current_day_of_week = ExtractDayOfWeek(base::Time::Now());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(
          kUtcTimezone,
          {WeeklyTimeInterval(
              WeeklyTime(current_day_of_week, 0, 0),
              WeeklyTime(NextDayOfWeek(current_day_of_week),
                         TimeDelta::FromHours(10).InMilliseconds(), 0))}));
  EXPECT_FALSE(device_settings_service_->device_settings()
                   ->guest_mode_enabled()
                   .guest_mode_enabled());
}

TEST_F(DeviceOffHoursControllerSimpleTest,
       IsCurrentSessionAllowedOnlyForOffHours) {
  system_clock_client()->SetServiceIsAvailable(true);
  EXPECT_FALSE(
      device_off_hours_controller()->IsCurrentSessionAllowedOnlyForOffHours());

  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  EXPECT_FALSE(
      device_off_hours_controller()->IsCurrentSessionAllowedOnlyForOffHours());

  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  int current_day_of_week = ExtractDayOfWeek(base::Time::Now());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(
          kUtcTimezone,
          {WeeklyTimeInterval(
              WeeklyTime(current_day_of_week, 0, 0),
              WeeklyTime(NextDayOfWeek(current_day_of_week),
                         TimeDelta::FromHours(10).InMilliseconds(), 0))}));
  UpdateDeviceSettings();

  EXPECT_FALSE(
      device_off_hours_controller()->IsCurrentSessionAllowedOnlyForOffHours());

  user_manager_->AddGuestUser();
  user_manager_->LoginUser(user_manager_->GetGuestAccountId());

  EXPECT_TRUE(
      device_off_hours_controller()->IsCurrentSessionAllowedOnlyForOffHours());
}

class DeviceOffHoursControllerFakeClockTest
    : public DeviceOffHoursControllerSimpleTest {
 protected:
  DeviceOffHoursControllerFakeClockTest() {}

  void SetUp() override {
    DeviceOffHoursControllerSimpleTest::SetUp();
    system_clock_client()->SetNetworkSynchronized(true);
    system_clock_client()->NotifyObserversSystemClockUpdated();
    // Clocks are set to 1970-01-01 00:00:00 UTC, Thursday.
    test_clock_.SetNow(base::Time::UnixEpoch());
    test_tick_clock_.SetNowTicks(base::TimeTicks::UnixEpoch());
    device_off_hours_controller()->SetClockForTesting(&test_clock_,
                                                      &test_tick_clock_);
  }

  void AdvanceTestClock(TimeDelta duration) {
    test_clock_.Advance(duration);
    test_tick_clock_.Advance(duration);
  }

  base::Clock* clock() { return &test_clock_; }

 private:
  base::SimpleTestClock test_clock_;
  base::SimpleTestTickClock test_tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOffHoursControllerFakeClockTest);
};

TEST_F(DeviceOffHoursControllerFakeClockTest, FakeClock) {
  system_clock_client()->SetServiceIsAvailable(true);
  EXPECT_FALSE(device_off_hours_controller()->is_off_hours_mode());
  int current_day_of_week = ExtractDayOfWeek(clock()->Now());
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(
          kUtcTimezone,
          {WeeklyTimeInterval(
              WeeklyTime(current_day_of_week,
                         TimeDelta::FromHours(14).InMilliseconds(), 0),
              WeeklyTime(current_day_of_week,
                         TimeDelta::FromHours(15).InMilliseconds(), 0))}));
  AdvanceTestClock(TimeDelta::FromHours(14));
  UpdateDeviceSettings();
  EXPECT_TRUE(device_off_hours_controller()->is_off_hours_mode());
  AdvanceTestClock(TimeDelta::FromHours(1));
  UpdateDeviceSettings();
  EXPECT_FALSE(device_off_hours_controller()->is_off_hours_mode());
}

TEST_F(DeviceOffHoursControllerFakeClockTest, CheckSendSuspendDone) {
  system_clock_client()->SetServiceIsAvailable(true);
  int current_day_of_week = ExtractDayOfWeek(clock()->Now());
  LOG(ERROR) << "day " << current_day_of_week;
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(kUtcTimezone,
                     {WeeklyTimeInterval(
                         WeeklyTime(NextDayOfWeek(current_day_of_week), 0, 0),
                         WeeklyTime(NextDayOfWeek(current_day_of_week),
                                    kHour.InMilliseconds(), 0))}));
  UpdateDeviceSettings();
  EXPECT_FALSE(device_off_hours_controller()->is_off_hours_mode());

  AdvanceTestClock(kDay);
  power_manager_client()->SendSuspendDone();
  EXPECT_TRUE(device_off_hours_controller()->is_off_hours_mode());

  AdvanceTestClock(kHour);
  power_manager_client()->SendSuspendDone();
  EXPECT_FALSE(device_off_hours_controller()->is_off_hours_mode());
}

class DeviceOffHoursControllerUpdateTest
    : public DeviceOffHoursControllerFakeClockTest,
      public testing::WithParamInterface<
          std::tuple<OffHoursPolicy, TimeDelta, bool>> {
 public:
  OffHoursPolicy off_hours_policy() const { return std::get<0>(GetParam()); }
  TimeDelta advance_clock() const { return std::get<1>(GetParam()); }
  bool is_off_hours_expected() const { return std::get<2>(GetParam()); }
};

TEST_P(DeviceOffHoursControllerUpdateTest, CheckUpdateOffHoursPolicy) {
  system_clock_client()->SetServiceIsAvailable(true);
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  SetOffHoursPolicyToProto(&proto, off_hours_policy());
  AdvanceTestClock(advance_clock());
  UpdateDeviceSettings();
  EXPECT_EQ(device_off_hours_controller()->is_off_hours_mode(),
            is_off_hours_expected());
}

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    DeviceOffHoursControllerUpdateTest,
    testing::Values(
        std::make_tuple(
            OffHoursPolicy(
                kUtcTimezone,
                {WeeklyTimeInterval(
                    WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                               TimeDelta::FromHours(1).InMilliseconds(),
                               0),
                    WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                               TimeDelta::FromHours(2).InMilliseconds(),
                               0))}),
            kHour,
            true),
        std::make_tuple(
            OffHoursPolicy(
                kUtcTimezone,
                {WeeklyTimeInterval(
                    WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                               TimeDelta::FromHours(1).InMilliseconds(),
                               0),
                    WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                               TimeDelta::FromHours(2).InMilliseconds(),
                               0))}),
            kHour * 2,
            false),
        std::make_tuple(
            OffHoursPolicy(
                kUtcTimezone,
                {WeeklyTimeInterval(
                    WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                               TimeDelta::FromHours(1).InMilliseconds(),
                               0),
                    WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                               TimeDelta::FromHours(2).InMilliseconds(),
                               0))}),
            kHour * 1.5,
            true),
        std::make_tuple(
            OffHoursPolicy(
                kUtcTimezone,
                {WeeklyTimeInterval(
                    WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                               TimeDelta::FromHours(1).InMilliseconds(),
                               0),
                    WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                               TimeDelta::FromHours(2).InMilliseconds(),
                               0))}),
            kHour * 3,
            false)));

}  // namespace off_hours
}  // namespace policy
