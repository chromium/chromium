// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_OAUTH_TOKEN_FETCHER_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_OAUTH_TOKEN_FETCHER_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace ash {

// Callback that is going to be called when OAuth token request is resolved.
// The first argument is the email for which the token request is being made.
// The error state and access token info are provided by IdentityManager for the
// OAuth token request that was made.
using AccessTokenRequestCallback =
    base::OnceCallback<void(const std::string&,
                            GoogleServiceAuthError error,
                            const signin::AccessTokenInfo&)>;

// When a request comes for an OAuth token for an account, an instance of this
// class is created. If more requests come for the account while the original
// request is pending, this class buffers them in `callbacks` to be resolved
// when the fetch finishes.
class AccessTokenRequests {
 public:
  AccessTokenRequests();
  AccessTokenRequests(AccessTokenRequests&&);
  AccessTokenRequests& operator=(AccessTokenRequests&&);
  ~AccessTokenRequests();

  std::vector<AccessTokenRequestCallback> callbacks;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher;
};

// Class that handles the need of the Projector SWA from IdentityManager. It has
// the responsibility of providing the list of available acccounts and providing
// appropriately scopped OAuth tokens for the Projector SWA. The class caches
// the tokens until they expire so that it may synchonously resolve some
// requests that are made.
class ProjectorOAuthTokenFetcher {
 public:
  ProjectorOAuthTokenFetcher();
  ProjectorOAuthTokenFetcher(const ProjectorOAuthTokenFetcher&) = delete;
  ProjectorOAuthTokenFetcher& operator=(const ProjectorOAuthTokenFetcher&) =
      delete;
  ~ProjectorOAuthTokenFetcher();

  // Returns the list of accounts, primary and secondary accounts, for the
  // Projector SWA to use.
  static std::vector<AccountInfo> GetAccounts();

  // Returns the CoreAccountInfo for the primary account.
  static CoreAccountInfo GetPrimaryAccountInfo();

  // If an unexpired access token is present for the email, synchronously
  // executes the callback with the cached OAuth token. Otherwise, creates a
  // signin::AccessTokenFetcher to fetch the requested OAuth token and caches
  // the callback to be executed when fetching completes.
  void GetAccessTokenFor(const std::string& email,
                         AccessTokenRequestCallback callback);

  // Remove the given token in cache.
  void InvalidateToken(const std::string& token);

  // Returns true if there exists a cached token for account with `email`.
  bool HasCachedTokenForTest(const std::string& email);
  bool HasPendingRequestForTest(const std::string& email);

 private:
  void InitiateAccessTokenFetchFor(const std::string& email,
                                   AccessTokenRequestCallback callback);

  // Executed when an OAuth token fetch either completes or fails
  void OnAccessTokenRequestCompleted(const std::string& email,
                                     GoogleServiceAuthError error,
                                     signin::AccessTokenInfo info);

  // Keeps pending requests to fetch access tokens associated with an account.
  // When account fetching is successful, each request is resolved in FIFO
  // manner.
  base::flat_map<std::string, AccessTokenRequests> pending_oauth_token_fetch_;

  // Keeps the access tokens associated with an account. If an unexpired access
  // token is present, then requests to get access token are synchronously
  // resolved.
  base::flat_map<std::string, signin::AccessTokenInfo> fetched_access_tokens_;
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_OAUTH_TOKEN_FETCHER_H_
