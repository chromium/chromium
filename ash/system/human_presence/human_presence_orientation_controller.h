// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HUMAN_PRESENCE_HUMAN_PRESENCE_ORIENTATION_CONTROLLER_H_
#define ASH_SYSTEM_HUMAN_PRESENCE_HUMAN_PRESENCE_ORIENTATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"

namespace ash {

// HPS features only work when the sensor is in a "standard" configuration: in
// laptop mode on the user's desk / lap in front of them.
//
// This controller tracks the physical state of the device and signals observers
// when it enters or leaves non-standard orientations.
class ASH_EXPORT HumanPresenceOrientationController
    : public TabletModeObserver,
      public display::DisplayObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    // Called when the suitability of the device orientation for HPS-based
    // features changes.
    virtual void OnOrientationChanged(bool suitable_for_hps) = 0;
  };

  HumanPresenceOrientationController();
  HumanPresenceOrientationController(
      const HumanPresenceOrientationController& other) = delete;
  HumanPresenceOrientationController& operator=(
      const HumanPresenceOrientationController& other) = delete;
  ~HumanPresenceOrientationController() override;

  // Start or stop listening for changes to device orientation status.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Whether or not the device orientation is currently suitable for HPS-based
  // features.
  bool IsOrientationSuitable() const;

 private:
  // TabletModeObserver:
  void OnTabletPhysicalStateChanged() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // chromeos::PowerManagerClient::Observer:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks timestamp) override;
  void OnReceiveSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // Updates the internal state of the controller, and notifies observers if
  // the state has changed.
  void UpdateOrientation(bool physical_tablet_state,
                         bool display_rotated,
                         bool lid_closed);

  // If the device is physically configured like a tablet, we will sense frames
  // from atypical angles. Note this is different from tablet mode in general,
  // since a device can be in a physical tablet configuration but not be in
  // general "tablet mode" because it is still using the non-tablet UI (e.g.
  // when an external keyboard is connected).
  bool physical_tablet_state_ = false;

  // We make the assumption that the user is always oriented to match the screen
  // rotation. Hence, if the screen is rotated, we will be sensing rotated
  // frames.
  bool display_rotated_ = false;

  // Device with close lid captures unusable frames.
  bool lid_closed_ = false;

  // Clients listening for orientation status changes.
  base::ObserverList<Observer> observers_;

  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observation_{this};
  display::ScopedDisplayObserver display_observation_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  base::WeakPtrFactory<HumanPresenceOrientationController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HUMAN_PRESENCE_HUMAN_PRESENCE_ORIENTATION_CONTROLLER_H_
