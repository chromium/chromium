// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_BATTERY_SAVER_CONTROLLER_H_
#define ASH_SYSTEM_POWER_BATTERY_SAVER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/ash_features.h"
#include "ash/system/power/power_status.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// BatterySaverController is a singleton that controls battery saver state via
// PowerManagerClient by watching for updates to ash::prefs::kPowerBatterySaver
// from settings and power status for charging state, and logs metrics.
class ASH_EXPORT BatterySaverController : public PowerStatus::Observer {
 public:
  enum class UpdateReason {
    kCharging,
    kLowPower,
    kPowerManager,
    kSettings,
    kThreshold,
    kAlwaysOn,
  };

  // The battery charge percent at which battery saver is activated.
  static const double kActivationChargePercent;

  explicit BatterySaverController(PrefService* local_state);
  BatterySaverController(const BatterySaverController&) = delete;
  BatterySaverController& operator=(const BatterySaverController&) = delete;
  ~BatterySaverController() override;

  // Registers local state prefs used in the settings UI.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void SetState(bool active, UpdateReason reason);

 private:
  // Types used for metrics tracking.
  struct EnableRecord {
    base::Time time;
    UpdateReason reason;
  };

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  void OnSettingsPrefChanged();

  void DisplayBatterySaverModeDisabledToast();

  absl::optional<int> GetRemainingMinutes(const PowerStatus* status);

  void MaybeResetNotificationAvailability(
      features::BatterySaverNotificationBehavior experiment,
      const double battery_percent,
      const int battery_remaining_minutes);

  raw_ptr<PrefService, ExperimentalAsh> local_state_;  // Non-owned and must
                                                       // out-live this.

  base::ScopedObservation<PowerStatus, PowerStatus::Observer>
      power_status_observation_{this};

  PrefChangeRegistrar pref_change_registrar_;

  // Used to debounce changes to the pref and state in Power Manager.
  bool active_ = false;

  bool always_on_ = false;

  bool previously_plugged_in_ = false;

  bool threshold_crossed_ = false;

  bool low_power_crossed_ = false;

  absl::optional<EnableRecord> enable_record_{absl::nullopt};

  base::WeakPtrFactory<BatterySaverController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_BATTERY_SAVER_CONTROLLER_H_
