// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_HPS_NOTIFY_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_HPS_NOTIFY_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/hps/hps_orientation_controller.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ash {

class HpsNotifyNotificationBlocker;

// Pushes status changes to the snooping protection icon and notification
// blocker based on DBus state, preferences and session type.
class ASH_EXPORT HpsNotifyController
    : public SessionObserver,
      public HpsOrientationController::Observer,
      public chromeos::HpsDBusClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when an observer should show or hide itself because the snooping
    // status has changed. Argument is true if a snooper has now been detected.
    virtual void OnSnoopingStatusChanged(bool snooper) = 0;

    // Used to coordinate observers that might outlive the controller.
    virtual void OnHpsNotifyControllerDestroyed() = 0;
  };

  HpsNotifyController();
  HpsNotifyController(const HpsNotifyController&) = delete;
  HpsNotifyController& operator=(const HpsNotifyController&) = delete;
  ~HpsNotifyController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // HpsOrientationObserver:
  void OnOrientationChanged(bool suitable_for_hps) override;

  // chromeos::HpsDBusClient::Observer:
  void OnHpsSenseChanged(hps::HpsResult state) override;
  void OnHpsNotifyChanged(hps::HpsResult state) override;
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
    bool hps_state = false;       // The state last reported by the daemon.
    bool session_active = false;  // Whether there is an active user
                                  // session ongoing.
    bool pref_enabled = false;    // Whether the user has enabled the
                                  // feature via preferences.
    bool hps_available = false;   // Whether the daemon is available for
                                  // communication.
    bool hps_configured = false;  // Whether the daemon has been
                                  // successfully configured.
    bool orientation_suitable = false;  // Whether the device is in physical
                                        // orientation where our models are
                                        // accurate.
    bool within_pos_window = false;     // Whether we are within the minimum
                                        // time window for which to report a
                                        // positive result.
  };

  // Updates snooper state as appropriate given the signal, session,
  // preference and device orientation state. If changed, notifies observers.
  void UpdateSnooperStatus(const State& new_state);

  // Requests the start or stop of the HPS snooping signal, so that the daemon
  // need not be running snooping logic while the user has the feature disabled.
  // Also updates the new state of HPS availability.
  void ReconfigureHps(State* new_state);

  // Configures the daemon, polls its initial state and opts into its signals.
  void StartHpsObservation(bool service_is_available);

  // Performs the state update from the daemon response.
  void UpdateHpsState(absl::optional<hps::HpsResult> result);

  // A callback to update visibility when the user enables or disables the
  // feature.
  void UpdatePrefState();

  // A callback that fires once a positive signal has been emitted for the
  // minimum allowed time. Sends out any delayed updates to observers.
  void OnMinWindowExpired();

  // The state of all signals relevant to snooping status.
  State state_;

  // Used to enforce a minimum window of positive results.
  base::RetainingOneShotTimer pos_window_timer_;

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};
  base::ScopedObservation<HpsOrientationController,
                          HpsOrientationController::Observer>
      orientation_observation_{this};
  base::ScopedObservation<chromeos::HpsDBusClient,
                          chromeos::HpsDBusClient::Observer>
      hps_dbus_observation_{this};

  // Used to notify ourselves of changes to the pref that enables / disables
  // this feature.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Clients listening for snooping status changes.
  base::ObserverList<Observer> observers_;

  // Controls popup hiding and our info notification.
  const std::unique_ptr<HpsNotifyNotificationBlocker> notification_blocker_;

  // The minimum amount of time between emitting an initial positive signal and
  // then a subsequent negative one.
  const base::TimeDelta pos_window_;

  // Must be last.
  base::WeakPtrFactory<HpsNotifyController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_HPS_NOTIFY_CONTROLLER_H_
