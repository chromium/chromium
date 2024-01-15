// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

namespace base {
class Clock;
}

namespace ash {

// Enforces a limit on the length of time for which a user authenticated via
// Gaia without SAML or with SAML can use offline authentication against a
// cached password before being forced to go through online authentication
// against GAIA again.
class OfflineSigninLimiter : public KeyedService,
                             public base::PowerSuspendObserver,
                             public session_manager::SessionManagerObserver {
 public:
  // `profile` and `clock` must remain valid until Shutdown() is called. If
  // `clock` is NULL, the shared base::DefaultClock instance will be used.
  OfflineSigninLimiter(Profile* profile, const base::Clock* clock);
  OfflineSigninLimiter(const OfflineSigninLimiter&) = delete;
  OfflineSigninLimiter& operator=(const OfflineSigninLimiter&) = delete;
  ~OfflineSigninLimiter() override;  // public for testing purpose only.

  // Called when the user successfully authenticates. `auth_flow` indicates
  // the type of authentication flow that the user went through.
  void SignedIn(UserContext::AuthFlow auth_flow);

  base::WallClockTimer* GetTimerForTesting();

  base::WallClockTimer* GetLockscreenTimerForTesting();

  // KeyedService:
  void Shutdown() override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

 private:
  friend class OfflineSigninLimiterFactory;
  friend class OfflineSigninLimiterTest;

  // Recalculates the amount of time remaining until online login should be
  // forced and sets the `offline_signin_limit_timer_` accordingly. If the limit
  // has expired already, sets the flag enforcing online login immediately.
  void UpdateLimit();

  // Recalculates the amount of time remaining until online login through the
  // lock screen should be forced and sets the
  // `offline_lock_screen_signin_limit_timer_` accordingly. If the limit has
  // expired already, sets the flag enforcing online re-authentication
  // immediately.
  void UpdateLockScreenLimit();

  // Reads the timestamp of the last online signin of the user from the Local
  // State.
  base::Time GetLastOnlineSigninTime();

  // Convenience method to get the time limit for SAML and no-SAML flows.
  // Returns nullopt if it is an invalid time.
  std::optional<base::TimeDelta> GetGaiaNoSamlTimeLimit();
  std::optional<base::TimeDelta> GetGaiaSamlTimeLimit();
  std::optional<base::TimeDelta> GetGaiaNoSamlLockScreenTimeLimit();
  std::optional<base::TimeDelta> GetGaiaSamlLockScreenTimeLimit();

  // Sets the flag enforcing online login. This will cause the user's next login
  // to use online authentication against GAIA.
  void ForceOnlineLogin();

  // Enforces online reauthentication on the lock screen.
  void ForceOnlineLockScreenReauth();

  // Stores the last online login time and offline login time limit
  void UpdateOnlineSigninData(base::Time time,
                              std::optional<base::TimeDelta> limit);

  // Helper function to get user for the given profile_.
  const user_manager::User& GetUser();

  raw_ptr<Profile> profile_;
  raw_ptr<const base::Clock> clock_;

  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<base::WallClockTimer> offline_signin_limit_timer_;

  std::unique_ptr<base::WallClockTimer> offline_lock_screen_signin_limit_timer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_H_
