// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_H_

#include <memory>

#include "base/macros.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/login/auth/user_context.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

namespace base {
class Clock;
}

namespace chromeos {

// Enforces a limit on the length of time for which a user authenticated via
// Gaia without SAML or with SAML can use offline authentication against a
// cached password before being forced to go through online authentication
// against GAIA again.
class OfflineSigninLimiter : public KeyedService,
                             public base::PowerSuspendObserver,
                             public session_manager::SessionManagerObserver {
 public:
  // Called when the user successfully authenticates. `auth_flow` indicates
  // the type of authentication flow that the user went through.
  void SignedIn(UserContext::AuthFlow auth_flow);

  // Allows a mock timer to be substituted for testing purposes.
  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer);

  // KeyedService:
  void Shutdown() override;

  // base::PowerObserver:
  void OnResume() override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

 private:
  friend class OfflineSigninLimiterFactory;
  friend class OfflineSigninLimiterTest;

  // `profile` and `clock` must remain valid until Shutdown() is called. If
  // `clock` is NULL, the shared base::DefaultClock instance will be used.
  OfflineSigninLimiter(Profile* profile, base::Clock* clock);
  ~OfflineSigninLimiter() override;

  // Recalculates the amount of time remaining until online login should be
  // forced and sets the `offline_signin_limit_timer_` accordingly. If the limit
  // has expired already, sets the flag enforcing online login immediately.
  void UpdateLimit();

  // Convenience method to get the time limit for SAML and no-SAML flows
  // taking into consideration a possible override from the command line.
  // Returns nullopt if it is an invalid time.
  base::Optional<base::TimeDelta> GetGaiaSamlTimeLimit();
  base::Optional<base::TimeDelta> GetGaiaNoSamlTimeLimit();
  base::Optional<base::TimeDelta> GetTimeLimitOverrideForTesting();

  // Sets the flag enforcing online login. This will cause the user's next login
  // to use online authentication against GAIA.
  void ForceOnlineLogin();

  // Stores the last online login time and offline login time limit
  void UpdateOnlineSigninData(base::Time time,
                              base::Optional<base::TimeDelta> limit);

  Profile* profile_;
  base::Clock* clock_;

  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<base::OneShotTimer> offline_signin_limit_timer_;

  DISALLOW_COPY_AND_ASSIGN(OfflineSigninLimiter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_OFFLINE_SIGNIN_LIMITER_H_
