// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/machine_learning/user_settings_event_logger.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/shell.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/power/power_status.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {
namespace ml {

using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::SecurityType;

// static
UserSettingsEventLogger* UserSettingsEventLogger::instance_ = nullptr;

// static
void UserSettingsEventLogger::CreateInstance() {
  DCHECK(!instance_);
  instance_ = new UserSettingsEventLogger();
}

// static
void UserSettingsEventLogger::DeleteInstance() {
  delete instance_;
  instance_ = nullptr;
}

// static
UserSettingsEventLogger* UserSettingsEventLogger::Get() {
  return instance_;
}

UserSettingsEventLogger::UserSettingsEventLogger()
    : presenting_session_count_(0),
      is_recently_presenting_(false),
      is_recently_fullscreen_(false),
      used_cellular_in_session_(false),
      is_playing_audio_(false),
      is_playing_video_(false),
      clock_(base::DefaultClock::GetInstance()) {
  auto* audio_handler = CrasAudioHandler::Get();
  DCHECK(audio_handler);

  audio_handler->AddAudioObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->video_detector()->AddObserver(this);

  volume_ = audio_handler->GetOutputVolumePercent();
}

UserSettingsEventLogger::~UserSettingsEventLogger() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->video_detector()->RemoveObserver(this);
}

void UserSettingsEventLogger::LogNetworkUkmEvent(
    const NetworkStateProperties& network) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();
  auto* const features = settings_event.mutable_features();

  if (network.type == NetworkType::kWiFi) {
    event->set_setting_id(UserSettingsEvent::Event::WIFI);
    const auto& wifi_state = network.type_state->get_wifi();
    features->set_signal_strength(wifi_state->signal_strength);
    features->set_has_wifi_security(wifi_state->security !=
                                    SecurityType::kNone);
  } else if (network.type == NetworkType::kCellular) {
    event->set_setting_id(UserSettingsEvent::Event::CELLULAR);
    features->set_signal_strength(
        network.type_state->get_cellular()->signal_strength);
    features->set_used_cellular_in_session(used_cellular_in_session_);
    used_cellular_in_session_ = true;
  } else {
    // We are not interested in other types of networks.
    return;
  }
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);

  PopulateSharedFeatures(&settings_event);
  SendToUkmAndAppList(settings_event);
}

void UserSettingsEventLogger::LogNightLightUkmEvent(const bool enabled) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();
  auto* const features = settings_event.mutable_features();

  event->set_setting_id(UserSettingsEvent::Event::NIGHT_LIGHT);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);
  // Convert the setting state to an int. Some settings have multiple states, so
  // all setting states are stored as ints.
  event->set_previous_value(!enabled ? 1 : 0);
  event->set_current_value(enabled ? 1 : 0);

  const auto* night_light_controller = Shell::Get()->night_light_controller();
  const auto schedule_type = night_light_controller->GetScheduleType();
  const bool has_night_light_schedule =
      schedule_type != NightLightController::ScheduleType::kNone;
  UMA_HISTOGRAM_BOOLEAN("Ash.Shelf.UkmLogger.HasNightLightSchedule",
                        has_night_light_schedule);
  features->set_has_night_light_schedule(has_night_light_schedule);
  features->set_is_after_sunset(
      night_light_controller->IsNowWithinSunsetSunrise());

  PopulateSharedFeatures(&settings_event);
  SendToUkmAndAppList(settings_event);
}

void UserSettingsEventLogger::LogQuietModeUkmEvent(const bool enabled) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();

  event->set_setting_id(UserSettingsEvent::Event::DO_NOT_DISTURB);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);
  // Convert the setting state to an int. Some settings have multiple states, so
  // all setting states are stored as ints.
  event->set_previous_value(!enabled ? 1 : 0);
  event->set_current_value(enabled ? 1 : 0);

  settings_event.mutable_features()->set_is_recently_presenting(
      is_recently_presenting_);

  PopulateSharedFeatures(&settings_event);
  SendToUkmAndAppList(settings_event);
}

void UserSettingsEventLogger::LogAccessibilityUkmEvent(
    UserSettingsEvent::Event::AccessibilityId id,
    bool enabled) {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();

  event->set_setting_id(UserSettingsEvent::Event::ACCESSIBILITY);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);
  // Convert the setting state to an int. Some settings have multiple states, so
  // all setting states are stored as ints.
  event->set_previous_value(!enabled ? 1 : 0);
  event->set_current_value(enabled ? 1 : 0);
  event->set_accessibility_id(id);

  PopulateSharedFeatures(&settings_event);
  SendToUkmAndAppList(settings_event);
}

