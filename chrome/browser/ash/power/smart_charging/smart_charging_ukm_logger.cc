// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/smart_charging/smart_charging_ukm_logger.h"

#include "chrome/browser/ash/power/smart_charging/user_charging_event.pb.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {
namespace power {

void SmartChargingUkmLogger::LogEvent(
    const UserChargingEvent& user_charging_event) const {
  const ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::builders::SmartCharging ukm_smart_charging(source_id);

  ukm_smart_charging.SetEventId(user_charging_event.event().event_id());
  ukm_smart_charging.SetReason(user_charging_event.event().reason());

  const UserChargingEvent::Features features = user_charging_event.features();

  if (features.has_battery_percentage()) {
    ukm_smart_charging.SetBatteryPercentage(features.battery_percentage());
  }

  if (features.has_time_since_last_charge()) {
    ukm_smart_charging.SetTimeSinceLastCharge(
        features.time_since_last_charge());
  }

  if (features.has_duration_of_last_charge()) {
    ukm_smart_charging.SetDurationOfLastCharge(
        features.duration_of_last_charge());
  }

  if (features.has_battery_percentage_of_last_charge()) {
    ukm_smart_charging.SetBatteryPercentageOfLastCharge(
        features.battery_percentage_of_last_charge());
  }

  if (features.has_battery_percentage_before_last_charge()) {
    ukm_smart_charging.SetBatteryPercentageBeforeLastCharge(
        features.battery_percentage_before_last_charge());
  }

  if (features.has_time_of_the_day()) {
    ukm_smart_charging.SetTimeOfTheDay(features.time_of_the_day());
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

  if (features.has_timezone_difference_from_last_charge()) {
    ukm_smart_charging.SetTimezoneDifferenceFromLastCharge(
        features.timezone_difference_from_last_charge());
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

  if (features.has_duration_recent_video_playing()) {
    ukm_smart_charging.SetDurationRecentVideoPlaying(
        features.duration_recent_video_playing());
  }

  if (features.has_duration_recent_audio_playing()) {
    ukm_smart_charging.SetDurationRecentAudioPlaying(
        features.duration_recent_audio_playing());
  }

  if (features.has_screen_brightness_percent()) {
    ukm_smart_charging.SetScreenBrightnessPercent(
        features.screen_brightness_percent());
  }

  if (features.has_voltage()) {
    ukm_smart_charging.SetVoltage(features.voltage());
  }

  if (features.has_halt_from_last_charge()) {
    ukm_smart_charging.SetHaltFromLastCharge(features.halt_from_last_charge());
  }

  if (features.has_is_charging()) {
    ukm_smart_charging.SetIsCharging(features.is_charging());
  }

  ukm::UkmRecorder* const ukm_recorder = ukm::UkmRecorder::Get();
  ukm_smart_charging.Record(ukm_recorder);
}

}  // namespace power
}  // namespace ash
