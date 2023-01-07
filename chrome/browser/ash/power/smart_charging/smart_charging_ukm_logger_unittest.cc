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

using UkmSmartCharging = ukm::builders::SmartCharging;
using UkmDailySummary = ukm::builders::DailyChargeSummary;
using UkmChargeEvent = ukm::builders::ChargeEventHistory;
using UserChargingEvent = power_manager::UserChargingEvent;
using ChargeHistoryState = power_manager::ChargeHistoryState;

}  // namespace

class SmartChargingUkmLoggerTest : public ChromeRenderViewHostTestHarness {
 public:
  SmartChargingUkmLoggerTest() {}
  ~SmartChargingUkmLoggerTest() override = default;
  SmartChargingUkmLoggerTest(const SmartChargingUkmLoggerTest&) = delete;
  SmartChargingUkmLoggerTest& operator=(const SmartChargingUkmLoggerTest&) =
      delete;

  void LogEvent(const UserChargingEvent& user_charging_event,
                const ChargeHistoryState& charge_history) {
    smart_charging_ukm_logger_.LogEvent(user_charging_event, charge_history,
                                        base::Time::Now());
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

  ChargeHistoryState charge_history;
  auto* charge_event = charge_history.add_charge_event();
  charge_event->set_duration(7200000000);
  charge_event->set_start_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds() -
      2100000000);
  auto* daily_history = charge_history.add_daily_history();
  daily_history->set_utc_midnight(base::Time::Now()
                                      .UTCMidnight()
                                      .ToDeltaSinceWindowsEpoch()
                                      .InMicroseconds() -
                                  172800000000u);
  daily_history->set_time_on_ac(5700000000);
  daily_history->set_time_full_on_ac(1900000000);
  daily_history->set_hold_time_on_ac(4000000000);

  // Constructs the expected output.
  const UkmMetricMap smart_charging_output = {
      {UkmSmartCharging::kEventIdName, 5},
      {UkmSmartCharging::kReasonName, 1},
      {UkmSmartCharging::kBatteryPercentageName, 35},
      {UkmSmartCharging::kTimeSinceLastChargeName, 128},
      {UkmSmartCharging::kDurationOfLastChargeName, 47},
      {UkmSmartCharging::kBatteryPercentageOfLastChargeName, 80},
      {UkmSmartCharging::kBatteryPercentageBeforeLastChargeName, 19},
      {UkmSmartCharging::kTimeOfTheDayName, 620},
      {UkmSmartCharging::kDayOfWeekName, 3},
      {UkmSmartCharging::kDayOfMonthName, 27},
      {UkmSmartCharging::kMonthName, 8},
      {UkmSmartCharging::kTimezoneDifferenceFromLastChargeName, -5},
      {UkmSmartCharging::kDeviceTypeName, 4},
      {UkmSmartCharging::kDeviceModeName, 3},
      {UkmSmartCharging::kNumRecentKeyEventsName, 75},
      {UkmSmartCharging::kNumRecentMouseEventsName, 235},
      {UkmSmartCharging::kNumRecentTouchEventsName, 139},
      {UkmSmartCharging::kNumRecentStylusEventsName, 92},
      {UkmSmartCharging::kDurationRecentVideoPlayingName, 4},
      {UkmSmartCharging::kDurationRecentAudioPlayingName, 8},
      {UkmSmartCharging::kScreenBrightnessPercentName, 23},
      {UkmSmartCharging::kHaltFromLastChargeName, 1},
      {UkmSmartCharging::kIsChargingName, 1}};
  const UkmMetricMap charge_event_output = {
      {UkmChargeEvent::kEventIdName, 5},
      {UkmChargeEvent::kChargeEventHistorySizeName, 1},
      {UkmChargeEvent::kChargeEventHistoryIndexName, 0},
      {UkmChargeEvent::kChargeEventHistoryDurationName, 120},
      {UkmChargeEvent::kChargeEventHistoryStartTimeName, 30}};
  const UkmMetricMap daily_history_output = {
      {UkmDailySummary::kEventIdName, 5},
      {UkmDailySummary::kDailySummarySizeName, 1},
      {UkmDailySummary::kDailySummaryIndexName, 0},
      {UkmDailySummary::kDailySummaryHoldTimeOnAcName, 60},
      {UkmDailySummary::kDailySummaryTimeOnAcName, 90},
      {UkmDailySummary::kDailySummaryTimeFullOnAcName, 30},
      {UkmDailySummary::kDailySummaryNumDaysDistanceName, 2}};

