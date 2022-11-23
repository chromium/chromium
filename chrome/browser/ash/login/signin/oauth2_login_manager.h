// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_LOGIN_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_LOGIN_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/signin/oauth2_login_verifier.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class GoogleServiceAuthError;
class Profile;

namespace ash {

// This class is responsible for restoring authenticated web sessions out of
// OAuth2 refresh tokens or pre-authenticated cookie jar.
class OAuth2LoginManager : public KeyedService,
                           public OAuth2LoginVerifier::Delegate,
                           public signin::IdentityManager::Observer {
 public:
  // Session restore states.
  enum SessionRestoreState {
    // Session restore is not started.
    SESSION_RESTORE_NOT_STARTED = 0,
    // Session restore is being prepared.
    SESSION_RESTORE_PREPARING = 1,
    // Session restore is in progress. We are currently issuing calls to verify
    // stored OAuth tokens and populate cookie jar with GAIA credentials.
    SESSION_RESTORE_IN_PROGRESS = 2,
    // Session restore is completed.
    SESSION_RESTORE_DONE = 3,
    // Session restore failed.
    SESSION_RESTORE_FAILED = 4,
    // Session restore failed due to connection or service errors.
    SESSION_RESTORE_CONNECTION_FAILED = 5,
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // Raised when merge session state changes.
    virtual void OnSessionRestoreStateChanged(Profile* user_profile,
                                              SessionRestoreState state) {}

    // Raised when session's GAIA credentials (SID+LSID) are available to
    // other signed in services.
    virtual void OnSessionAuthenticated(Profile* user_profile) {}
  };

  explicit OAuth2LoginManager(Profile* user_profile);

  OAuth2LoginManager(const OAuth2LoginManager&) = delete;
  OAuth2LoginManager& operator=(const OAuth2LoginManager&) = delete;

  ~OAuth2LoginManager() override;

  void AddObserver(OAuth2LoginManager::Observer* observer);
  void RemoveObserver(OAuth2LoginManager::Observer* observer);

  // Restores and verifies OAuth tokens.
  void RestoreSession(
      const std::string& oauth2_access_token);

  // Continues session restore after transient network errors.
  void ContinueSessionRestore();

  // Start restoring session from saved OAuth2 refresh token.
  void RestoreSessionFromSavedTokens();

  // Stops all background authentication requests.
  void Stop();

  // Returns session restore state.
  SessionRestoreState state() { return state_; }

  const base::Time& session_restore_start() { return session_restore_start_; }

  bool SessionRestoreIsRunning() const;

  // Returns true if the tab loading should block until session restore
  // finishes.
  bool ShouldBlockTabLoading() const;

 private:
  friend class MergeSessionNavigationThrottleTest;
  friend class OAuth2Test;

  // Session restore outcomes (for UMA).
  enum SessionRestoreOutcome {
    SESSION_RESTORE_UNDEFINED = 0,
    SESSION_RESTORE_SUCCESS = 1,
    SESSION_RESTORE_TOKEN_FETCH_FAILED = 2,
    SESSION_RESTORE_NO_REFRESH_TOKEN_FAILED = 3,
    SESSION_RESTORE_OAUTHLOGIN_FAILED = 4,
    SESSION_RESTORE_MERGE_SESSION_FAILED = 5,
    SESSION_RESTORE_LISTACCOUNTS_FAILED = 6,
    SESSION_RESTORE_NOT_NEEDED = 7,
    SESSION_RESTORE_COUNT = 8,
  };

  // Outcomes of post-merge session verification.
  // This enum is used for an UMA histogram, and hence new items should only be
  // appended at the end.
  enum MergeVerificationOutcome {
    POST_MERGE_UNDEFINED = 0,
    POST_MERGE_SUCCESS = 1,
    POST_MERGE_NO_ACCOUNTS = 2,
    POST_MERGE_MISSING_PRIMARY_ACCOUNT = 3,
    POST_MERGE_PRIMARY_NOT_FIRST_ACCOUNT = 4,
    POST_MERGE_VERIFICATION_FAILED = 5,
    POST_MERGE_CONNECTION_FAILED = 6,
    POST_MERGE_COUNT = 7,
  };

  // KeyedService implementation.
  void Shutdown() override;

  // OAuth2LoginVerifier::Delegate overrides.
  void OnSessionMergeSuccess() override;
  void OnSessionMergeFailure(bool connection_error) override;
  void OnListAccountsSuccess(
      const std::vector<gaia::ListedAccount>& accounts) override;
  void OnListAccountsFailure(bool connection_error) override;

  // signin::IdentityManager::Observer implementation:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  // Signals delegate that authentication is completed, kicks off token fetching
  // process.
  void CompleteAuthentication();

  // Retrieves IdentityManager for `user_profile_`.
  signin::IdentityManager* GetIdentityManager();

  // Retrieves the primary account ID for `user_profile_`.
  CoreAccountId GetUnconsentedPrimaryAccountId();

  // Checks if primary account sessions cookies are stale and restores them
  // if needed.
  void VerifySessionCookies();

  // Issue GAIA cookie recovery (MergeSession) from `refresh_token_`.
  void RestoreSessionCookies();

  // Checks GAIA error and figures out whether the request should be
  // re-attempted.
  bool RetryOnError(const GoogleServiceAuthError& error);

  // Changes `state_`, if needed fires observers (OnSessionRestoreStateChanged).
  void SetSessionRestoreState(SessionRestoreState state);

  // Testing helper.
  void SetSessionRestoreStartForTesting(const base::Time& time);

  // Records `outcome` of session restore process and sets new `state`.
  void RecordSessionRestoreOutcome(SessionRestoreOutcome outcome,
                                   SessionRestoreState state);

  // Records `outcome` of merge verification check. `is_pre_merge` specifies
  // if this is pre or post merge session verification.
  static void RecordCookiesCheckOutcome(bool is_pre_merge,
                                        MergeVerificationOutcome outcome);

  Profile* user_profile_;
  SessionRestoreState state_;

  // Whether there is pending TokenService::LoadCredentials call.
  bool pending_token_service_load_ = false;

  std::unique_ptr<OAuth2LoginVerifier> login_verifier_;

  // OAuthLogin scoped access token.
  std::string oauthlogin_access_token_;

  // Session restore start time.
  base::Time session_restore_start_;

  // List of observers to notify when token availability changes.
  // Makes sure list is empty on destruction.
  // TODO(zelidrag|gspencer): Figure out how to get rid of ProfileHelper so we
  // can change the line below to base::ObserverList<Observer, true>.
  base::ObserverList<Observer, false>::Unchecked observer_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_OAUTH2_LOGIN_MANAGER_H_
