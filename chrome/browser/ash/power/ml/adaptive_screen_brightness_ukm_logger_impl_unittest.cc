// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/adaptive_screen_brightness_ukm_logger_impl.h"

#include <memory>

#include "chrome/browser/ash/power/ml/screen_brightness_event.pb.h"
#include "chrome/browser/ash/power/ml/user_activity_ukm_logger_impl.h"
#include "chrome/browser/ui/tabs/tab_ukm_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace ml {

class AdaptiveScreenBrightnessUkmLoggerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AdaptiveScreenBrightnessUkmLoggerImplTest() {}

  AdaptiveScreenBrightnessUkmLoggerImplTest(
      const AdaptiveScreenBrightnessUkmLoggerImplTest&) = delete;
  AdaptiveScreenBrightnessUkmLoggerImplTest& operator=(
      const AdaptiveScreenBrightnessUkmLoggerImplTest&) = delete;

  void LogActivity(const ScreenBrightnessEvent& screen_brightness_event,
                   ukm::SourceId tab_id,
                   bool has_form_entry) {
    screen_brightness_ukm_logger_impl_.LogActivity(screen_brightness_event,
                                                   tab_id, has_form_entry);
  }

 protected:
  UkmEntryChecker ukm_entry_checker_;

 private:
  AdaptiveScreenBrightnessUkmLoggerImpl screen_brightness_ukm_logger_impl_;
};

TEST_F(AdaptiveScreenBrightnessUkmLoggerImplTest, Basic) {
  // Construct the input event.
  ScreenBrightnessEvent screen_brightness_event;
  ScreenBrightnessEvent::Features* const features =
      screen_brightness_event.mutable_features();

  ScreenBrightnessEvent::Features::ActivityData* const activity_data =
      features->mutable_activity_data();
  activity_data->set_num_recent_mouse_events(12);
  activity_data->set_num_recent_key_events(0);
  activity_data->set_num_recent_stylus_events(34);
  activity_data->set_num_recent_touch_events(56);
  activity_data->set_last_activity_time_sec(22);
  activity_data->set_recent_time_active_sec(33);
  activity_data->set_is_video_playing(1);

  ScreenBrightnessEvent::Features::EnvData* const env_data =
      features->mutable_env_data();
  env_data->set_on_battery(1);
  env_data->set_battery_percent(96.0);
  env_data->set_device_mode(ScreenBrightnessEvent::Features::EnvData::LAPTOP);
  env_data->set_night_light_temperature_percent(40);
  env_data->set_previous_brightness(10);

  ScreenBrightnessEvent::Features::AccessibilityData* const accessibility_data =
      features->mutable_accessibility_data();
  accessibility_data->set_is_magnifier_enabled(1);
  accessibility_data->set_is_high_contrast_enabled(1);
  accessibility_data->set_is_large_cursor_enabled(1);
  accessibility_data->set_is_virtual_keyboard_enabled(1);
  accessibility_data->set_is_spoken_feedback_enabled(1);
  accessibility_data->set_is_select_to_speak_enabled(1);
  accessibility_data->set_is_mono_audio_enabled(1);
  accessibility_data->set_is_caret_highlight_enabled(1);
  accessibility_data->set_is_cursor_highlight_enabled(1);
  accessibility_data->set_is_focus_highlight_enabled(1);
  accessibility_data->set_is_braille_display_connected(1);
  accessibility_data->set_is_autoclick_enabled(1);
  accessibility_data->set_is_switch_access_enabled(1);

  ScreenBrightnessEvent::Event* const event =
      screen_brightness_event.mutable_event();
  event->set_brightness(31);
  event->set_reason(ScreenBrightnessEvent::Event::USER_UP);
  event->set_time_since_last_event_sec(9);

  LogActivity(screen_brightness_event, 5 /* tab_id */,
              true /* has_form_entry */);

  EXPECT_EQ(1, ukm_entry_checker_.NumNewEntriesRecorded(
                   ukm::builders::ScreenBrightness::kEntryName));

  // Construct the expected output logs.
  const UkmMetricMap screen_brightness_values = {
      {ukm::builders::ScreenBrightness::kBatteryPercentName, 95},
      {ukm::builders::ScreenBrightness::kBrightnessName, 31},
      {ukm::builders::ScreenBrightness::kDeviceModeName, 2},  // LAPTOP
      {ukm::builders::ScreenBrightness::kIsAutoclickEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsBrailleDisplayConnectedName, 1},
      {ukm::builders::ScreenBrightness::kIsCaretHighlightEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsCursorHighlightEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsFocusHighlightEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsHighContrastEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsLargeCursorEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsMagnifierEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsMonoAudioEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsSelectToSpeakEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsSpokenFeedbackEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsSwitchAccessEnabledName, 1},
      {ukm::builders::ScreenBrightness::kIsVideoPlayingName, 1},
      {ukm::builders::ScreenBrightness::kIsVirtualKeyboardEnabledName, 1},
      {ukm::builders::ScreenBrightness::kLastActivityTimeSecName, 22},
      {ukm::builders::ScreenBrightness::kNightLightTemperaturePercentName, 40},
      {ukm::builders::ScreenBrightness::kNumRecentKeyEventsName, 0},
      {ukm::builders::ScreenBrightness::kNumRecentMouseEventsName, 12},
      {ukm::builders::ScreenBrightness::kNumRecentStylusEventsName, 34},
      {ukm::builders::ScreenBrightness::kNumRecentTouchEventsName, 56},
      {ukm::builders::ScreenBrightness::kOnBatteryName, 1},
      {ukm::builders::ScreenBrightness::kPreviousBrightnessName, 10},
      {ukm::builders::ScreenBrightness::kReasonName, 1},
      {ukm::builders::ScreenBrightness::kRecentTimeActiveSecName, 33},
      {ukm::builders::ScreenBrightness::kTimeSinceLastEventSecName, 9},
  };

  ukm_entry_checker_.ExpectNewEntry(ukm::builders::ScreenBrightness::kEntryName,
                                    GURL(""), screen_brightness_values);

  EXPECT_EQ(1, ukm_entry_checker_.NumNewEntriesRecorded(
                   ukm::builders::UserActivityId::kEntryName));
  const UkmMetricMap user_activity_id_values = {
      {ukm::builders::UserActivityId::kHasFormEntryName, 1},
  };
  ukm_entry_checker_.ExpectNewEntry(ukm::builders::UserActivityId::kEntryName,
                                    GURL(""), user_activity_id_values);
}

