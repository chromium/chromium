// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_BATTERY_SAVER_CONTROLLER_H_
#define ASH_SYSTEM_POWER_BATTERY_SAVER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/constants/ash_features.h"
#include "ash/system/power/power_status.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

// BatterySaverController is a singleton that controls battery saver state via
// PowerManagerClient by watching for updates to ash::prefs::kPowerBatterySaver
// from settings and power status for charging state.
// TODO(cwd): And logs metrics.
class ASH_EXPORT BatterySaverController : public PowerStatus::Observer {
 public:
  enum NotificationType { kThreshold, kLowPower };
  // The battery charge percent at which battery saver is activated.
  static const double kActivationChargePercent;

  explicit BatterySaverController(PrefService* local_state);
  BatterySaverController(const BatterySaverController&) = delete;
  BatterySaverController& operator=(const BatterySaverController&) = delete;
  ~BatterySaverController() override;

  // Registers local state prefs used in the settings UI.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void SetStateForTesting(bool active);

  void UpdateBatterySaverStateFromNotification(
      NotificationType notification_type,
      bool active);

 private:
  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  void OnSettingsPrefChanged();

  void DisplayBatterySaverModeDisabledToast();

  bool UpdateSettings(bool active);

  void SetBatterySaverState(bool active);

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

  bool always_on_ = false;

  bool previously_plugged_in_ = false;

  bool threshold_crossed_ = false;

  bool low_power_crossed_ = false;

  // Has the user opted in or out (meaning depends on experiment arm), and at
  // what level (threshold or low power)?
  std::map<features::BatterySaverNotificationBehavior,
           std::map<NotificationType, bool>>
      users_opt_status_;

  base::WeakPtrFactory<BatterySaverController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_BATTERY_SAVER_CONTROLLER_H_
