// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_offline_signin_limiter.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/login_pref_names.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/chromeos/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

namespace {

constexpr int kSAMLOfflineSigninTimeLimitNotSet = -1;

}

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
    prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_->Now());
    const int saml_offline_limit =
        prefs->GetInteger(prefs::kSAMLOfflineSigninTimeLimit);
    UpdateOnlineSigninData(
        clock_->Now(), saml_offline_limit == kSAMLOfflineSigninTimeLimitNotSet
                           ? base::TimeDelta()
                           : base::TimeDelta::FromSeconds(saml_offline_limit));
  }

  // Start listening for pref changes.
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kSAMLOfflineSigninTimeLimit,
                             base::Bind(&SAMLOfflineSigninLimiter::UpdateLimit,
                                        base::Unretained(this)));

  // Start listening to power state.
  base::PowerMonitor::AddObserver(this);

  // Start listening to session lock state
  auto* session_manager = session_manager::SessionManager::Get();
  // Extra check as SessionManager may not be initialized in unit tests.
  if (session_manager) {
    session_manager->AddObserver(this);
  }

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

void SAMLOfflineSigninLimiter::OnSessionStateChanged() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    UpdateLimit();
  }
}

SAMLOfflineSigninLimiter::SAMLOfflineSigninLimiter(Profile* profile,
                                                   base::Clock* clock)
    : profile_(profile),
      clock_(clock ? clock : base::DefaultClock::GetInstance()),
      offline_signin_limit_timer_(std::make_unique<base::OneShotTimer>()) {}

SAMLOfflineSigninLimiter::~SAMLOfflineSigninLimiter() {
  base::PowerMonitor::RemoveObserver(this);
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->RemoveObserver(this);
  }
}

void SAMLOfflineSigninLimiter::UpdateLimit() {
  // Stop the |offline_signin_limit_timer_|.
  offline_signin_limit_timer_->Stop();

  PrefService* prefs = pref_change_registrar_.prefs();
  const base::TimeDelta offline_signin_time_limit =
      base::TimeDelta::FromSeconds(
          prefs->GetInteger(prefs::kSAMLOfflineSigninTimeLimit));
  base::Time last_gaia_signin_time =
      prefs->GetTime(prefs::kSAMLLastGAIASignInTime);
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
    prefs->SetTime(prefs::kSAMLLastGAIASignInTime, now);
    UpdateOnlineSigninData(now, offline_signin_time_limit);
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
  DCHECK(user);

  user_manager::UserManager::Get()->SaveForceOnlineSignin(user->GetAccountId(),
                                                          true);
  // Re-auth on lock - enabled only for the primary user.
  InSessionPasswordSyncManager* password_sync_manager =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile_);
  if (password_sync_manager && password_sync_manager->IsLockReauthEnabled()) {
    password_sync_manager->MaybeForceReauthOnLockScreen(
        InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  }
  RecordReauthReason(user->GetAccountId(), ReauthReason::SAML_REAUTH_POLICY);
  offline_signin_limit_timer_->Stop();
}

void SAMLOfflineSigninLimiter::UpdateOnlineSigninData(base::Time time,
                                                      base::TimeDelta limit) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  if (!user) {
    NOTREACHED();
    return;
  }

  user_manager::known_user::SetLastOnlineSignin(user->GetAccountId(), time);
  user_manager::known_user::SetOfflineSigninLimit(user->GetAccountId(), limit);
}

}  // namespace chromeos
