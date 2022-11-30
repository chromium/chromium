// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_
#define ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_

#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/shell_observer.h"
#include "ash/system/machine_learning/user_settings_event.pb.h"
#include "ash/wm/video_detector.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace power_manager {
class BacklightBrightnessChange;
}

namespace ash {
namespace ml {

static constexpr base::TimeDelta kSliderDelay = base::Seconds(1);

// This class handles logging for settings changes that are initiated by the
// user from the quick settings tray. Exported for tests.
class ASH_EXPORT UserSettingsEventLogger
    : public CrasAudioHandler::AudioObserver,
      public chromeos::PowerManagerClient::Observer,
      public ShellObserver,
      public VideoDetector::Observer {
 public:
  UserSettingsEventLogger(const UserSettingsEventLogger&) = delete;
  UserSettingsEventLogger& operator=(const UserSettingsEventLogger&) = delete;

  // Creates an instance of the logger. Only one instance of the logger can
  // exist in the current process.
  static void CreateInstance();
  static void DeleteInstance();
  // Gets the current instance of the logger.
  static UserSettingsEventLogger* Get();

  // Logs an event to UKM that the user has connected to the given network.
  void LogNetworkUkmEvent(
      const chromeos::network_config::mojom::NetworkStateProperties& network);

  // Logs an event to UKM that the user has toggled night light to the given
  // state.
  void LogNightLightUkmEvent(bool enabled);

  // Logs an event to UKM that the user has toggled Quiet Mode to the given
  // state.
  void LogQuietModeUkmEvent(bool enabled);

  // Logs an event to UKM that the user has toggled an accessibility setting to
  // the given state.
  void LogAccessibilityUkmEvent(UserSettingsEvent::Event::AccessibilityId id,
                                bool enabled);

  // CrasAudioHandler::AudioObserver overrides:
  void OnOutputNodeVolumeChanged(uint64_t node, int volume) override;
  void OnOutputMuteChanged(bool mute_on) override;
  void OnOutputStarted() override;
  void OnOutputStopped() override;

  // chromeos::PowerManagerClient::Observer overrides:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;

  // ShellObserver overrides:
  void OnCastingSessionStartedOrStopped(bool started) override;
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override;

  // VideoDetector::Observer overrides:
  void OnVideoStateChanged(VideoDetector::State state) override;

  void SetClockForTesting(const base::Clock* clock);

 private:
  friend class UserSettingsEventLoggerTest;

  UserSettingsEventLogger();
  ~UserSettingsEventLogger() override;

  // Populates contextual information shared by all settings events.
  void PopulateSharedFeatures(UserSettingsEvent* settings_event);

  // Sends the given event to UKM and AppListClient.
  void SendToUkmAndAppList(const UserSettingsEvent& settings_event);

  void OnVolumeTimerEnded();
  void OnBrightnessTimerEnded();
  void OnPresentingTimerEnded();
  void OnFullscreenTimerEnded();

  // Timer to ensure that volume is only recorded after a pause.
  base::OneShotTimer volume_timer_;
  int volume_;
  int volume_before_mute_;
  int volume_before_user_change_;

  // Timer to ensure that brightness is only recorded after a pause.
  base::OneShotTimer brightness_timer_;
  // Most up-to-date brightness value. Can be null briefly, before an initial
  // brightness is set upon login.
  absl::optional<int> brightness_;
  absl::optional<int> brightness_before_user_change_;
  int brightness_after_user_change_;

  base::OneShotTimer presenting_timer_;
  int presenting_session_count_;
  // Whether the device has been presenting in the last 5 minutes.
  bool is_recently_presenting_;

  base::OneShotTimer fullscreen_timer_;
  // Whether the device has been in fullscreen mode in the last 5 minutes.
  bool is_recently_fullscreen_;

  bool used_cellular_in_session_;
  bool is_playing_audio_;
  bool is_playing_video_;

  const base::Clock* clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  static UserSettingsEventLogger* instance_;
};

}  // namespace ml
}  // namespace ash

#endif  // ASH_SYSTEM_MACHINE_LEARNING_USER_SETTINGS_EVENT_LOGGER_H_
