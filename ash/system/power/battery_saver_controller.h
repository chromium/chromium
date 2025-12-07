// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_BATTERY_SAVER_CONTROLLER_H_
#define ASH_SYSTEM_POWER_BATTERY_SAVER_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/system/power/power_status.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

// IsBatterySaverAllowed returns true if the Battery Saver feature is enabled
// and it is not disabled via policy.
ASH_EXPORT bool IsBatterySaverAllowed();

// Test method to allow testing without the Battery Saver feature.
ASH_EXPORT void OverrideIsBatterySaverAllowedForTesting(
    std::optional<bool> isAllowed);

// BatterySaverController is a singleton that controls battery saver state via
// PowerManagerClient by watching for updates to ash::prefs::kPowerBatterySaver
// from settings and power status for charging state, and logs metrics.
class ASH_EXPORT BatterySaverController : public PowerStatus::Observer {
 public:
  enum class UpdateReason {
    kCharging,
    kChargeIncrease,
    kLowPower,
    kPowerManager,
    kSettings,
    kThreshold,
    kAlwaysOn,
  };

  static constexpr char kBatterySaverToastId[] =
      "battery_saver_mode_state_changed";

  // When Battery Saver is enabled, the amount of percent increase in battery
  // charge that will trigger disabling. Used to detect charging while asleep or
  // shut down.
  static constexpr int kBatterySaverSleepChargeThreshold = 3;

  explicit BatterySaverController(PrefService* local_state);
  BatterySaverController(const BatterySaverController&) = delete;
  BatterySaverController& operator=(const BatterySaverController&) = delete;
  ~BatterySaverController() override;

  // Registers local state prefs used in the settings UI.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Reset the pref and state in Power Manager. Used to clean up state when
  // Battery Saver is no longer available.
  static void ResetState(PrefService* local_state);

  void SetState(bool active, UpdateReason reason);

  bool IsBatterySaverSupported() const;

  bool IsDisabledByPolicy() const;

  void ShowBatterySaverModeDisabledToast();

  void ShowBatterySaverModeEnabledToast();

  void ClearBatterySaverModeToast();

  void StopObservingPowerStatusForTest();

 private:
  // Types used for metrics tracking.
  struct EnableRecord {
    base::TimeTicks time;
    UpdateReason reason;
  };

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  void OnSettingsPrefChanged();

  void ShowBatterySaverModeToastHelper(const ToastCatalogName catalog_name,
                                       const std::u16string& toast_text);

  std::optional<int> GetRemainingMinutes(const PowerStatus* status);

  // Non-owned and must out-live this. May be null in some test contexts.
  raw_ptr<PrefService> local_state_;

  base::ScopedObservation<PowerStatus, PowerStatus::Observer>
      power_status_observation_{this};

  PrefChangeRegistrar pref_change_registrar_;

  const double activation_charge_percent_;

  bool always_on_ = false;

  bool previously_plugged_in_ = false;

  bool threshold_crossed_ = false;

  // Whether OnSettingsPrefChanged() was called from `SetState`.
  bool in_set_state_ = false;

  std::optional<EnableRecord> enable_record_{std::nullopt};

  base::WeakPtrFactory<BatterySaverController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_BATTERY_SAVER_CONTROLLER_H_
