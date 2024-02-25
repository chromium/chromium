// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_CONTROLLER_H_
#define ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/human_presence/human_presence_orientation_controller.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"
#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"
#include "components/session_manager/session_manager_types.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ash {

class SnoopingProtectionNotificationBlocker;

// Pushes status changes to the snooping protection icon and notification
// blocker based on DBus state, preferences and session type.
class ASH_EXPORT SnoopingProtectionController
    : public SessionObserver,
      public HumanPresenceOrientationController::Observer,
      public HumanPresenceDBusClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when an observer should show or hide itself because the snooping
    // status has changed. Argument is true if a snooper has now been detected.
    virtual void OnSnoopingStatusChanged(bool snooper) = 0;

    // Used to coordinate observers that might outlive the controller.
    virtual void OnSnoopingProtectionControllerDestroyed() = 0;
  };

  SnoopingProtectionController();
  SnoopingProtectionController(const SnoopingProtectionController&) = delete;
  SnoopingProtectionController& operator=(const SnoopingProtectionController&) =
      delete;
  ~SnoopingProtectionController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // HumanPresenceOrientationObserver:
  void OnOrientationChanged(bool suitable_for_human_presence) override;

  // HumanPresenceDBusClient::Observer:
  void OnHpsSenseChanged(const hps::HpsResultProto&) override;
  void OnHpsNotifyChanged(const hps::HpsResultProto&) override;
  void OnRestart() override;
  void OnShutdown() override;

  // Add/remove views that are listening for snooper presence.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // The current snooper status.
  bool SnooperPresent() const;

 private:
  // Used to track whether a signal should actually trigger a visibility change.
  struct State {
    // Whether a snooper is present, as last reported by the service.
    bool present = false;

    // Whether there is an active user session ongoing.
    bool session_active = false;

    // Whether the user has enabled the feature via preferences.
    bool pref_enabled = false;

    // Whether the daemon is available for communication.
    bool service_available = false;

    // Whether the daemon has been successfully configured.
    bool service_configured = false;

    // Whether the device is in physical orientation where our models are
    // accurate.
    bool orientation_suitable = false;

    // Whether we are within the minimum time window for which to report a
    // positive result.
    bool within_pos_window = false;
  };

  // Updates snooper state as appropriate given the signal, session,
  // preference and device orientation state. If changed, notifies observers.
  void UpdateSnooperStatus(const State& new_state);

  // Requests the start or stop of the snooping signal, so that the daemon need
  // not be running snooping logic while the user has the feature disabled.
  // Also updates the new state of service availability.
  void ReconfigureService(State* new_state);

  // Configures the daemon, polls its initial state and opts into its signals.
  void StartServiceObservation(bool service_is_available);

  // Performs the state update from the daemon response.
  void UpdateServiceState(std::optional<hps::HpsResultProto> result);

  // A callback to update visibility when the user enables or disables the
  // feature.
  void UpdatePrefState();

  // A callback that fires once a positive signal has been emitted for the
  // minimum allowed time. Sends out any delayed updates to observers.
  void OnMinWindowExpired();

  // Logs the amount of time a snooper was present / absent.
  // An initial call just caches the current time (and doesn't log anything).
  // Subsequent calls update the cache and log time elapsed if snooping status
  // has changed.
  // The cache can be cleared by resetting `last_presence_report_time_`.
  void LogPresenceWindow(bool is_present);

  // The state of all signals relevant to snooping status.
  State state_;

  // Used to enforce a minimum window of positive results.
  base::RetainingOneShotTimer pos_window_timer_;

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};
  base::ScopedObservation<HumanPresenceOrientationController,
                          HumanPresenceOrientationController::Observer>
      orientation_observation_{this};
  base::ScopedObservation<HumanPresenceDBusClient,
                          HumanPresenceDBusClient::Observer>
      human_presence_dbus_observation_{this};

  // Used to notify ourselves of changes to the pref that enables / disables
  // this feature.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Clients listening for snooping status changes.
  base::ObserverList<Observer> observers_;

  // Controls popup hiding and our info notification.
  const std::unique_ptr<SnoopingProtectionNotificationBlocker>
      notification_blocker_;

  // The minimum amount of time between emitting an initial positive signal and
  // then a subsequent negative one.
  const base::TimeDelta pos_window_;

  // Last time the snooper became present or absent.
  base::TimeTicks last_presence_report_time_;

  // Must be last.
  base::WeakPtrFactory<SnoopingProtectionController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_CONTROLLER_H_
