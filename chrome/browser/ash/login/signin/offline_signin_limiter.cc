// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/offline_signin_limiter.h"

#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager_factory.h"
#include "chrome/browser/ash/login/login_constants.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

void OfflineSigninLimiter::SignedIn(UserContext::AuthFlow auth_flow) {
  PrefService* prefs = profile_->GetPrefs();
  const user_manager::User& user = GetUser();

  const AccountId account_id = user.GetAccountId();
  if (auth_flow == UserContext::AUTH_FLOW_GAIA_WITH_SAML ||
      auth_flow == UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML) {
    // The user went through online authentication. Update the time of last
    // online sign-in and clear the flag enforcing it. The flag will be set
    // again when the limit expires. If the limit already expired (e.g. because
    // it was set to zero), the flag will be set again immediately.
    user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id, false);
    bool using_saml = auth_flow == UserContext::AUTH_FLOW_GAIA_WITH_SAML;
    UpdateOnlineSigninData(clock_->Now(), using_saml
                                              ? GetGaiaSamlTimeLimit()
                                              : GetGaiaNoSamlTimeLimit());
  }

  // Start listening for pref changes.
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kGaiaOfflineSigninTimeLimitDays,
      base::BindRepeating(&OfflineSigninLimiter::UpdateLimit,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSAMLOfflineSigninTimeLimit,
      base::BindRepeating(&OfflineSigninLimiter::UpdateLimit,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kGaiaLockScreenOfflineSigninTimeLimitDays,
      base::BindRepeating(&OfflineSigninLimiter::UpdateLockScreenLimit,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSamlLockScreenOfflineSigninTimeLimitDays,
      base::BindRepeating(&OfflineSigninLimiter::UpdateLockScreenLimit,
                          base::Unretained(this)));
  // Start listening to power state.
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);

  // Start listening to session lock state
  auto* session_manager = session_manager::SessionManager::Get();
  // Extra check as SessionManager may not be initialized in unit tests.
  if (session_manager) {
    session_manager->AddObserver(this);
  }

  // Arm the `offline_signin_limit_timer_` if a limit is in force.
  UpdateLimit();
  // Arm the `offline_lock_screen_signin_limit_timer_` if a limit is in force.
  UpdateLockScreenLimit();
}

base::WallClockTimer* OfflineSigninLimiter::GetTimerForTesting() {
  return offline_signin_limit_timer_.get();
}

base::WallClockTimer* OfflineSigninLimiter::GetLockscreenTimerForTesting() {
  return offline_lock_screen_signin_limit_timer_.get();
}

void OfflineSigninLimiter::Shutdown() {
  offline_signin_limit_timer_->Stop();
  offline_lock_screen_signin_limit_timer_->Stop();
  pref_change_registrar_.RemoveAll();
}

void OfflineSigninLimiter::OnSessionStateChanged() {
  TRACE_EVENT0("login", "OfflineSigninLimiter::OnSessionStateChanged");
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    UpdateLimit();
  } else {
    UpdateLockScreenLimit();
  }
}

OfflineSigninLimiter::OfflineSigninLimiter(Profile* profile,
                                           const base::Clock* clock)
    : profile_(profile),
      clock_(clock ? clock : base::DefaultClock::GetInstance()),
      offline_signin_limit_timer_(std::make_unique<base::WallClockTimer>()),
      offline_lock_screen_signin_limit_timer_(
          std::make_unique<base::WallClockTimer>()) {}

OfflineSigninLimiter::~OfflineSigninLimiter() {
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->RemoveObserver(this);
  }
}

