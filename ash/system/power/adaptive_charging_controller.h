// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_CONTROLLER_H_
#define ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/power/adaptive_charging_notification_controller.h"
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
  AdaptiveChargingController();
  AdaptiveChargingController(const AdaptiveChargingController&) = delete;
  AdaptiveChargingController& operator=(const AdaptiveChargingController&) =
      delete;
  ~AdaptiveChargingController() override;

  // Returns whether AdaptiveCharging is supported by hardware. This value is
  // set 1 second after PowerManager DBus initialized.
  bool IsAdaptiveChargingSupported();

  // Whether the power daemon is currently suspending charging.
  bool is_adaptive_delaying_charge() { return is_adaptive_delaying_charge_; }

 private:
  // chromeos::PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  bool is_adaptive_charging_supported_ = false;
  bool is_adaptive_delaying_charge_ = false;
  bool is_on_charger_ = false;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observation_{this};

  const std::unique_ptr<AdaptiveChargingNotificationController>
      notification_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_CONTROLLER_H_
