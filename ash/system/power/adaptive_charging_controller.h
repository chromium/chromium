// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_CONTROLLER_H_
#define ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace ash {

// The controller responsible for the adaptive charging toast and notifications,
// and communication with the power daemon.
//
// Is currently a stub. TODO(b:216035280): add in real logic.
class ASH_EXPORT AdaptiveChargingController
    : public chromeos::PowerManagerClient::Observer {
 public:
  // A client that observes AdaptiveCharging state should inherit this.
  class Observer : public base::CheckedObserver {
   public:
    Observer();
    ~Observer() override;

    // This is fired when AdaptiveCharging in PowerD decides to suspend current
    // charging process. The charging process will be resumed when
    // OnAdaptiveChargingStopped is fired.
    virtual void OnAdaptiveChargingStarted() = 0;

    // This is fired when AdaptiveCharging in PowerD decides to resume the
    // charging process after suspending the charging for some time.
    virtual void OnAdaptiveChargingStopped() = 0;
  };

  AdaptiveChargingController();
  AdaptiveChargingController(const AdaptiveChargingController&) = delete;
  AdaptiveChargingController& operator=(const AdaptiveChargingController&) =
      delete;
  ~AdaptiveChargingController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether AdaptiveCharging is supported by hardware. This value is
  // set 1 second after PowerManager DBus initialized.
  bool IsAdaptiveChargingSupported();

  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

 private:
  absl::optional<bool> adaptive_delaying_charge_;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observation_{this};

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_CONTROLLER_H_