TEST_F(AdaptiveScreenBrightnessUkmLoggerImplTest, AccessibilityOff) {
  // Construct the input event.
  ScreenBrightnessEvent screen_brightness_event;
  ScreenBrightnessEvent::Features* const features =
      screen_brightness_event.mutable_features();

  ScreenBrightnessEvent::Features::ActivityData* const activity_data =
      features->mutable_activity_data();
  activity_data->set_is_video_playing(0);

  ScreenBrightnessEvent::Features::AccessibilityData* const accessibility_data =
      features->mutable_accessibility_data();
  accessibility_data->set_is_magnifier_enabled(0);
  accessibility_data->set_is_high_contrast_enabled(0);
  accessibility_data->set_is_large_cursor_enabled(0);
  accessibility_data->set_is_virtual_keyboard_enabled(0);
  accessibility_data->set_is_spoken_feedback_enabled(0);
  accessibility_data->set_is_select_to_speak_enabled(0);
  accessibility_data->set_is_mono_audio_enabled(0);
  accessibility_data->set_is_caret_highlight_enabled(0);
  accessibility_data->set_is_cursor_highlight_enabled(0);
  accessibility_data->set_is_focus_highlight_enabled(0);
  accessibility_data->set_is_braille_display_connected(0);
  accessibility_data->set_is_autoclick_enabled(0);
  accessibility_data->set_is_switch_access_enabled(0);

  ScreenBrightnessEvent::Event* const event =
      screen_brightness_event.mutable_event();
  event->set_brightness(0);

  LogActivity(screen_brightness_event, ukm::kInvalidSourceId,
              false /* has_form_entry */);

  EXPECT_EQ(1, ukm_entry_checker_.NumNewEntriesRecorded(
                   ukm::builders::ScreenBrightness::kEntryName));

  // Construct the expected output logs.
  const UkmMetricMap screen_brightness_values = {
      {ukm::builders::ScreenBrightness::kBrightnessName, 0},
      {ukm::builders::ScreenBrightness::kIsAutoclickEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsBrailleDisplayConnectedName, 0},
      {ukm::builders::ScreenBrightness::kIsCaretHighlightEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsCursorHighlightEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsFocusHighlightEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsHighContrastEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsLargeCursorEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsMagnifierEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsMonoAudioEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsSelectToSpeakEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsSpokenFeedbackEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsSwitchAccessEnabledName, 0},
      {ukm::builders::ScreenBrightness::kIsVideoPlayingName, 0},
      {ukm::builders::ScreenBrightness::kIsVirtualKeyboardEnabledName, 0},
  };

  ukm_entry_checker_.ExpectNewEntry(ukm::builders::ScreenBrightness::kEntryName,
                                    GURL(""), screen_brightness_values);
}

}  // namespace ml
}  // namespace power
}  // namespace ash
