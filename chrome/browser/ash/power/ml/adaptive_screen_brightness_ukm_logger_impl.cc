// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/adaptive_screen_brightness_ukm_logger_impl.h"

#include <array>
#include <cmath>

#include "base/check.h"
#include "chrome/browser/ash/power/ml/screen_brightness_event.pb.h"
#include "chrome/browser/ash/power/ml/user_activity_ukm_logger_helpers.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {
namespace power {
namespace ml {

namespace {

constexpr int kSecondsPerHour = 3600;

constexpr std::array<Bucket, 1> kBatteryPercentBuckets = {{{100, 5}}};

constexpr std::array<Bucket, 3> kUserInputEventBuckets = {
    {{100, 1}, {1000, 100}, {10000, 1000}}};

constexpr std::array<Bucket, 5> kRecentTimeActiveBuckets = {
    {{60, 1}, {600, 60}, {1200, 300}, {3600, 600}, {18000, 1800}}};

constexpr std::array<Bucket, 2> kTimeSinceLastEventBuckets = {
    {{60, 1}, {600, 60}}};

}  // namespace

AdaptiveScreenBrightnessUkmLoggerImpl::
    ~AdaptiveScreenBrightnessUkmLoggerImpl() = default;

void AdaptiveScreenBrightnessUkmLoggerImpl::LogActivity(
    const ScreenBrightnessEvent& screen_brightness_event,
    ukm::SourceId tab_id,
    bool has_form_entry) {
  const ukm::SourceId source_id = ukm::NoURLSourceId();
  ukm::builders::ScreenBrightness ukm_screen_brightness(source_id);
  ukm_screen_brightness.SetSequenceId(next_sequence_id_++);

  const ScreenBrightnessEvent_Features features =
      screen_brightness_event.features();
  const ScreenBrightnessEvent_Features_ActivityData activity_data =
      features.activity_data();

  if (activity_data.has_time_of_day_sec()) {
    ukm_screen_brightness.SetHourOfDay(
        std::floor(activity_data.time_of_day_sec() / kSecondsPerHour));
  }

  if (activity_data.has_day_of_week()) {
    ukm_screen_brightness.SetDayOfWeek(activity_data.day_of_week());
  }

  if (activity_data.has_num_recent_mouse_events()) {
    ukm_screen_brightness.SetNumRecentMouseEvents(Bucketize(
        activity_data.num_recent_mouse_events(), kUserInputEventBuckets));
  }

  if (activity_data.has_num_recent_key_events()) {
    ukm_screen_brightness.SetNumRecentKeyEvents(Bucketize(
        activity_data.num_recent_key_events(), kUserInputEventBuckets));
  }

  if (activity_data.has_num_recent_stylus_events()) {
    ukm_screen_brightness.SetNumRecentStylusEvents(Bucketize(
        activity_data.num_recent_stylus_events(), kUserInputEventBuckets));
  }

  if (activity_data.has_num_recent_touch_events()) {
    ukm_screen_brightness.SetNumRecentTouchEvents(Bucketize(
        activity_data.num_recent_touch_events(), kUserInputEventBuckets));
  }

  if (activity_data.has_last_activity_time_sec()) {
    ukm_screen_brightness.SetLastActivityTimeSec(
        activity_data.last_activity_time_sec());
  }

  if (activity_data.has_recent_time_active_sec()) {
    ukm_screen_brightness.SetRecentTimeActiveSec(Bucketize(
        activity_data.recent_time_active_sec(), kRecentTimeActiveBuckets));
  }

  if (activity_data.has_is_video_playing()) {
    ukm_screen_brightness.SetIsVideoPlaying(activity_data.is_video_playing());
  }

  const ScreenBrightnessEvent_Features_EnvData env_data = features.env_data();

  if (env_data.has_on_battery()) {
    ukm_screen_brightness.SetOnBattery(env_data.on_battery());
  }

  if (env_data.has_battery_percent()) {
    ukm_screen_brightness.SetBatteryPercent(Bucketize(
        std::floor(env_data.battery_percent()), kBatteryPercentBuckets));
  }

  if (env_data.has_device_mode()) {
    ukm_screen_brightness.SetDeviceMode(env_data.device_mode());
  }

  if (env_data.has_night_light_temperature_percent()) {
    ukm_screen_brightness.SetNightLightTemperaturePercent(
        env_data.night_light_temperature_percent());
  }

  if (env_data.has_previous_brightness()) {
    ukm_screen_brightness.SetPreviousBrightness(env_data.previous_brightness());
  }

  const ScreenBrightnessEvent_Features_AccessibilityData accessibility_data =
      features.accessibility_data();

  if (accessibility_data.has_is_magnifier_enabled()) {
    ukm_screen_brightness.SetIsMagnifierEnabled(
        accessibility_data.is_magnifier_enabled());
  }

  if (accessibility_data.has_is_high_contrast_enabled()) {
    ukm_screen_brightness.SetIsHighContrastEnabled(
        accessibility_data.is_high_contrast_enabled());
  }

  if (accessibility_data.has_is_large_cursor_enabled()) {
    ukm_screen_brightness.SetIsLargeCursorEnabled(
        accessibility_data.is_large_cursor_enabled());
  }

  if (accessibility_data.has_is_virtual_keyboard_enabled()) {
    ukm_screen_brightness.SetIsVirtualKeyboardEnabled(
        accessibility_data.is_virtual_keyboard_enabled());
  }

  if (accessibility_data.has_is_spoken_feedback_enabled()) {
    ukm_screen_brightness.SetIsSpokenFeedbackEnabled(
        accessibility_data.is_spoken_feedback_enabled());
  }

  if (accessibility_data.has_is_select_to_speak_enabled()) {
    ukm_screen_brightness.SetIsSelectToSpeakEnabled(
        accessibility_data.is_select_to_speak_enabled());
  }

  if (accessibility_data.has_is_mono_audio_enabled()) {
    ukm_screen_brightness.SetIsMonoAudioEnabled(
        accessibility_data.is_mono_audio_enabled());
  }

  if (accessibility_data.has_is_caret_highlight_enabled()) {
    ukm_screen_brightness.SetIsCaretHighlightEnabled(
        accessibility_data.is_caret_highlight_enabled());
  }

  if (accessibility_data.has_is_cursor_highlight_enabled()) {
    ukm_screen_brightness.SetIsCursorHighlightEnabled(
        accessibility_data.is_cursor_highlight_enabled());
  }

  if (accessibility_data.has_is_focus_highlight_enabled()) {
    ukm_screen_brightness.SetIsFocusHighlightEnabled(
        accessibility_data.is_focus_highlight_enabled());
  }

  if (accessibility_data.has_is_braille_display_connected()) {
    ukm_screen_brightness.SetIsBrailleDisplayConnected(
        accessibility_data.is_braille_display_connected());
  }

  if (accessibility_data.has_is_autoclick_enabled()) {
    ukm_screen_brightness.SetIsAutoclickEnabled(
        accessibility_data.is_autoclick_enabled());
  }

  if (accessibility_data.has_is_switch_access_enabled()) {
    ukm_screen_brightness.SetIsSwitchAccessEnabled(
        accessibility_data.is_switch_access_enabled());
  }

  const ScreenBrightnessEvent_Event event = screen_brightness_event.event();

  DCHECK(event.has_brightness());
  ukm_screen_brightness.SetBrightness(event.brightness());

  if (event.has_reason()) {
    ukm_screen_brightness.SetReason(event.reason());
  }

  if (event.has_time_since_last_event_sec()) {
    ukm_screen_brightness.SetTimeSinceLastEventSec(Bucketize(
        event.time_since_last_event_sec(), kTimeSinceLastEventBuckets));
  }

  ukm::UkmRecorder* const ukm_recorder = ukm::UkmRecorder::Get();
  ukm_screen_brightness.Record(ukm_recorder);

  if (tab_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::UserActivityId user_activity_id(tab_id);
  user_activity_id.SetActivityId(source_id).SetHasFormEntry(has_form_entry);
  user_activity_id.Record(ukm_recorder);
}

}  // namespace ml
}  // namespace power
}  // namespace ash
