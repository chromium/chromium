// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/smart_charging/smart_charging_ukm_logger.h"

#include "chromeos/dbus/power_manager/charge_history_state.pb.h"
#include "chromeos/dbus/power_manager/user_charging_event.pb.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {
namespace power {

namespace {
constexpr int kBucketSize = 15;
using UserChargingEvent = power_manager::UserChargingEvent;
using ChargeHistoryState = power_manager::ChargeHistoryState;
}  // namespace

void SmartChargingUkmLogger::LogEvent(
    const UserChargingEvent& user_charging_event,
    const ChargeHistoryState& charge_history,
    base::Time time_of_call) const {
  const ukm::SourceId source_id = ukm::NoURLSourceId();
  // Log UserChargingEvent and ChargeHistoryState using a same SourceID and
  // |event_id|.
  ukm::builders::SmartCharging ukm_smart_charging(source_id);

  ukm_smart_charging.SetEventId(user_charging_event.event().event_id());
  ukm_smart_charging.SetReason(user_charging_event.event().reason());

  const UserChargingEvent::Features features = user_charging_event.features();

  if (features.has_battery_percentage()) {
    ukm_smart_charging.SetBatteryPercentage(features.battery_percentage());
  }

  if (features.has_time_since_last_charge_minutes()) {
    ukm_smart_charging.SetTimeSinceLastCharge(
        features.time_since_last_charge_minutes());
  }

  if (features.has_duration_of_last_charge_minutes()) {
    ukm_smart_charging.SetDurationOfLastCharge(
        features.duration_of_last_charge_minutes());
  }

  if (features.has_battery_percentage_of_last_charge()) {
    ukm_smart_charging.SetBatteryPercentageOfLastCharge(
        features.battery_percentage_of_last_charge());
  }

  if (features.has_battery_percentage_before_last_charge()) {
    ukm_smart_charging.SetBatteryPercentageBeforeLastCharge(
        features.battery_percentage_before_last_charge());
  }

  if (features.has_time_of_the_day_minutes()) {
    ukm_smart_charging.SetTimeOfTheDay(features.time_of_the_day_minutes());
  }

  if (features.has_day_of_week()) {
    ukm_smart_charging.SetDayOfWeek(features.day_of_week());
  }

  if (features.has_day_of_month()) {
    ukm_smart_charging.SetDayOfMonth(features.day_of_month());
  }

  if (features.has_month()) {
    ukm_smart_charging.SetMonth(features.month());
  }

  if (features.has_timezone_difference_from_last_charge_hours()) {
    ukm_smart_charging.SetTimezoneDifferenceFromLastCharge(
        features.timezone_difference_from_last_charge_hours());
  }

  if (features.has_device_type()) {
    ukm_smart_charging.SetDeviceType(features.device_type());
  }

  if (features.has_device_mode()) {
    ukm_smart_charging.SetDeviceMode(features.device_mode());
  }

  if (features.has_num_recent_key_events()) {
    ukm_smart_charging.SetNumRecentKeyEvents(features.num_recent_key_events());
  }

  if (features.has_num_recent_mouse_events()) {
    ukm_smart_charging.SetNumRecentMouseEvents(
        features.num_recent_mouse_events());
  }

  if (features.has_num_recent_touch_events()) {
    ukm_smart_charging.SetNumRecentTouchEvents(
        features.num_recent_touch_events());
  }

  if (features.has_num_recent_stylus_events()) {
    ukm_smart_charging.SetNumRecentStylusEvents(
        features.num_recent_stylus_events());
  }

  if (features.has_duration_recent_video_playing_minutes()) {
    ukm_smart_charging.SetDurationRecentVideoPlaying(
        features.duration_recent_video_playing_minutes());
  }

  if (features.has_duration_recent_audio_playing_minutes()) {
    ukm_smart_charging.SetDurationRecentAudioPlaying(
        features.duration_recent_audio_playing_minutes());
  }

  if (features.has_screen_brightness_percent()) {
    ukm_smart_charging.SetScreenBrightnessPercent(
        features.screen_brightness_percent());
  }

  if (features.has_voltage_mv()) {
    ukm_smart_charging.SetVoltage(features.voltage_mv());
  }

  if (features.has_halt_from_last_charge()) {
    ukm_smart_charging.SetHaltFromLastCharge(features.halt_from_last_charge());
  }

  if (features.has_is_charging()) {
    ukm_smart_charging.SetIsCharging(features.is_charging());
  }

  ukm_smart_charging.Record(ukm::UkmRecorder::Get());

  for (int i = 0; i < charge_history.charge_event_size(); ++i) {
    ukm::builders::ChargeEventHistory charge_event_history(source_id);
    charge_event_history.SetEventId(user_charging_event.event().event_id());
    charge_event_history.SetChargeEventHistoryIndex(i);
    charge_event_history.SetChargeEventHistorySize(
        charge_history.charge_event_size());
    if (charge_history.charge_event(i).has_start_time()) {
      charge_event_history.SetChargeEventHistoryStartTime(
          ukm::GetLinearBucketMin(
              static_cast<int64_t>(
                  (time_of_call -
                   base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
                       charge_history.charge_event(i).start_time())))
                      .InMinutes()),
              kBucketSize));
    }
    if (charge_history.charge_event(i).has_duration()) {
      charge_event_history.SetChargeEventHistoryDuration(
          ukm::GetLinearBucketMin(
              static_cast<int64_t>(
                  base::Microseconds(charge_history.charge_event(i).duration())
                      .InMinutes()),
              kBucketSize));
    }

    charge_event_history.Record(ukm::UkmRecorder::Get());
  }

  for (int i = 0; i < charge_history.daily_history_size(); ++i) {
    ukm::builders::DailyChargeSummary daily_summary(source_id);
    daily_summary.SetEventId(user_charging_event.event().event_id());
    daily_summary.SetDailySummaryIndex(i);
    daily_summary.SetDailySummarySize(charge_history.daily_history_size());

    if (charge_history.daily_history(i).has_utc_midnight()) {
      daily_summary.SetDailySummaryNumDaysDistance(
          (time_of_call.UTCMidnight() -
           base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
               charge_history.daily_history(i).utc_midnight())))
              .InHours() /
          24);
    }
    if (charge_history.daily_history(i).has_time_on_ac()) {
      daily_summary.SetDailySummaryTimeOnAc(ukm::GetLinearBucketMin(
          static_cast<int64_t>(
              base::Microseconds(charge_history.daily_history(i).time_on_ac())
                  .InMinutes()),
          kBucketSize));
    }
    if (charge_history.daily_history(i).has_time_full_on_ac()) {
      daily_summary.SetDailySummaryTimeFullOnAc(ukm::GetLinearBucketMin(
          static_cast<int64_t>(
              base::Microseconds(
                  charge_history.daily_history(i).time_full_on_ac())
                  .InMinutes()),
          kBucketSize));
    }
    if (charge_history.daily_history(i).has_hold_time_on_ac()) {
      daily_summary.SetDailySummaryHoldTimeOnAc(ukm::GetLinearBucketMin(
          static_cast<int64_t>(
              base::Microseconds(
                  charge_history.daily_history(i).hold_time_on_ac())
                  .InMinutes()),
          kBucketSize));
    }

    daily_summary.Record(ukm::UkmRecorder::Get());
  }
}

}  // namespace power
}  // namespace ash
