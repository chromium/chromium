// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/offline_signin_limiter.h"

#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
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
// This constant value comes from the `GaiaOfflineSigninTimeLimitDays`
// policy's definition.
constexpr int kGaiaOfflineSigninTimeLimitDaysNotSet = -1;

}  // namespace

void OfflineSigninLimiter::SignedIn(UserContext::AuthFlow auth_flow) {
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
    // a SAML IdP. Update the time of last login without SAML. Clear the flag
    // enforcing online login, the flag will be set again when the limit
    // expires. If the limit already expired (e.g. because it was set to zero),
    // the flag will be set again immediately.
    user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id, false);
    prefs->SetTime(prefs::kGaiaLastOnlineSignInTime, clock_->Now());

    UpdateOnlineSigninData(clock_->Now(), GetGaiaNoSamlTimeLimit());
    // Clear the time of last login with SAML.
    prefs->ClearPref(prefs::kSAMLLastGAIASignInTime);
  }

  if (auth_flow == UserContext::AUTH_FLOW_GAIA_WITH_SAML) {
    // The user went through online authentication and GAIA did redirect to a
    // SAML IdP. Update the time of last login with SAML and clear the flag
    // enforcing online login. The flag will be set again when the limit
    // expires. If the limit already expired (e.g. because it was set to zero),
    // the flag will be set again immediately.
    user_manager::UserManager::Get()->SaveForceOnlineSignin(account_id, false);
    prefs->SetTime(prefs::kSAMLLastGAIASignInTime, clock_->Now());

    UpdateOnlineSigninData(clock_->Now(), GetGaiaSamlTimeLimit());
    // Clear the time of last Gaia login without SAML.
    prefs->ClearPref(prefs::kGaiaLastOnlineSignInTime);
  }

  // Start listening for pref changes.
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kSAMLOfflineSigninTimeLimit,
      base::BindRepeating(&OfflineSigninLimiter::UpdateLimit,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kGaiaOfflineSigninTimeLimitDays,
      base::BindRepeating(&OfflineSigninLimiter::UpdateLimit,
                          base::Unretained(this)));
  // Start listening to power state.
  base::PowerMonitor::AddPowerSuspendObserver(this);

  // Start listening to session lock state
  auto* session_manager = session_manager::SessionManager::Get();
  // Extra check as SessionManager may not be initialized in unit tests.
  if (session_manager) {
    session_manager->AddObserver(this);
  }

  // Arm the `offline_signin_limit_timer_` if a limit is in force.
  UpdateLimit();
}

void OfflineSigninLimiter::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  offline_signin_limit_timer_ = std::move(timer);
}

void OfflineSigninLimiter::Shutdown() {
  offline_signin_limit_timer_->Stop();
  pref_change_registrar_.RemoveAll();
}

void OfflineSigninLimiter::OnResume() {
  UpdateLimit();
}

void OfflineSigninLimiter::OnSessionStateChanged() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    UpdateLimit();
  }
}

OfflineSigninLimiter::OfflineSigninLimiter(Profile* profile, base::Clock* clock)
    : profile_(profile),
      clock_(clock ? clock : base::DefaultClock::GetInstance()),
      offline_signin_limit_timer_(std::make_unique<base::OneShotTimer>()) {}

OfflineSigninLimiter::~OfflineSigninLimiter() {
  base::PowerMonitor::RemovePowerSuspendObserver(this);
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->RemoveObserver(this);
  }
}