void UserSettingsEventLogger::OnOutputNodeVolumeChanged(uint64_t /*node*/,
                                                        const int volume) {
  if (!volume_timer_.IsRunning()) {
    volume_before_user_change_ = volume_;
  }
  volume_ = volume;
  volume_before_mute_ = volume;
  volume_timer_.Start(FROM_HERE, kSliderDelay, this,
                      &UserSettingsEventLogger::OnVolumeTimerEnded);
}

void UserSettingsEventLogger::OnOutputMuteChanged(const bool mute_on) {
  if (!volume_timer_.IsRunning()) {
    volume_before_user_change_ = volume_;
  }
  volume_ = mute_on ? 0 : volume_before_mute_;
  volume_timer_.Start(FROM_HERE, kSliderDelay, this,
                      &UserSettingsEventLogger::OnVolumeTimerEnded);
}

void UserSettingsEventLogger::OnVolumeTimerEnded() {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();

  event->set_setting_id(UserSettingsEvent::Event::VOLUME);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);
  event->set_previous_value(volume_before_user_change_);
  event->set_current_value(volume_);

  PopulateSharedFeatures(&settings_event);
  SendToUkmAndAppList(settings_event);
}

void UserSettingsEventLogger::OnOutputStarted() {
  is_playing_audio_ = true;
}

void UserSettingsEventLogger::OnOutputStopped() {
  is_playing_audio_ = false;
}

void UserSettingsEventLogger::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  const int new_brightness = std::floor(change.percent());
  if (change.cause() ==
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST) {
    if (!brightness_timer_.IsRunning()) {
      brightness_before_user_change_ = brightness_;
    }
    brightness_after_user_change_ = new_brightness;
    // Keep starting the timer until there is a pause in brightness activity.
    // Then only one event will be logged to summarise that activity.
    brightness_timer_.Start(FROM_HERE, kSliderDelay, this,
                            &UserSettingsEventLogger::OnBrightnessTimerEnded);
  }
  brightness_ = new_brightness;
}

void UserSettingsEventLogger::OnBrightnessTimerEnded() {
  UserSettingsEvent settings_event;
  auto* const event = settings_event.mutable_event();

  event->set_setting_id(UserSettingsEvent::Event::BRIGHTNESS);
  event->set_setting_type(UserSettingsEvent::Event::QUICK_SETTINGS);
  if (brightness_before_user_change_.has_value()) {
    event->set_previous_value(brightness_before_user_change_.value());
  }
  event->set_current_value(brightness_after_user_change_);

  settings_event.mutable_features()->set_is_recently_fullscreen(
      is_recently_fullscreen_);

  PopulateSharedFeatures(&settings_event);
  SendToUkmAndAppList(settings_event);
}

void UserSettingsEventLogger::OnCastingSessionStartedOrStopped(
    const bool started) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (started) {
    ++presenting_session_count_;
    is_recently_presenting_ = true;
    presenting_timer_.Stop();
  } else {
    --presenting_session_count_;
    DCHECK_GE(presenting_session_count_, 0);
    if (presenting_session_count_ == 0) {
      presenting_timer_.Start(FROM_HERE, base::Minutes(5), this,
                              &UserSettingsEventLogger::OnPresentingTimerEnded);
    }
  }
}

void UserSettingsEventLogger::OnPresentingTimerEnded() {
  is_recently_presenting_ = false;
}

