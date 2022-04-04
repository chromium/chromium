// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_HPS_SENSE_CONTROLLER_H_
#define ASH_SYSTEM_POWER_HPS_SENSE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/hps/hps_orientation_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_service.pb.h"

namespace ash {

// Helper class for chromeos::HpsDBusClient, responsible for enabling/disabling
// the DBus service via the client and is responsible for maintaining state
// between restarts.
class ASH_EXPORT HpsSenseController : public HpsOrientationController::Observer,
                                      chromeos::HpsDBusClient::Observer {
 public:
  // The state of HpsSense inside HpsDbusService that is configured. It is set
  // as kUnknown in this class on initialization. And is set to either kEnable
  // or kDisable when EnableHpsSense() or DisableHpsSense() is called.
  enum class ConfiguredHpsSenseState {
    kUnknown,
    kEnabled,
    kDisabled,
  };

  HpsSenseController();
  HpsSenseController(const HpsSenseController&) = delete;
  HpsSenseController& operator=(const HpsSenseController&) = delete;

  ~HpsSenseController() override;

  // Enables the HpsSense feature inside HpsDBusClient; and it only sends the
  // method call if HpsSense is not enabled yet.
  void EnableHpsSense();
  // Disables the HpsSense feature inside HpsDBusClient if it is currently
  // enabled.
  void DisableHpsSense();

  // HpsOrientationObserver:
  void OnOrientationChanged(bool suitable_for_hps) override;

  // chromeos::HpsDBusClient::Observer:
  void OnHpsSenseChanged(hps::HpsResult state) override;
  void OnHpsNotifyChanged(hps::HpsResult state) override;
  // Re-enables HpsSense on HpsBusService restart if it was enabled before.
  void OnRestart() override;
  void OnShutdown() override;

 private:
  // Called when the Hps Service is available.
  void OnHpsServiceAvailable(bool service_available);

  // May disable/enable hps_sense based on current state.
  void ReconfigViaDbus();

  // Indicates whether the hps service is available; it is set inside
  // OnHpsServiceAvailable and set to false OnShutdown.
  bool service_available_ = false;

  // Records requested hps sense enable state from client.
  bool want_hps_sense_ = false;

  // Whether the device is in physical orientation where our models are
  // accurate.
  bool suitable_for_hps_ = false;

  // Current configured state of HpsSense.
  ConfiguredHpsSenseState configured_state_ = ConfiguredHpsSenseState::kUnknown;

  base::ScopedObservation<chromeos::HpsDBusClient,
                          chromeos::HpsDBusClient::Observer>
      hps_observation_{this};
  base::ScopedObservation<HpsOrientationController,
                          HpsOrientationController::Observer>
      orientation_observation_{this};
  base::WeakPtrFactory<HpsSenseController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_LISTENER_H_