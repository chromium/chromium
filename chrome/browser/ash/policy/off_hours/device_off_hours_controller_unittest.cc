// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/policy/off_hours/device_off_hours_controller.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/power_monitor_test.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"

namespace policy::off_hours {

namespace {

namespace em = ::enterprise_management;

constexpr em::WeeklyTimeProto_DayOfWeek kWeekdays[] = {
    em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED,
    em::WeeklyTimeProto::MONDAY,
    em::WeeklyTimeProto::TUESDAY,
    em::WeeklyTimeProto::WEDNESDAY,
    em::WeeklyTimeProto::THURSDAY,
    em::WeeklyTimeProto::FRIDAY,
    em::WeeklyTimeProto::SATURDAY,
    em::WeeklyTimeProto::SUNDAY};

constexpr base::TimeDelta kHour = base::Hours(1);
constexpr base::TimeDelta kDay = base::Days(1);

const char kGmtTimezone[] = "GMT";
const char kBerlinTimezone[] = "Europe/Berlin";
const char kLosAngelesTimezone[] = "America/Los_Angeles";

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

// Return number of weekday from 1 to 7 in |input_time|.
// (1 = Monday etc.)
int ExtractDayOfWeek(base::Time input_time) {
  base::Time::Exploded exploded;
  input_time.UTCExplode(&exploded);
  int current_day_of_week = exploded.day_of_week;
  if (current_day_of_week == 0)
    current_day_of_week = 7;
  return current_day_of_week;
}

// Return next day of week. |day_of_week| and return value are from 1 to 7.
// (1 = Monday etc.)
int NextDayOfWeek(int day_of_week) {
  return day_of_week % 7 + 1;
}

// Add DeviceOffHours policy to |proto| with an interval that includes the
// current time (until tomorrow at 10am).
// That gives us at least 10 hours to test things that depend on OffHours being
// active.
void SetOffHoursNowInProto(em::ChromeDeviceSettingsProto* proto) {
  const int current_day_of_week = ExtractDayOfWeek(base::Time::Now());
  SetOffHoursPolicyToProto(
      proto,
      OffHoursPolicy(kGmtTimezone,
                     {WeeklyTimeInterval(
                         WeeklyTime(current_day_of_week, 0, 0),
                         WeeklyTime(NextDayOfWeek(current_day_of_week),
                                    base::Hours(10).InMilliseconds(), 0))}));
}
}  // namespace

class DeviceOffHoursControllerSimpleTest : public ash::DeviceSettingsTestBase {
 public:
  DeviceOffHoursControllerSimpleTest(
      const DeviceOffHoursControllerSimpleTest&) = delete;
  DeviceOffHoursControllerSimpleTest& operator=(
      const DeviceOffHoursControllerSimpleTest&) = delete;

 protected:
  DeviceOffHoursControllerSimpleTest()
      : ash::DeviceSettingsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~DeviceOffHoursControllerSimpleTest() override = default;

  void SetUp() override {
    ash::DeviceSettingsTestBase::SetUp();
    ash::SystemClockClient::InitializeFake();
    system_clock_client()->SetServiceIsAvailable(false);

    device_settings_service_->SetDeviceOffHoursControllerForTesting(
        std::make_unique<DeviceOffHoursController>());
  }

  void TearDown() override {
    ash::SystemClockClient::Shutdown();
    ash::DeviceSettingsTestBase::TearDown();
  }

  void UpdateDeviceSettings() {
    device_policy_->Build();
    session_manager_client_.set_device_policy(device_policy_->GetBlob());
    ReloadDeviceSettings();
  }

  bool IsGuestModeEnabled() const {
    DCHECK(device_settings_service_);
    DCHECK(device_settings_service_->device_settings());
    return device_settings_service_->device_settings()
        ->guest_mode_enabled()
        .guest_mode_enabled();
  }

  ash::SystemClockClient::TestInterface* system_clock_client() {
    return ash::SystemClockClient::Get()->GetTestInterface();
  }

  DeviceOffHoursController* device_off_hours_controller() {
    return device_settings_service_->device_off_hours_controller();
  }
};

TEST_F(DeviceOffHoursControllerSimpleTest, CheckOffHoursUnset) {
  system_clock_client()->SetServiceIsAvailable(true);
  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  UpdateDeviceSettings();
  EXPECT_FALSE(IsGuestModeEnabled());
  RemoveOffHoursPolicyFromProto(&proto);
  UpdateDeviceSettings();
  EXPECT_FALSE(IsGuestModeEnabled());
}

TEST_F(DeviceOffHoursControllerSimpleTest, CheckOffHoursModeOff) {
  system_clock_client()->SetServiceIsAvailable(true);
  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  UpdateDeviceSettings();
  EXPECT_FALSE(IsGuestModeEnabled());
  int current_day_of_week = ExtractDayOfWeek(base::Time::Now());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(kGmtTimezone,
                     {WeeklyTimeInterval(
                         WeeklyTime(NextDayOfWeek(current_day_of_week),
                                    base::Hours(10).InMilliseconds(), 0),
                         WeeklyTime(NextDayOfWeek(current_day_of_week),
                                    base::Hours(15).InMilliseconds(), 0))}));
  UpdateDeviceSettings();
  EXPECT_FALSE(IsGuestModeEnabled());
}