  // Log a charging event.
  LogEvent(user_charging_event, charge_history);

  // Check the outputs.
  EXPECT_EQ(1, ukm_entry_checker_.NumNewEntriesRecorded(
                   UkmSmartCharging::kEntryName));
  EXPECT_EQ(
      1, ukm_entry_checker_.NumNewEntriesRecorded(UkmChargeEvent::kEntryName));
  EXPECT_EQ(
      1, ukm_entry_checker_.NumNewEntriesRecorded(UkmDailySummary::kEntryName));

  ukm_entry_checker_.ExpectNewEntry(UkmSmartCharging::kEntryName, GURL(""),
                                    smart_charging_output);
  ukm_entry_checker_.ExpectNewEntry(UkmChargeEvent::kEntryName, GURL(""),
                                    charge_event_output);
  ukm_entry_checker_.ExpectNewEntry(UkmDailySummary::kEntryName, GURL(""),
                                    daily_history_output);
}

TEST_F(SmartChargingUkmLoggerTest, TestRecordWithMultipleItems) {
  // Constructs an input event.
  UserChargingEvent user_charging_event;
  user_charging_event.mutable_event()->set_event_id(101);
  user_charging_event.mutable_event()->set_reason(
      UserChargingEvent::Event::CHARGER_PLUGGED_IN);

  ChargeHistoryState charge_history;
  for (size_t i = 0; i < 50; ++i) {
    auto* charge_event = charge_history.add_charge_event();
    charge_event->set_duration(7200000000);
    charge_event->set_start_time(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds() -
        1800000000);
  }
  for (size_t i = 0; i < 30; ++i) {
    auto* daily_history = charge_history.add_daily_history();
    daily_history->set_utc_midnight(base::Time::Now()
                                        .UTCMidnight()
                                        .ToDeltaSinceWindowsEpoch()
                                        .InMicroseconds() -
                                    172800000000u);
    daily_history->set_time_on_ac(5400000000);
    daily_history->set_time_full_on_ac(1800000000);
    daily_history->set_hold_time_on_ac(3600000000);
  }

  // Constructs the expected outputs.
  const UkmMetricMap smart_charging_output = {
      {UkmSmartCharging::kEventIdName, 101},
      {UkmSmartCharging::kReasonName, 1}};
  std::vector<UkmMetricMap> charge_event_outputs;
  for (size_t i = 0; i < 50; ++i) {
    charge_event_outputs.push_back(
        {{UkmChargeEvent::kEventIdName, 101},
         {UkmChargeEvent::kChargeEventHistorySizeName, 50},
         {UkmChargeEvent::kChargeEventHistoryIndexName, i},
         {UkmChargeEvent::kChargeEventHistoryDurationName, 120},
         {UkmChargeEvent::kChargeEventHistoryStartTimeName, 30}});
  }
  std::vector<UkmMetricMap> daily_history_outputs;
  for (size_t i = 0; i < 30; ++i) {
    daily_history_outputs.push_back(
        {{UkmDailySummary::kEventIdName, 101},
         {UkmDailySummary::kDailySummarySizeName, 30},
         {UkmDailySummary::kDailySummaryIndexName, i},
         {UkmDailySummary::kDailySummaryHoldTimeOnAcName, 60},
         {UkmDailySummary::kDailySummaryTimeOnAcName, 90},
         {UkmDailySummary::kDailySummaryTimeFullOnAcName, 30},
         {UkmDailySummary::kDailySummaryNumDaysDistanceName, 2}});
  }

  // Log a charging event.
  LogEvent(user_charging_event, charge_history);

  // Check the output.
  EXPECT_EQ(1, ukm_entry_checker_.NumNewEntriesRecorded(
                   UkmSmartCharging::kEntryName));
  EXPECT_EQ(
      50, ukm_entry_checker_.NumNewEntriesRecorded(UkmChargeEvent::kEntryName));
  EXPECT_EQ(30, ukm_entry_checker_.NumNewEntriesRecorded(
                    UkmDailySummary::kEntryName));

  ukm_entry_checker_.ExpectNewEntry(UkmSmartCharging::kEntryName, GURL(""),
                                    smart_charging_output);
  ukm_entry_checker_.ExpectNewEntries(UkmChargeEvent::kEntryName,
                                      charge_event_outputs);
  ukm_entry_checker_.ExpectNewEntries(UkmDailySummary::kEntryName,
                                      daily_history_outputs);
}

}  // namespace power
}  // namespace ash
