// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/smart_charging/smart_charging_ukm_logger.h"

#include "chrome/browser/ui/tabs/tab_ukm_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chromeos/dbus/power_manager/user_charging_event.pb.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {

namespace {

using UkmEntry = ukm::builders::SmartCharging;
using UserChargingEvent = power_manager::UserChargingEvent;

}  // namespace

class SmartChargingUkmLoggerTest : public ChromeRenderViewHostTestHarness {
 public:
  SmartChargingUkmLoggerTest() {}
  ~SmartChargingUkmLoggerTest() override = default;
  SmartChargingUkmLoggerTest(const SmartChargingUkmLoggerTest&) = delete;
  SmartChargingUkmLoggerTest& operator=(const SmartChargingUkmLoggerTest&) =
      delete;

  void LogEvent(const UserChargingEvent& user_charging_event) {
    smart_charging_ukm_logger_.LogEvent(user_charging_event);
  }

 protected:
  UkmEntryChecker ukm_entry_checker_;

 private:
  SmartChargingUkmLogger smart_charging_ukm_logger_;
};

TEST_F(SmartChargingUkmLoggerTest, TestRecordCorrectly) {
  // Constructs an input event.
  UserChargingEvent user_charging_event;
  user_charging_event.mutable_event()->set_event_id(5);
  user_charging_event.mutable_event()->set_reason(
      UserChargingEvent::Event::CHARGER_PLUGGED_IN);

  UserChargingEvent::Features* features =
      user_charging_event.mutable_features();
  features->set_battery_percentage(35);
  features->set_time_since_last_charge_minutes(128);
  features->set_duration_of_last_charge_minutes(47);
  features->set_battery_percentage_of_last_charge(80);
  features->set_battery_percentage_before_last_charge(19);
  features->set_time_of_the_day_minutes(620);
  features->set_day_of_week(UserChargingEvent::Features::WED);
  features->set_day_of_month(27);
  features->set_month(UserChargingEvent::Features::AUG);
  features->set_timezone_difference_from_last_charge_hours(-5);
  features->set_device_type(UserChargingEvent::Features::TABLET);
  features->set_device_mode(UserChargingEvent::Features::TABLET_MODE);
  features->set_num_recent_key_events(75);
  features->set_num_recent_mouse_events(235);
  features->set_num_recent_touch_events(139);
  features->set_num_recent_stylus_events(92);
  features->set_duration_recent_video_playing_minutes(4);
  features->set_duration_recent_audio_playing_minutes(8);
  features->set_screen_brightness_percent(23);
  features->set_voltage_mv(3500);
  features->set_halt_from_last_charge(true);
  features->set_is_charging(true);

  // Constructs the expected output.
  const UkmMetricMap user_charging_values = {
      {UkmEntry::kEventIdName, 5},
      {UkmEntry::kReasonName, 1},
      {UkmEntry::kBatteryPercentageName, 35},
      {UkmEntry::kTimeSinceLastChargeName, 128},
      {UkmEntry::kDurationOfLastChargeName, 47},
      {UkmEntry::kBatteryPercentageOfLastChargeName, 80},
      {UkmEntry::kBatteryPercentageBeforeLastChargeName, 19},
      {UkmEntry::kTimeOfTheDayName, 620},
      {UkmEntry::kDayOfWeekName, 3},
      {UkmEntry::kDayOfMonthName, 27},
      {UkmEntry::kMonthName, 8},
      {UkmEntry::kTimezoneDifferenceFromLastChargeName, -5},
      {UkmEntry::kDeviceTypeName, 4},
      {UkmEntry::kDeviceModeName, 3},
      {UkmEntry::kNumRecentKeyEventsName, 75},
      {UkmEntry::kNumRecentMouseEventsName, 235},
      {UkmEntry::kNumRecentTouchEventsName, 139},
      {UkmEntry::kNumRecentStylusEventsName, 92},
      {UkmEntry::kDurationRecentVideoPlayingName, 4},
      {UkmEntry::kDurationRecentAudioPlayingName, 8},
      {UkmEntry::kScreenBrightnessPercentName, 23},
      {UkmEntry::kHaltFromLastChargeName, 1},
      {UkmEntry::kIsChargingName, 1}};

  // Log a charging event.
  LogEvent(user_charging_event);

  // Check the output.
  EXPECT_EQ(1, ukm_entry_checker_.NumNewEntriesRecorded(UkmEntry::kEntryName));
  ukm_entry_checker_.ExpectNewEntry(UkmEntry::kEntryName, GURL(""),
                                    user_charging_values);
}

}  // namespace power
}  // namespace ash