void OfflineSigninLimiter::UpdateLimit() {
  // Stop the `offline_signin_limit_timer_`.
  offline_signin_limit_timer_->Stop();

  PrefService* prefs = pref_change_registrar_.prefs();

  bool using_saml =
      ProfileHelper::Get()->GetUserByProfile(profile_)->using_saml();

  const base::Optional<base::TimeDelta> offline_signin_time_limit =
      using_saml ? GetGaiaSamlTimeLimit() : GetGaiaNoSamlTimeLimit();
  base::Time last_gaia_signin_time =
      prefs->GetTime(using_saml ? prefs::kSAMLLastGAIASignInTime
                                : prefs::kGaiaLastOnlineSignInTime);

  if (!offline_signin_time_limit.has_value()) {
    UpdateOnlineSigninData(last_gaia_signin_time, base::nullopt);
    // If no limit is in force, return.
    return;
  }

  if (last_gaia_signin_time.is_null()) {
    // If the time of last login is not set, enforce online signin in the next
    // login.
    ForceOnlineLogin();
    return;
  }

  const base::Time now = clock_->Now();
  if (last_gaia_signin_time > now) {
    // If the time of last login lies in the future, set it to the
    // current time.
    NOTREACHED();
    last_gaia_signin_time = now;
    prefs->SetTime(using_saml ? prefs::kSAMLLastGAIASignInTime
                              : prefs::kGaiaLastOnlineSignInTime,
                   now);
  }

  UpdateOnlineSigninData(last_gaia_signin_time, offline_signin_time_limit);
  const base::TimeDelta time_since_last_gaia_signin =
      now - last_gaia_signin_time;
  if (time_since_last_gaia_signin >= offline_signin_time_limit.value()) {
    // If the limit already expired, set the flag enforcing online login
    // immediately and return.
    ForceOnlineLogin();
    return;
  }

  // Arm `offline_signin_limit_timer_` so that it sets the flag enforcing online
  // login when the limit expires.
  // TODO(b/179636755): Use `WallClockTimer` class instead of the
  // `OneShotTimer`.
  offline_signin_limit_timer_->Start(
      FROM_HERE,
      offline_signin_time_limit.value() - time_since_last_gaia_signin, this,
      &OfflineSigninLimiter::ForceOnlineLogin);
}

base::Optional<base::TimeDelta> OfflineSigninLimiter::GetGaiaSamlTimeLimit() {
  // TODO(crbug.com/1177416): Clean up this override once testing is complete.
  auto override_val = GetTimeLimitOverrideForTesting();
  if (override_val.has_value())
    return override_val;

  const int saml_offline_limit =
      profile_->GetPrefs()->GetInteger(prefs::kSAMLOfflineSigninTimeLimit);
  if (saml_offline_limit <= kSAMLOfflineSigninTimeLimitNotSet)
    return base::nullopt;

  return base::make_optional<base::TimeDelta>(
      base::TimeDelta::FromSeconds(saml_offline_limit));
}

base::Optional<base::TimeDelta> OfflineSigninLimiter::GetGaiaNoSamlTimeLimit() {
  // TODO(crbug.com/1177416): Clean up this override once testing is complete.
  auto override_val = GetTimeLimitOverrideForTesting();
  if (override_val.has_value())
    return override_val;

  int no_saml_offline_limit =
      profile_->GetPrefs()->GetInteger(prefs::kGaiaOfflineSigninTimeLimitDays);
  if (no_saml_offline_limit <= kGaiaOfflineSigninTimeLimitDaysNotSet)
    return base::nullopt;

  return base::make_optional<base::TimeDelta>(
      base::TimeDelta::FromDays(no_saml_offline_limit));
}

base::Optional<base::TimeDelta>
OfflineSigninLimiter::GetTimeLimitOverrideForTesting() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOfflineSignInTimeLimitInSecondsOverrideForTesting)) {
    const std::string ascii_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kOfflineSignInTimeLimitInSecondsOverrideForTesting);
    int numeric_val = 0;
    if (base::StringToInt(ascii_value, &numeric_val) && numeric_val >= 0) {
      return base::make_optional<base::TimeDelta>(
          base::TimeDelta::FromSeconds(numeric_val));
    }
    LOG(WARNING)
        << "Manual offline signin time limit override requested but failed.";
  }

  return base::nullopt;
}

void OfflineSigninLimiter::ForceOnlineLogin() {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);

  user_manager::UserManager::Get()->SaveForceOnlineSignin(user->GetAccountId(),
                                                          true);
  // Re-auth on lock - enabled only for the primary user.
  InSessionPasswordSyncManager* password_sync_manager =
      InSessionPasswordSyncManagerFactory::GetForProfile(profile_);
  if (password_sync_manager) {
    password_sync_manager->MaybeForceReauthOnLockScreen(
        InSessionPasswordSyncManager::ReauthenticationReason::kPolicy);
  }
  if (user->using_saml())
    RecordReauthReason(user->GetAccountId(), ReauthReason::SAML_REAUTH_POLICY);
  else
    RecordReauthReason(user->GetAccountId(), ReauthReason::GAIA_REAUTH_POLICY);
  offline_signin_limit_timer_->Stop();
}

void OfflineSigninLimiter::UpdateOnlineSigninData(
    base::Time time,
    base::Optional<base::TimeDelta> limit) {
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
