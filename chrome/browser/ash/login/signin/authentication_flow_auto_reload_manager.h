// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTHENTICATION_FLOW_AUTO_RELOAD_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTHENTICATION_FLOW_AUTO_RELOAD_MANAGER_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/idle/idle_polling_service.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace ash {

// This class manages the automatic reloading of the user's authentication flow
// depending on the value set for `DeviceAuthenticationFlowAutoReloadInterval`
// policy.
class AuthenticationFlowAutoReloadManager
    : public ui::IdlePollingService::Observer {
 public:
  static constexpr base::TimeDelta kPostponeInterval = base::Minutes(1);

  AuthenticationFlowAutoReloadManager();

  AuthenticationFlowAutoReloadManager(
      const AuthenticationFlowAutoReloadManager&) = delete;
  AuthenticationFlowAutoReloadManager& operator=(
      const AuthenticationFlowAutoReloadManager&) = delete;

  ~AuthenticationFlowAutoReloadManager() override;

  // Activate auto reload to start timer.
  void Activate(base::OnceClosure callback);

  // Terminate auto reload preventing any scheduled reloads from happening.
  void Terminate();

  int GetAttemptsCount() const;

  static void SetClockForTesting(base::Clock* clock,
                                 const base::TickClock* tick_clock);

  void ResumeTimerForTesting();

  bool IsActiveForTesting();

 private:
  // Fetch policy value for the reload time interval.
  std::optional<base::TimeDelta> GetAutoReloadInterval();

  void OnPolicyUpdated();

  void ReloadAuthenticationFlow();

  // ui::IdlePollingService::Observer:
  void OnIdleStateChange(const ui::IdlePollingService::State& state) override;

  // By default assume the device is idle and autoreload should be fired.
  bool is_idle_ = true;

  // Used to allow postponing the automatic reload only once in case the device
  // is not idle.
  bool reload_postponed_ = false;

  base::ScopedObservation<ui::IdlePollingService,
                          ui::IdlePollingService::Observer>
      idle_state_observer_{this};

  PrefChangeRegistrar local_state_registrar_;

  base::OnceClosure callback_;

  std::unique_ptr<base::WallClockTimer> auto_reload_timer_;

  int auto_reload_attempts_ = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTHENTICATION_FLOW_AUTO_RELOAD_MANAGER_H_