void OfflineSigninLimiter::UpdateLimit() {
  // Stop the `offline_signin_limit_timer_`.
  offline_signin_limit_timer_->Stop();

  const user_manager::User& user = GetUser();
  bool using_saml = user.using_saml();

  const std::optional<base::TimeDelta> offline_signin_time_limit =
      using_saml ? GetGaiaSamlTimeLimit() : GetGaiaNoSamlTimeLimit();
  base::Time last_online_signin_time = GetLastOnlineSigninTime();

  if (!offline_signin_time_limit.has_value()) {
    UpdateOnlineSigninData(last_online_signin_time, std::nullopt);
    // If no limit is in force, return.
    return;
  }

  if (last_online_signin_time.is_null()) {
    // last_online_signin_time should not be null since it gets initialized at
    // the initial user signin. Even in the unlikely case that happens, the way
    // to handle the situation nicely is to enforce online signin next time so
    // the value gets re-initialized. We should not crash here as it may lead to
    // rolling reboot.
    LOG(DFATAL) << "Last signin time is null: forcing reauth.";
    ForceOnlineLogin();
    return;
  }

  const base::Time now = clock_->Now();
  if (last_online_signin_time > now) {
    // If the time of last login lies in the future, set it to the
    // current time and log error.
    LOG(DFATAL) << "Last signin time in the future.";
    last_online_signin_time = now;
  }

  UpdateOnlineSigninData(last_online_signin_time, offline_signin_time_limit);
  const base::TimeDelta time_since_last_online_signin =
      now - last_online_signin_time;
  const base::TimeDelta time_limit_left =
      offline_signin_time_limit.value() - time_since_last_online_signin;

  if (time_limit_left <= base::TimeDelta()) {
    // If the limit already expired, set the flag enforcing online login
    // immediately and return.
    ForceOnlineLogin();
    return;
  }

  // Arm `offline_signin_limit_timer_` so that it sets the flag enforcing online
  // login when the limit expires.
  const base::Time offline_signin_limit = now + time_limit_left;
  offline_signin_limit_timer_->Start(
      FROM_HERE, offline_signin_limit,
      base::BindOnce(&OfflineSigninLimiter::ForceOnlineLogin,
                     base::Unretained(this)));
}

void OfflineSigninLimiter::UpdateLockScreenLimit() {
  // Stop the `offline_lock_screen_signin_limit_timer_`.
  offline_lock_screen_signin_limit_timer_->Stop();

  const user_manager::User& user = GetUser();
  bool using_saml = user.using_saml();

  const std::optional<base::TimeDelta> offline_lock_screen_signin_time_limit =
      using_saml ? GetGaiaSamlLockScreenTimeLimit()
                 : GetGaiaNoSamlLockScreenTimeLimit();

  // This is needed to update the Local State data for the login screen.
  const std::optional<base::TimeDelta> offline_signin_time_limit =
      using_saml ? GetGaiaSamlTimeLimit() : GetGaiaNoSamlTimeLimit();

  base::Time last_online_signin_time = GetLastOnlineSigninTime();

  if (!offline_lock_screen_signin_time_limit.has_value()) {
    // If no limit is in force, return.
    return;
  }

  if (last_online_signin_time.is_null()) {
    // last_online_signin_time should not be null since it gets initialized at
    // the initial user signin. Even in the unlikely case that happens, the way
    // to handle the situation nicely is to enforce online signin next time so
    // the value gets re-initialized. We should not crash here as it may lead to
    // rolling reboot.
    LOG(DFATAL) << "Last signin time is null: forcing reauth.";
    ForceOnlineLockScreenReauth();
    return;
  }

  const base::Time now = clock_->Now();
  if (last_online_signin_time > now) {
    // If the time of last login lies in the future, set it to the
    // current time and log error.
    LOG(DFATAL) << "Last online nlock time in the future";
    last_online_signin_time = now;
  }

  UpdateOnlineSigninData(last_online_signin_time, offline_signin_time_limit);
  const base::TimeDelta time_since_last_online_signin =
      now - last_online_signin_time;
  const base::TimeDelta time_limit_left =
      offline_lock_screen_signin_time_limit.value() -
      time_since_last_online_signin;

  if (time_limit_left <= base::TimeDelta()) {
    // If the limit already expired, set the flag enforcing online login
    // immediately and return.
    ForceOnlineLockScreenReauth();
    return;
  }

  // Arm `offline_lock_screen_signin_limit_timer_` so that it sets the flag
  // enforcing online login when the limit expires.
  const base::Time offline_signin_limit = now + time_limit_left;
  offline_lock_screen_signin_limit_timer_->Start(
      FROM_HERE, offline_signin_limit,
      base::BindOnce(&OfflineSigninLimiter::ForceOnlineLockScreenReauth,
                     base::Unretained(this)));
}

std::optional<base::TimeDelta> OfflineSigninLimiter::GetGaiaNoSamlTimeLimit() {
  int no_saml_offline_limit =
      profile_->GetPrefs()->GetInteger(prefs::kGaiaOfflineSigninTimeLimitDays);
  if (no_saml_offline_limit <= constants::kOfflineSigninTimeLimitNotSet) {
    return std::nullopt;
  }

  return std::make_optional<base::TimeDelta>(base::Days(no_saml_offline_limit));
}

