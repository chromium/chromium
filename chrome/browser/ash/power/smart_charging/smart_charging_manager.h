// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_SMART_CHARGING_SMART_CHARGING_MANAGER_H_
#define CHROME_BROWSER_ASH_POWER_SMART_CHARGING_SMART_CHARGING_MANAGER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/power/ml/boot_clock.h"
#include "chrome/browser/ash/power/smart_charging/smart_charging_ukm_logger.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/charge_history_state.pb.h"
#include "chromeos/dbus/power_manager/user_charging_event.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace ash {
namespace power {
namespace ml {
class RecentEventsCounter;
}  // namespace ml

// SmartChargingManager logs battery percentage and other features related to
// user charging events. It is currently used to log data and will be
// extended to do inference in the future.
class SmartChargingManager : public ui::UserActivityObserver,
                             public chromeos::PowerManagerClient::Observer,
                             public viz::mojom::VideoDetectorObserver,
                             public session_manager::SessionManagerObserver {
 public:
  SmartChargingManager(
      ui::UserActivityDetector* detector,
      mojo::PendingReceiver<viz::mojom::VideoDetectorObserver> receiver,
      session_manager::SessionManager* session_manager,
      std::unique_ptr<base::RepeatingTimer> periodic_timer);
  ~SmartChargingManager() override;
  SmartChargingManager(const SmartChargingManager&) = delete;
  SmartChargingManager& operator=(const SmartChargingManager&) = delete;

  // Stores start time and end time of events.
  struct TimePeriod {
    TimePeriod(base::TimeDelta start, base::TimeDelta end) {
      start_time = start;
      end_time = end;
    }
    base::TimeDelta start_time;
    base::TimeDelta end_time;
  };

  static std::unique_ptr<SmartChargingManager> CreateInstance();

  // ui::UserActivityObserver overrides:
  void OnUserActivity(const ui::Event* event) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void PowerManagerBecameAvailable(bool available) override;
  void ShutdownRequested(power_manager::RequestShutdownReason reason) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;
  void TabletModeEventReceived(chromeos::PowerManagerClient::TabletMode mode,
                               base::TimeTicks timestamp) override;

  // viz::mojom::VideoDetectorObserver overrides:
  void OnVideoActivityStarted() override;
  void OnVideoActivityEnded() override;

  // session_manager::SessionManagerObserver overrides:
  void OnUserSessionStarted(bool is_primary_user) override;

 private:
  friend class SmartChargingManagerTest;

  // Populates the UserChargingEvent proto for logging/inference.
  void PopulateUserChargingEventProto(power_manager::UserChargingEvent* proto);

  // Log the event.
  void LogEvent(const power_manager::UserChargingEvent::Event::Reason& reason);

  // Called when the periodic timer triggers.
  void OnTimerFired();

  // Get charge history from powerd and update it.
  void UpdateChargeHistory();

  // Updates screen brightness percent from received value.
  void OnReceiveScreenBrightnessPercent(
      std::optional<double> screen_brightness_percent);

  // Updates lid state and tablet mode from received switch states.
  void OnReceiveSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // Gets amount of time of video playing recently (e.g. in the last 30
  // minutes).
  base::TimeDelta DurationRecentVideoPlaying();

  // Tries to load data from user profile path.
  void MaybeLoadFromDisk(const base::FilePath& profile_path);

  // Tries to save data to user profile path.
  void MaybeSaveToDisk(const base::FilePath& profile_path);

  // Calls after saving from disk completes.
  void OnLoadProtoFromDiskComplete(
      std::unique_ptr<power_manager::PastChargingEvents> proto);

  // Adds a past events given it's reason to |past_events_|.
  void AddPastEvent(
      const power_manager::UserChargingEvent::Event::Reason& reason);

  // Updates and deletes events.
  void UpdatePastEvents();

  // Gets the "plug in" and "unplug" events of the last charge.
  std::tuple<power_manager::PastChargingEvents::Event,
             power_manager::PastChargingEvents::Event>
  GetLastChargeEvents();

  void OnChargeHistoryReceived(
      std::optional<power_manager::ChargeHistoryState> proto);

  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observation_{this};

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  // Timer to trigger periodically for logging data.
  const std::unique_ptr<base::RepeatingTimer> periodic_timer_;

  // Timer to trigger the update of charge history periodically;
  const std::unique_ptr<base::RepeatingTimer> charge_history_timer_ =
      std::make_unique<base::RepeatingTimer>();

  // Number of trials to get charge history again if failed.
  int num_trials_getting_charge_history_ = 5;
  const std::unique_ptr<base::OneShotTimer>
      retry_getting_charge_history_timer_ =
          std::make_unique<base::OneShotTimer>();

  // Checks if data is loaded from disk yet.
  bool loaded_from_disk_ = false;

  // Helper to return TimeSinceBoot.
  ml::BootClock boot_clock_;
  int event_id_ = -1;

  const mojo::Receiver<viz::mojom::VideoDetectorObserver> receiver_;

  // Counters for user events.
  const std::unique_ptr<ml::RecentEventsCounter> mouse_counter_;
  const std::unique_ptr<ml::RecentEventsCounter> key_counter_;
  const std::unique_ptr<ml::RecentEventsCounter> stylus_counter_;
  const std::unique_ptr<ml::RecentEventsCounter> touch_counter_;

  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::NOT_PRESENT;

  chromeos::PowerManagerClient::TabletMode tablet_mode_ =
      chromeos::PowerManagerClient::TabletMode::UNSUPPORTED;

  // A queue that stores recent video usage of the user.
  base::circular_deque<TimePeriod> recent_video_usage_;
  // Most recent time the user started playing video.
  base::TimeDelta most_recent_video_start_time_;
  bool is_video_playing_ = false;

  // TODO(crbug.com/40109338): This is for testing only. Need to remove when ukm
  // logger is available.
  power_manager::UserChargingEvent user_charging_event_for_test_;
  std::vector<power_manager::PastChargingEvents::Event> past_events_;

  std::optional<double> battery_percent_;
  std::optional<double> screen_brightness_percent_;
  std::optional<power_manager::PowerSupplyProperties::ExternalPower>
      external_power_;
  power_manager::ChargeHistoryState charge_history_;
  std::optional<bool> is_charging_;

  std::optional<base::FilePath> profile_path_;
  const std::unique_ptr<SmartChargingUkmLogger> ukm_logger_;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  base::WeakPtrFactory<SmartChargingManager> weak_ptr_factory_{this};
};

}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_SMART_CHARGING_SMART_CHARGING_MANAGER_H_