TEST_F(DeviceOffHoursControllerSimpleTest, CheckOffHoursModeOn) {
  system_clock_client()->SetServiceIsAvailable(true);
  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  UpdateDeviceSettings();
  EXPECT_FALSE(IsGuestModeEnabled());
  SetOffHoursNowInProto(&proto);
  UpdateDeviceSettings();
  EXPECT_TRUE(IsGuestModeEnabled());
}

TEST_F(DeviceOffHoursControllerSimpleTest,
       CheckOffHoursEnabledBeforeSystemClockUpdated) {
  system_clock_client()->SetServiceIsAvailable(false);
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  UpdateDeviceSettings();
  SetOffHoursNowInProto(&proto);
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
  EXPECT_FALSE(IsGuestModeEnabled());
  SetOffHoursNowInProto(&proto);
  UpdateDeviceSettings();
  EXPECT_FALSE(IsGuestModeEnabled());
}

TEST_F(DeviceOffHoursControllerSimpleTest,
       IsCurrentSessionAllowedOnlyForOffHours) {
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager> user_manager{
      std::make_unique<ash::FakeChromeUserManager>()};

  system_clock_client()->SetServiceIsAvailable(true);
  EXPECT_FALSE(
      device_off_hours_controller()->IsCurrentSessionAllowedOnlyForOffHours());

  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  EXPECT_FALSE(
      device_off_hours_controller()->IsCurrentSessionAllowedOnlyForOffHours());

  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(false);
  SetOffHoursNowInProto(&proto);
  UpdateDeviceSettings();

  EXPECT_FALSE(
      device_off_hours_controller()->IsCurrentSessionAllowedOnlyForOffHours());

  user_manager->AddGuestUser();
  user_manager->LoginUser(user_manager::GuestAccountId());

  EXPECT_TRUE(
      device_off_hours_controller()->IsCurrentSessionAllowedOnlyForOffHours());
}

