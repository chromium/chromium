// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// Returns `true` if `AccountReconcilor` is in a terminal `state`.
// A terminal state is defined as a state which is not going to change unless a
// reconciliation cycle is run again or if `AccountReconcilor` is enabled /
// unblocked.
bool IsTerminalState(const signin_metrics::AccountReconcilorState& state) {
  switch (state) {
    case signin_metrics::AccountReconcilorState::kOk:
    case signin_metrics::AccountReconcilorState::kError:
      return true;
    case signin_metrics::AccountReconcilorState::kRunning:
    case signin_metrics::AccountReconcilorState::kScheduled:
    case signin_metrics::AccountReconcilorState::kInactive:
      // Note: We do not consider
      // `signin_metrics::AccountReconcilorState::kInactive` a terminal state
      // here because `AccountReconcilor` can be unblocked / enabled later and
      // change its state.
      return false;
  }
}

}  // namespace

OAuth2LoginManager::OAuth2LoginManager(Profile* user_profile)
    : user_profile_(user_profile),
      state_(SESSION_RESTORE_NOT_STARTED) {
  GetIdentityManager()->AddObserver(this);

  // For telemetry, we mark session restore completed to avoid warnings from
  // MergeSessionThrottle.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGaiaServices)) {
    SetSessionRestoreState(SESSION_RESTORE_DONE);
  }
}

OAuth2LoginManager::~OAuth2LoginManager() = default;

void OAuth2LoginManager::AddObserver(OAuth2LoginManager::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OAuth2LoginManager::RemoveObserver(
    OAuth2LoginManager::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void OAuth2LoginManager::RestoreSession(
    const std::string& oauth2_access_token) {
  DCHECK(user_profile_);
  oauthlogin_access_token_ = oauth2_access_token;
  session_restore_start_ = base::Time::Now();
  ContinueSessionRestore();
}

void OAuth2LoginManager::ContinueSessionRestore() {
  SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_PREPARING);

  CheckIfTokensHaveBeenLoaded();

  // ContinueSessionRestore could be called multiple times when network
  // connection changes. Only add observation once.
  if (!account_reconcilor_observation_.IsObserving()) {
    account_reconcilor_observation_.Observe(GetAccountReconcilor());
  }

  const signin_metrics::AccountReconcilorState state =
      GetAccountReconcilor()->GetState();
  if (IsTerminalState(state)) {
    // We are not going to receive any notifications from `AccountReconcilor`.
    // Process its current state.
    OnStateChanged(state);
  } else {
    // `AccountReconcilor` is still minting cookies. Set the session restore
    // state in progress and wait for `AccountReconcilor` to reach a terminal
    // state.
    SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS);
  }
}

void OAuth2LoginManager::CheckIfTokensHaveBeenLoaded() {
  signin::IdentityManager* identity_manager = GetIdentityManager();

  if (identity_manager->AreRefreshTokensLoaded() &&
      identity_manager->HasAccountWithRefreshToken(
          GetUnconsentedPrimaryAccountId())) {
    // Tokens have been loaded in `IdentityManager`. Nothing to do.
    // `OnRefreshTokenUpdatedForAccount()` / `OnStateChanged()` will handle the
    // respective callbacks from `IdentityManager` / `AccountReconcilor` about
    // token loading / cookie minting respectively.
    return;
  }

  VLOG(1) << "Waiting for OAuth2 refresh token being loaded from database.";
  SetSessionRestoreState(OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS);

  // Flag user with unknown token status in case there are no saved tokens and
  // `OnRefreshTokenUpdatedForAccount()` is not called. Flagging it here would
  // cause user to go through Gaia in next login to obtain a new refresh
  // token.
  const CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (!account_info.IsEmpty()) {
    // Primary account is empty when Active Directory accounts are used.
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        AccountIdFromAccountInfo(account_info),
        user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN);
  }
}

bool OAuth2LoginManager::SessionRestoreIsRunning() const {
  return state_ == SESSION_RESTORE_PREPARING ||
         state_ == SESSION_RESTORE_IN_PROGRESS;
}

