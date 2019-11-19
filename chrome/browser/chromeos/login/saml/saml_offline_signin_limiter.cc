// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

void SAMLOfflineSigninLimiter::SignedIn(UserContext::AuthFlow auth_flow) {
  PrefService* prefs = profile_->GetPrefs();
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  if (!user) {
    NOTREACHED();
    return;
  }
  const AccountId account_id = user->GetAccountId();

  if (auth_flow == UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML) {
    // The user went through online authentication and GAIA did not redirect to
    // a SAML IdP. No limit applies in this case. Clear the time of last login
    // with SAML and the flag enforcing online login, then return.
    prefs->ClearPref(prefs::kSAMLLastGAIASignInTime);
    user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id, false);
    return;
  }

  if (auth_flow == UserContext::AUTH_FLOW_GAIA_WITH_SAML) {
    // The user went through online authentication and GAIA did redirect to a
    // SAML IdP. Update the time of last login with SAML and clear the flag
    // enforcing online login. The flag will be set again when the limit
    // expires. If the limit already expired (e.g. because it was set to zero),
    // the flag will be set again immediately.
    user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id, false);
    prefs->SetInt64(prefs::kSAMLLastGAIASignInTime,
                    clock_->Now().ToInternalValue());
  }

  // Start listening for pref changes.
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kSAMLOfflineSigninTimeLimit,
                             base::Bind(&SAMLOfflineSigninLimiter::UpdateLimit,
                                        base::Unretained(this)));

  // Start listening to power state.
  base::PowerMonitor::AddObserver(this);

  // Arm the |offline_signin_limit_timer_| if a limit is in force.
  UpdateLimit();
}

void SAMLOfflineSigninLimiter::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  offline_signin_limit_timer_ = std::move(timer);
}

void SAMLOfflineSigninLimiter::Shutdown() {
  offline_signin_limit_timer_->Stop();
  pref_change_registrar_.RemoveAll();
}

void SAMLOfflineSigninLimiter::OnResume() {
  UpdateLimit();
}

SAMLOfflineSigninLimiter::SAMLOfflineSigninLimiter(Profile* profile,
                                                   base::Clock* clock)
    : profile_(profile),
      clock_(clock ? clock : base::DefaultClock::GetInstance()),
      offline_signin_limit_timer_(std::make_unique<base::OneShotTimer>()) {}

SAMLOfflineSigninLimiter::~SAMLOfflineSigninLimiter() {
  base::PowerMonitor::RemoveObserver(this);
}

void SAMLOfflineSigninLimiter::UpdateLimit() {
  // Stop the |offline_signin_limit_timer_|.
  offline_signin_limit_timer_->Stop();

  PrefService* prefs = pref_change_registrar_.prefs();
  const base::TimeDelta offline_signin_time_limit =
      base::TimeDelta::FromSeconds(
          prefs->GetInteger(prefs::kSAMLOfflineSigninTimeLimit));
  base::Time last_gaia_signin_time = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kSAMLLastGAIASignInTime));
  if (offline_signin_time_limit < base::TimeDelta() ||
      last_gaia_signin_time.is_null()) {
    // If no limit is in force, return.
    return;
  }

  const base::Time now = clock_->Now();
  if (last_gaia_signin_time > now) {
    // If the time of last login with SAML lies in the future, set it to the
    // current time.
    NOTREACHED();
    last_gaia_signin_time = now;
    prefs->SetInt64(prefs::kSAMLLastGAIASignInTime, now.ToInternalValue());
  }

  const base::TimeDelta time_since_last_gaia_signin =
      now - last_gaia_signin_time;
  if (time_since_last_gaia_signin >= offline_signin_time_limit) {
    // If the limit already expired, set the flag enforcing online login
    // immediately and return.
    ForceOnlineLogin();
    return;
  }

  // Arm |offline_signin_limit_timer_| so that it sets the flag enforcing online
  // login when the limit expires.
  offline_signin_limit_timer_->Start(
      FROM_HERE, offline_signin_time_limit - time_since_last_gaia_signin, this,
      &SAMLOfflineSigninLimiter::ForceOnlineLogin);
}

void SAMLOfflineSigninLimiter::ForceOnlineLogin() {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  if (!user) {
    NOTREACHED();
    return;
  }

  user_manager::UserManager::Get()->SaveForceOnlineSignin(user->GetAccountId(),
                                                          true);
  RecordReauthReason(user->GetAccountId(), ReauthReason::SAML_REAUTH_POLICY);
  offline_signin_limit_timer_->Stop();
}

}  // namespace chromeos