class DeviceOffHoursControllerFakeClockTest
    : public DeviceOffHoursControllerSimpleTest {
 public:
  DeviceOffHoursControllerFakeClockTest(
      const DeviceOffHoursControllerFakeClockTest&) = delete;
  DeviceOffHoursControllerFakeClockTest& operator=(
      const DeviceOffHoursControllerFakeClockTest&) = delete;

 protected:
  DeviceOffHoursControllerFakeClockTest() {}

  void SetUp() override {
    DeviceOffHoursControllerSimpleTest::SetUp();
    system_clock_client()->SetNetworkSynchronized(true);
    system_clock_client()->NotifyObserversSystemClockUpdated();
    // Clocks are set to 1970-01-01 00:00:00 UTC, Thursday.
    test_clock_.SetNow(base::Time::UnixEpoch());
    device_off_hours_controller()->SetClockForTesting(
        &test_clock_, task_environment_.GetMockTickClock());
  }

  void AdvanceTestClock(base::TimeDelta duration) {
    test_clock_.Advance(duration);
    task_environment_.FastForwardBy(duration);

    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void SuspendFor(base::TimeDelta duration) {
    fake_power_monitor_source_.Suspend();

    test_clock_.Advance(duration);

    fake_power_monitor_source_.Resume();

    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  base::Clock* clock() { return &test_clock_; }

 private:
  base::SimpleTestClock test_clock_;
  base::test::ScopedPowerMonitorTestSource fake_power_monitor_source_;
};

TEST_F(DeviceOffHoursControllerFakeClockTest, FakeClock) {
  system_clock_client()->SetServiceIsAvailable(true);
  EXPECT_FALSE(device_off_hours_controller()->is_off_hours_mode());
  int current_day_of_week = ExtractDayOfWeek(clock()->Now());
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(kGmtTimezone,
                     {WeeklyTimeInterval(
                         WeeklyTime(current_day_of_week,
                                    base::Hours(14).InMilliseconds(), 0),
                         WeeklyTime(current_day_of_week,
                                    base::Hours(15).InMilliseconds(), 0))}));
  AdvanceTestClock(base::Hours(14));
  UpdateDeviceSettings();
  EXPECT_TRUE(device_off_hours_controller()->is_off_hours_mode());
  AdvanceTestClock(base::Hours(1));
  UpdateDeviceSettings();
  EXPECT_FALSE(device_off_hours_controller()->is_off_hours_mode());
}

TEST_F(DeviceOffHoursControllerFakeClockTest, CheckUnderSuspend) {
  system_clock_client()->SetServiceIsAvailable(true);
  int current_day_of_week = ExtractDayOfWeek(clock()->Now());
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  SetOffHoursPolicyToProto(
      &proto,
      OffHoursPolicy(kGmtTimezone,
                     {WeeklyTimeInterval(
                         WeeklyTime(NextDayOfWeek(current_day_of_week), 0, 0),
                         WeeklyTime(NextDayOfWeek(current_day_of_week),
                                    kHour.InMilliseconds(), 0))}));
  UpdateDeviceSettings();
  EXPECT_FALSE(device_off_hours_controller()->is_off_hours_mode());

  SuspendFor(kDay);
  EXPECT_TRUE(device_off_hours_controller()->is_off_hours_mode());

  AdvanceTestClock(kHour);
  EXPECT_FALSE(device_off_hours_controller()->is_off_hours_mode());
}

class DeviceOffHoursControllerUpdateTest
    : public DeviceOffHoursControllerFakeClockTest,
      public testing::WithParamInterface<
          std::tuple<OffHoursPolicy, base::TimeDelta, bool>> {
 public:
  OffHoursPolicy off_hours_policy() const { return std::get<0>(GetParam()); }
  base::TimeDelta advance_clock() const { return std::get<1>(GetParam()); }
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

// This is an interval from 1am to 2am on Thursdays.
// We use Thursday, because 1970-01-01 was a Thursday and we use that date in
// |DeviceOffHoursControllerFakeClockTest|.
const auto kOffHoursInterval =
    WeeklyTimeInterval(WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                                  base::Hours(1).InMilliseconds(),
                                  0),
                       WeeklyTime(em::WeeklyTimeProto::THURSDAY,
                                  base::Hours(2).InMilliseconds(),
                                  0));
INSTANTIATE_TEST_SUITE_P(
    TestCases,
    DeviceOffHoursControllerUpdateTest,
    testing::Values(
        // ----- Using GMT timezone
        std::make_tuple(OffHoursPolicy(kGmtTimezone, {kOffHoursInterval}),
                        base::TimeDelta{},  // Staying at 1970-01-01T00:00:00
                        false),
        std::make_tuple(OffHoursPolicy(kGmtTimezone, {kOffHoursInterval}),
                        kHour,  // Advancing to 1970-01-01T01:00:00
                        true),
        std::make_tuple(OffHoursPolicy(kGmtTimezone, {kOffHoursInterval}),
                        kHour * 1.5,
                        true),  // Advancing to 1970-01-01T01:30:00
        std::make_tuple(OffHoursPolicy(kGmtTimezone, {kOffHoursInterval}),
                        kHour * 2,
                        false),  // Advancing to 1970-01-01T02:00:00
        std::make_tuple(OffHoursPolicy(kGmtTimezone, {kOffHoursInterval}),
                        kHour * 3,  // Advancing to 1970-01-01T03:00:00
                        false),
        // ----- Using Berlin timezone, one hour ahead of GMT
        std::make_tuple(OffHoursPolicy(kBerlinTimezone, {kOffHoursInterval}),
                        base::TimeDelta{},  // Staying at 1970-01-01T00:00:00
                        true),
        std::make_tuple(OffHoursPolicy(kBerlinTimezone, {kOffHoursInterval}),
                        kHour * 0.5,  // Advancing to 1970-01-01T00:30:00
                        true),
        std::make_tuple(OffHoursPolicy(kBerlinTimezone, {kOffHoursInterval}),
                        kHour * 1,  // Advancing to 1970-01-01T01:00:00
                        false),
        // ----- Using Los Angeles timezone, eight hours behind GMT
        std::make_tuple(OffHoursPolicy(kLosAngelesTimezone,
                                       {kOffHoursInterval}),
                        kHour * 8,  // Advancing to 1970-01-01T08:00:00
                        false),
        std::make_tuple(OffHoursPolicy(kLosAngelesTimezone,
                                       {kOffHoursInterval}),
                        kHour * 9,  // Advancing to 1970-01-01T09:00:00
                        true),
        std::make_tuple(OffHoursPolicy(kLosAngelesTimezone,
                                       {kOffHoursInterval}),
                        kHour * 9.5,  // Advancing to 1970-01-01T09:30:00
                        true),
        std::make_tuple(OffHoursPolicy(kLosAngelesTimezone,
                                       {kOffHoursInterval}),
                        kHour * 10,  // Advancing to 1970-01-01T10:00:00
                        false)));

}  // namespace policy::off_hours