void UserSettingsEventLogger::OnFullscreenStateChanged(
    const bool is_fullscreen,
    aura::Window* /*container*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_fullscreen) {
    is_recently_fullscreen_ = true;
    fullscreen_timer_.Stop();
  } else {
    fullscreen_timer_.Start(FROM_HERE, base::Minutes(5), this,
                            &UserSettingsEventLogger::OnFullscreenTimerEnded);
  }
}

void UserSettingsEventLogger::OnFullscreenTimerEnded() {
  is_recently_fullscreen_ = false;
}

void UserSettingsEventLogger::OnVideoStateChanged(
    const VideoDetector::State state) {
  is_playing_video_ = (state != VideoDetector::State::NOT_PLAYING);
}

void UserSettingsEventLogger::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void UserSettingsEventLogger::PopulateSharedFeatures(
    UserSettingsEvent* settings_event) {
  auto* features = settings_event->mutable_features();

  // Set time features.
  base::Time::Exploded now;
  clock_->Now().LocalExplode(&now);
  features->set_hour_of_day(now.hour);
  features->set_day_of_week(
      static_cast<UserSettingsEvent::Features::DayOfWeek>(now.day_of_week));

  // Set power features.
  if (PowerStatus::IsInitialized()) {
    const auto* power_status = PowerStatus::Get();
    features->set_battery_percentage(power_status->GetRoundedBatteryPercent());
    features->set_is_charging(power_status->IsLinePowerConnected() ||
                              power_status->IsMainsChargerConnected() ||
                              power_status->IsUsbChargerConnected());
  }

  // Set activity features.
  features->set_is_playing_audio(is_playing_audio_);
  features->set_is_playing_video(is_playing_video_);

  // Set orientation features.
  features->set_device_mode(
      Shell::Get()->tablet_mode_controller()->InTabletMode()
          ? UserSettingsEvent::Features::TABLET_MODE
          : UserSettingsEvent::Features::CLAMSHELL_MODE);
  const auto orientation =
      Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
  if (chromeos::IsLandscapeOrientation(orientation)) {
    features->set_device_orientation(UserSettingsEvent::Features::LANDSCAPE);
  } else if (chromeos::IsPortraitOrientation(orientation)) {
    features->set_device_orientation(UserSettingsEvent::Features::PORTRAIT);
  }
}

void UserSettingsEventLogger::SendToUkmAndAppList(
    const UserSettingsEvent& settings_event) {
  const ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::builders::UserSettingsEvent ukm_event(source_id);

  const UserSettingsEvent::Event& event = settings_event.event();
  const UserSettingsEvent::Features& features = settings_event.features();

  if (event.has_event_id())
    ukm_event.SetEventId(event.event_id());
  if (event.has_setting_id())
    ukm_event.SetSettingId(event.setting_id());
  if (event.has_setting_type())
    ukm_event.SetSettingType(event.setting_type());
  if (event.has_previous_value())
    ukm_event.SetPreviousValue(event.previous_value());
  if (event.has_current_value())
    ukm_event.SetCurrentValue(event.current_value());
  if (event.has_accessibility_id())
    ukm_event.SetAccessibilityId(event.accessibility_id());

  if (features.has_hour_of_day())
    ukm_event.SetHourOfDay(features.hour_of_day());
  if (features.has_day_of_week())
    ukm_event.SetDayOfWeek(features.day_of_week());
  if (features.has_battery_percentage())
    ukm_event.SetBatteryPercentage(features.battery_percentage());
  if (features.has_is_charging())
    ukm_event.SetIsCharging(features.is_charging());
  if (features.has_is_playing_audio())
    ukm_event.SetIsPlayingAudio(features.is_playing_audio());
  if (features.has_is_playing_video())
    ukm_event.SetIsPlayingVideo(features.is_playing_video());
  if (features.has_device_mode())
    ukm_event.SetDeviceMode(features.device_mode());
  if (features.has_device_orientation())
    ukm_event.SetDeviceOrientation(features.device_orientation());

  if (features.has_is_recently_presenting())
    ukm_event.SetIsRecentlyPresenting(features.is_recently_presenting());
  if (features.has_is_recently_fullscreen())
    ukm_event.SetIsRecentlyFullscreen(features.is_recently_fullscreen());
  if (features.has_signal_strength())
    ukm_event.SetSignalStrength(features.signal_strength());
  if (features.has_has_wifi_security())
    ukm_event.SetHasWifiSecurity(features.has_wifi_security());
  if (features.has_used_cellular_in_session())
    ukm_event.SetUsedCellularInSession(features.used_cellular_in_session());
  if (features.has_has_night_light_schedule())
    ukm_event.SetHasNightLightSchedule(features.has_night_light_schedule());
  if (features.has_is_after_sunset())
    ukm_event.SetIsAfterSunset(features.is_after_sunset());

  ukm::UkmRecorder* const ukm_recorder = ukm::UkmRecorder::Get();
  ukm_event.Record(ukm_recorder);

  // Also log in browser side for other usage (CrOSActionRecorder for now).
  AppListClient* app_list_client =
      Shell::Get()->app_list_controller()->GetClient();
  if (app_list_client) {
    const std::string setting_name =
        UserSettingsEvent_Event_SettingId_Name(event.setting_id());
    app_list_client->OnQuickSettingsChanged(
        setting_name, {{"SettingType", static_cast<int>(event.setting_type())},
                       {"PreviousValue", event.previous_value()},
                       {"CurrentValue", event.current_value()},
                       {"SettingId", static_cast<int>(event.setting_id())}});
  }
}

}  // namespace ml
}  // namespace ash
