// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_PREFS_H_
#define ASH_SYSTEM_POWER_POWER_PREFS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/tick_clock.h"
#include "chromeos/dbus/power/power_manager_client.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace chromeos {
class PowerPolicyController;
}  // namespace chromeos

namespace power_manager {
class ScreenIdleState;
}  // namespace power_manager

namespace ash {

class PowerPrefsTest;

// Sends an updated power policy to the |power_policy_controller| whenever one
// of the power-related prefs changes.
class ASH_EXPORT PowerPrefs : public chromeos::PowerManagerClient::Observer,
                              public SessionObserver {
 public:
  PowerPrefs(chromeos::PowerPolicyController* power_policy_controller,
             chromeos::PowerManagerClient* power_manager_client,
             PrefService* local_state);
  ~PowerPrefs() override;

  // Registers power prefs with default values applicable to the local state
  // prefs.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Registers power prefs with default values applicable to the signin prefs.
  static void RegisterSigninProfilePrefs(PrefRegistrySimple* registry);

  // Registers power prefs with default values applicable to the user prefs.
  static void RegisterUserProfilePrefs(PrefRegistrySimple* registry);

  void set_tick_clock_for_test(base::TickClock* clock) { tick_clock_ = clock; }

 private:
  friend class PowerPrefsTest;

  // chromeos::PowerManagerClient::Observer:
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  void UpdatePowerPolicyFromPrefs();

  // Observes either the signin screen prefs or active user prefs and loads
  // initial settings.
  void ObservePrefs(PrefService* prefs);

  void ObserveLocalStatePrefs(PrefService* prefs);

  chromeos::PowerPolicyController* const
      power_policy_controller_;  // Not owned.

  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_;

  std::unique_ptr<PrefChangeRegistrar> profile_registrar_;
  std::unique_ptr<PrefChangeRegistrar> local_state_registrar_;

  const base::TickClock* tick_clock_;  // Not owned.

  // Time at which the screen was locked. Unset if the screen is unlocked.
  base::TimeTicks screen_lock_time_;

  // Time at which the screen was last turned off due to user inactivity.
  // Unset if the screen isn't currently turned off due to user inactivity.
  base::TimeTicks screen_idle_off_time_;

  PrefService* local_state_ = nullptr;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(PowerPrefs);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_PREFS_H_