std::optional<base::TimeDelta> OfflineSigninLimiter::GetGaiaSamlTimeLimit() {
  const int saml_offline_limit =
      profile_->GetPrefs()->GetInteger(prefs::kSAMLOfflineSigninTimeLimit);
  if (saml_offline_limit <= constants::kOfflineSigninTimeLimitNotSet) {
    return std::nullopt;
  }

  return std::make_optional<base::TimeDelta>(base::Seconds(saml_offline_limit));
}

std::optional<base::TimeDelta>
OfflineSigninLimiter::GetGaiaNoSamlLockScreenTimeLimit() {
  int no_saml_lock_screen_offline_limit = profile_->GetPrefs()->GetInteger(
      prefs::kGaiaLockScreenOfflineSigninTimeLimitDays);

  if (no_saml_lock_screen_offline_limit ==
      constants::kLockScreenOfflineSigninTimeLimitDaysMatchLogin) {
    no_saml_lock_screen_offline_limit = profile_->GetPrefs()->GetInteger(
        prefs::kGaiaOfflineSigninTimeLimitDays);
  }

  if (no_saml_lock_screen_offline_limit <=
      constants::kOfflineSigninTimeLimitNotSet) {
    return std::nullopt;
  }

  return std::make_optional<base::TimeDelta>(
      base::Days(no_saml_lock_screen_offline_limit));
}

std::optional<base::TimeDelta>
OfflineSigninLimiter::GetGaiaSamlLockScreenTimeLimit() {
  int saml_lock_screen_offline_limit = profile_->GetPrefs()->GetInteger(
      prefs::kSamlLockScreenOfflineSigninTimeLimitDays);

  if (saml_lock_screen_offline_limit ==
      constants::kLockScreenOfflineSigninTimeLimitDaysMatchLogin) {
    saml_lock_screen_offline_limit =
        profile_->GetPrefs()->GetInteger(prefs::kSAMLOfflineSigninTimeLimit);
  }

  if (saml_lock_screen_offline_limit <=
      constants::kOfflineSigninTimeLimitNotSet) {
    return std::nullopt;
  }

  return std::make_optional<base::TimeDelta>(
      base::Days(saml_lock_screen_offline_limit));
}

void OfflineSigninLimiter::ForceOnlineLogin() {
  const user_manager::User& user = GetUser();

  user_manager::UserManager::Get()->SaveForceOnlineSignin(user.GetAccountId(),
                                                          true);
  if (user.using_saml()) {
    RecordReauthReason(user.GetAccountId(), ReauthReason::kSamlReauthPolicy);
  } else {
    RecordReauthReason(user.GetAccountId(), ReauthReason::kGaiaReauthPolicy);
  }
  offline_signin_limit_timer_->Stop();
}

void OfflineSigninLimiter::ForceOnlineLockScreenReauth() {
  const user_manager::User& user = GetUser();
  ReauthReason reauth_reason = ReauthReason::kNone;
  if (user.using_saml()) {
    reauth_reason = ReauthReason::kSamlLockScreenReauthPolicy;
  } else {
    reauth_reason = ReauthReason::kGaiaLockScreenReauthPolicy;
  }

  LockScreenReauthManager* lock_screen_reauth_manager =
      LockScreenReauthManagerFactory::GetForProfile(profile_);
  DCHECK(lock_screen_reauth_manager);
  lock_screen_reauth_manager->MaybeForceReauthOnLockScreen(reauth_reason);
  RecordReauthReason(user.GetAccountId(), reauth_reason);
  offline_lock_screen_signin_limit_timer_->Stop();
}

void OfflineSigninLimiter::UpdateOnlineSigninData(
    base::Time time,
    std::optional<base::TimeDelta> limit) {
  const user_manager::User& user = GetUser();

  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetLastOnlineSignin(user.GetAccountId(), time);
  known_user.SetOfflineSigninLimit(user.GetAccountId(), limit);
}

base::Time OfflineSigninLimiter::GetLastOnlineSigninTime() {
  const user_manager::User& user = GetUser();

  user_manager::KnownUser known_user(g_browser_process->local_state());
  return known_user.GetLastOnlineSignin(user.GetAccountId());
}

const user_manager::User& OfflineSigninLimiter::GetUser() {
  return CHECK_DEREF(ProfileHelper::Get()->GetUserByProfile(profile_));
}

}  // namespace ash