bool OAuth2LoginManager::ShouldBlockTabLoading() const {
  return SessionRestoreIsRunning();
}

void OAuth2LoginManager::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  VLOG(1) << "OnRefreshTokenUpdatedForAccount";

  if (state_ == SESSION_RESTORE_NOT_STARTED) {
    return;
  }

  // Only restore session cookies for the primary account in the profile.
  if (GetUnconsentedPrimaryAccountId() != account_info.account_id) {
    return;
  }

  // Token is loaded. Undo the flagging before token loading.
  DCHECK(!account_info.gaia.empty());
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountIdFromAccountInfo(account_info),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
}

void OAuth2LoginManager::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  if (!IsTerminalState(state)) {
    // A reconciliation cycle is still running and `AccountReconcilor` is
    // bouncing between its states. Simply ignore these notifications until we
    // have reached a terminal state.
    return;
  }

  // We have reached a terminal state. Stop observing `AccountReconcilor` and
  // process the end state.
  account_reconcilor_observation_.Reset();

  SessionRestoreOutcome session_restore_outcome = SESSION_RESTORE_SUCCESS;
  SessionRestoreState session_restore_state = SESSION_RESTORE_DONE;
  switch (state) {
    case signin_metrics::AccountReconcilorState::kOk:
      // Use the default values defined above.
      break;
    case signin_metrics::AccountReconcilorState::kError:
      session_restore_outcome = SESSION_RESTORE_MERGE_SESSION_FAILED;
      session_restore_state =
          GetAccountReconcilor()->GetReconcileError().IsTransientError()
              ? SESSION_RESTORE_CONNECTION_FAILED
              : SESSION_RESTORE_FAILED;
      break;
    case signin_metrics::AccountReconcilorState::kRunning:
    case signin_metrics::AccountReconcilorState::kScheduled:
    case signin_metrics::AccountReconcilorState::kInactive:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  RecordSessionRestoreOutcome(session_restore_outcome, session_restore_state);
}

signin::IdentityManager* OAuth2LoginManager::GetIdentityManager() {
  return IdentityManagerFactory::GetForProfile(user_profile_);
}

AccountReconcilor* OAuth2LoginManager::GetAccountReconcilor() {
  return AccountReconcilorFactory::GetForProfile(user_profile_);
}

CoreAccountId OAuth2LoginManager::GetUnconsentedPrimaryAccountId() {
  // Use the primary ID whether or not the user has consented to browser sync.
  const CoreAccountId primary_account_id =
      GetIdentityManager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  LOG_IF(ERROR, primary_account_id.empty()) << "Primary account id is empty.";
  return primary_account_id;
}

void OAuth2LoginManager::Shutdown() {
  GetIdentityManager()->RemoveObserver(this);
  account_reconcilor_observation_.Reset();
}

void OAuth2LoginManager::RecordSessionRestoreOutcome(
    SessionRestoreOutcome outcome,
    OAuth2LoginManager::SessionRestoreState state) {
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore", outcome,
                            SESSION_RESTORE_COUNT);
  SetSessionRestoreState(state);
}

void OAuth2LoginManager::SetSessionRestoreState(
    OAuth2LoginManager::SessionRestoreState state) {
  if (state_ == state)
    return;

  state_ = state;
  if (state == OAuth2LoginManager::SESSION_RESTORE_FAILED) {
    UMA_HISTOGRAM_TIMES("OAuth2Login.SessionRestoreTimeToFailure",
                        base::Time::Now() - session_restore_start_);
  } else if (state == OAuth2LoginManager::SESSION_RESTORE_DONE) {
    UMA_HISTOGRAM_TIMES("OAuth2Login.SessionRestoreTimeToSuccess",
                        base::Time::Now() - session_restore_start_);
  }

  for (auto& observer : observer_list_)
    observer.OnSessionRestoreStateChanged(user_profile_, state_);
}

void OAuth2LoginManager::SetSessionRestoreStartForTesting(
    const base::Time& time) {
  session_restore_start_ = time;
}

}  // namespace ash
