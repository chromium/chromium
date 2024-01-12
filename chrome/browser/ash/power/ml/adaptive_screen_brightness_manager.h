// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_MANAGER_H_
#define CHROME_BROWSER_ASH_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/power/ml/boot_clock.h"
#include "chrome/browser/ash/power/ml/screen_brightness_event.pb.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace base {
class RepeatingTimer;
}  // namespace base

namespace ash {

class AccessibilityManager;
class MagnificationManager;

namespace power {
namespace ml {

class AdaptiveScreenBrightnessUkmLogger;
class RecentEventsCounter;

// AdaptiveScreenBrightnessManager logs screen brightness and other features
// periodically and also when the screen brightness changes.
class AdaptiveScreenBrightnessManager
    : public ui::UserActivityObserver,
      public chromeos::PowerManagerClient::Observer,
      public viz::mojom::VideoDetectorObserver {
 public:
  // Duration of inactivity that marks the end of an activity.
  static constexpr base::TimeDelta kInactivityDuration = base::Seconds(20);

  // Interval at which data should be logged.
  static constexpr base::TimeDelta kLoggingInterval = base::Minutes(10);

  AdaptiveScreenBrightnessManager(
      std::unique_ptr<AdaptiveScreenBrightnessUkmLogger> ukm_logger,
      ui::UserActivityDetector* detector,
      chromeos::PowerManagerClient* power_manager_client,
      AccessibilityManager* accessibility_manager,
      MagnificationManager* magnification_manager,
      mojo::PendingReceiver<viz::mojom::VideoDetectorObserver> receiver,
      std::unique_ptr<base::RepeatingTimer> periodic_timer);

  AdaptiveScreenBrightnessManager(const AdaptiveScreenBrightnessManager&) =
      delete;
  AdaptiveScreenBrightnessManager& operator=(
      const AdaptiveScreenBrightnessManager&) = delete;

  ~AdaptiveScreenBrightnessManager() override;

  // Returns a new instance of AdaptiveScreenBrightnessManager.
  static std::unique_ptr<AdaptiveScreenBrightnessManager> CreateInstance();

  // ui::UserActivityObserver overrides:
  void OnUserActivity(const ui::Event* event) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;
  void TabletModeEventReceived(chromeos::PowerManagerClient::TabletMode mode,
                               base::TimeTicks timestamp) override;

  // viz::mojom::VideoDetectorObserver overrides:
  void OnVideoActivityStarted() override;
  void OnVideoActivityEnded() override;

 private:
  friend class AdaptiveScreenBrightnessManagerTest;

  // Called when the periodic timer triggers.
  void OnTimerFired();

  // Updates lid state and tablet mode from received switch states.
  void OnReceiveSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // Updates screen brightness percent from received value.
  void OnReceiveScreenBrightnessPercent(
      std::optional<double> screen_brightness_percent);

  // Returns the night light temperature as a percentage in the range [0, 100].
  // Returns nullopt when the night light is not enabled.
  const std::optional<int> GetNightLightTemperaturePercent() const;

  void LogEvent();

  BootClock boot_clock_;

  // Timer to trigger periodically for logging data.
  const std::unique_ptr<base::RepeatingTimer> periodic_timer_;

  const std::unique_ptr<AdaptiveScreenBrightnessUkmLogger> ukm_logger_;

  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observation_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  const raw_ptr<AccessibilityManager> accessibility_manager_;
  const raw_ptr<MagnificationManager> magnification_manager_;

  const mojo::Receiver<viz::mojom::VideoDetectorObserver> receiver_;

  // Counters for user events.
  const std::unique_ptr<RecentEventsCounter> mouse_counter_;
  const std::unique_ptr<RecentEventsCounter> key_counter_;
  const std::unique_ptr<RecentEventsCounter> stylus_counter_;
  const std::unique_ptr<RecentEventsCounter> touch_counter_;

  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::NOT_PRESENT;

  chromeos::PowerManagerClient::TabletMode tablet_mode_ =
      chromeos::PowerManagerClient::TabletMode::UNSUPPORTED;

  std::optional<power_manager::PowerSupplyProperties::ExternalPower>
      external_power_;

  // Battery percent. This is in the range [0.0, 100.0].
  std::optional<float> battery_percent_;

  // Both |screen_brightness_percent_| and |previous_screen_brightness_percent_|
  // are values reported directly by powerd. They are percentages as double but
  // are in the range of [0, 100]. When we convert these values to the fields in
  // ScreenBrightnessEvent, we cast them to ints.
  std::optional<double> screen_brightness_percent_;
  std::optional<double> previous_screen_brightness_percent_;
  std::optional<base::TimeDelta> last_event_time_since_boot_;

  // The time (since boot) of the most recent active event. This is the end of
  // the most recent period of activity.
  std::optional<base::TimeDelta> last_activity_time_since_boot_;
  // The time (since boot) of the start of the most recent period of activity.
  std::optional<base::TimeDelta> start_activity_time_since_boot_;
  std::optional<bool> is_video_playing_;
  std::optional<ScreenBrightnessEvent_Event_Reason> reason_;

  base::WeakPtrFactory<AdaptiveScreenBrightnessManager> weak_ptr_factory_{this};
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_ADAPTIVE_SCREEN_BRIGHTNESS_MANAGER_H_
