// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"

namespace ash {

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

OAuth2LoginManager::~OAuth2LoginManager() {}

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

  RestoreSessionFromSavedTokens();
}

void OAuth2LoginManager::RestoreSessionFromSavedTokens() {
  signin::IdentityManager* identity_manager = GetIdentityManager();
  if (identity_manager->AreRefreshTokensLoaded() &&
      identity_manager->HasAccountWithRefreshToken(
          GetUnconsentedPrimaryAccountId())) {
    VLOG(1) << "OAuth2 refresh token is already loaded.";
    VerifySessionCookies();
  } else {
    VLOG(1) << "Waiting for OAuth2 refresh token being loaded from database.";

    // Flag user with unknown token status in case there are no saved tokens
    // and OnRefreshTokenAvailable is not called. Flagging it here would
    // cause user to go through Gaia in next login to obtain a new refresh
    // token.
    CoreAccountInfo account_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (!account_info.IsEmpty()) {
      // Primary account is empty when Active Directory accounts are used.
      user_manager::UserManager::Get()->SaveUserOAuthStatus(
          AccountIdFromAccountInfo(account_info),
          user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN);
    }
  }
}

void OAuth2LoginManager::Stop() {
  login_verifier_.reset();
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

  if (state_ == SESSION_RESTORE_NOT_STARTED)
    return;

  // TODO(fgorski): Once ProfileOAuth2TokenService supports multi-login, make
  // sure to restore session cookies in the context of the correct user_email.

  // Only restore session cookies for the primary account in the profile.
  if (GetUnconsentedPrimaryAccountId() == account_info.account_id) {
    // The refresh token has changed, so stop any ongoing actions that were
    // based on the old refresh token.
    Stop();

    // Token is loaded. Undo the flagging before token loading.
    DCHECK(!account_info.gaia.empty());
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        AccountIdFromAccountInfo(account_info),
        user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

    VerifySessionCookies();
  }
}

signin::IdentityManager* OAuth2LoginManager::GetIdentityManager() {
  return IdentityManagerFactory::GetForProfile(user_profile_);
}

CoreAccountId OAuth2LoginManager::GetUnconsentedPrimaryAccountId() {
  // Use the primary ID whether or not the user has consented to browser sync.
  const CoreAccountId primary_account_id =
      GetIdentityManager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  LOG_IF(ERROR, primary_account_id.empty()) << "Primary account id is empty.";
  return primary_account_id;
}

void OAuth2LoginManager::VerifySessionCookies() {
  if (!login_verifier_) {
    login_verifier_ = std::make_unique<OAuth2LoginVerifier>(
        this, GetIdentityManager(), GetUnconsentedPrimaryAccountId(),
        oauthlogin_access_token_);
  }

  login_verifier_->VerifyUserCookies();
}

void OAuth2LoginManager::RestoreSessionCookies() {
  SetSessionRestoreState(SESSION_RESTORE_IN_PROGRESS);
  login_verifier_->VerifyProfileTokens();
}

void OAuth2LoginManager::Shutdown() {
  GetIdentityManager()->RemoveObserver(this);
  login_verifier_.reset();
}

void OAuth2LoginManager::OnSessionMergeSuccess() {
  VLOG(1) << "OAuth2 refresh and/or GAIA token verification succeeded.";
  RecordSessionRestoreOutcome(SESSION_RESTORE_SUCCESS, SESSION_RESTORE_DONE);
}

void OAuth2LoginManager::OnSessionMergeFailure(bool connection_error) {
  LOG(ERROR) << "OAuth2 refresh and GAIA token verification failed!"
             << " connection_error: " << connection_error;
  RecordSessionRestoreOutcome(SESSION_RESTORE_MERGE_SESSION_FAILED,
                              connection_error
                                  ? SESSION_RESTORE_CONNECTION_FAILED
                                  : SESSION_RESTORE_FAILED);
}

void OAuth2LoginManager::OnListAccountsSuccess(
    const std::vector<gaia::ListedAccount>& accounts) {
  MergeVerificationOutcome outcome = POST_MERGE_SUCCESS;
  // Let's analyze which accounts we see logged in here:
  CoreAccountId user_account_id = GetUnconsentedPrimaryAccountId();
  if (!accounts.empty()) {
    bool found = false;
    bool first = true;
    for (std::vector<gaia::ListedAccount>::const_iterator iter =
             accounts.begin();
         iter != accounts.end(); ++iter) {
      if (iter->id == user_account_id) {
        found = iter->valid;
        break;
      }

      first = false;
    }

    if (!found)
      outcome = POST_MERGE_MISSING_PRIMARY_ACCOUNT;
    else if (!first)
      outcome = POST_MERGE_PRIMARY_NOT_FIRST_ACCOUNT;

  } else {
    outcome = POST_MERGE_NO_ACCOUNTS;
  }

  bool is_pre_merge = (state_ == SESSION_RESTORE_PREPARING);
  RecordCookiesCheckOutcome(is_pre_merge, outcome);
  // If the primary account is missing during the initial cookie freshness
  // check, try to restore GAIA session cookies form the OAuth2 tokens.
  if (is_pre_merge) {
    if (outcome != POST_MERGE_SUCCESS &&
        outcome != POST_MERGE_PRIMARY_NOT_FIRST_ACCOUNT) {
      RestoreSessionCookies();
    } else {
      // We are done with this account, it's GAIA cookies are legit.
      RecordSessionRestoreOutcome(SESSION_RESTORE_NOT_NEEDED,
                                  SESSION_RESTORE_DONE);
    }
  }
}

void OAuth2LoginManager::OnListAccountsFailure(bool connection_error) {
  bool is_pre_merge = (state_ == SESSION_RESTORE_PREPARING);
  RecordCookiesCheckOutcome(is_pre_merge, connection_error
                                              ? POST_MERGE_CONNECTION_FAILED
                                              : POST_MERGE_VERIFICATION_FAILED);
  if (is_pre_merge) {
    if (!connection_error) {
      // If we failed to get account list, our cookies might be stale so we
      // need to attempt to restore them.
      RestoreSessionCookies();
    } else {
      RecordSessionRestoreOutcome(SESSION_RESTORE_LISTACCOUNTS_FAILED,
                                  SESSION_RESTORE_CONNECTION_FAILED);
    }
  }
}

void OAuth2LoginManager::RecordSessionRestoreOutcome(
    SessionRestoreOutcome outcome,
    OAuth2LoginManager::SessionRestoreState state) {
  UMA_HISTOGRAM_ENUMERATION("OAuth2Login.SessionRestore", outcome,
                            SESSION_RESTORE_COUNT);
  SetSessionRestoreState(state);
}

// static
void OAuth2LoginManager::RecordCookiesCheckOutcome(
    bool is_pre_merge,
    MergeVerificationOutcome outcome) {
  if (is_pre_merge) {
    UMA_HISTOGRAM_ENUMERATION("OAuth2Login.PreMergeVerification", outcome,
                              POST_MERGE_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("OAuth2Login.PostMergeVerification", outcome,
                              POST_MERGE_COUNT);
  }
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
