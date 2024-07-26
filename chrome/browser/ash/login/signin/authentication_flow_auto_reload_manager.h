// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTHENTICATION_FLOW_AUTO_RELOAD_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTHENTICATION_FLOW_AUTO_RELOAD_MANAGER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "components/prefs/pref_change_registrar.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace ash {

// This class manages the automatic reloading of the user's authentication flow
// depending on the value set for `DeviceAuthenticationFlowAutoReloadInterval`
// policy.
class AuthenticationFlowAutoReloadManager {
 public:
  AuthenticationFlowAutoReloadManager();

  ~AuthenticationFlowAutoReloadManager();

  AuthenticationFlowAutoReloadManager(
      const AuthenticationFlowAutoReloadManager&) = delete;
  AuthenticationFlowAutoReloadManager& operator=(
      const AuthenticationFlowAutoReloadManager&) = delete;

  // Activate auto reload to start timer.
  void Activate(base::OnceClosure callback);

  // Terminate auto reload preventing any scheduled reloads from happening.
  void Terminate();

  static void SetClockForTesting(base::Clock* clock,
                                 base::TickClock* tick_clock);

  void ResumeTimerForTesting();

  bool IsTimerActiveForTesting();

  int GetAttemptsCount() const;

 private:
  // Fetch policy value for the reload time interval
  std::optional<base::TimeDelta> GetAutoReloadInterval();

  void OnPolicyUpdated();

  void ReloadAuthenticationFlow();

  PrefChangeRegistrar local_state_registrar_;

  base::OnceClosure callback_;

  std::unique_ptr<base::WallClockTimer> auto_reload_timer_;

  int auto_reload_attempts_ = 0;

  static base::Clock* clock_for_testing_;
  static base::TickClock* tick_clock_for_testing_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_AUTHENTICATION_FLOW_AUTO_RELOAD_MANAGER_H_
