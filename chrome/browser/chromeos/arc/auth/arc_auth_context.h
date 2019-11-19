// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_CONTEXT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "net/base/backoff_entry.h"

class GaiaAuthFetcher;
class Profile;

namespace signin {
class UbertokenFetcher;
}

namespace arc {

class ArcAuthContext : public GaiaAuthConsumer,
                       public signin::IdentityManager::Observer {
 public:
  // Creates an |ArcAuthContext| for the given |account_id|. This |account_id|
  // must be the |account_id| used by the OAuth Token Service chain.
  // Note: |account_id| can be the Device Account or a Secondary Account stored
  // in Chrome OS Account Manager.
  ArcAuthContext(Profile* profile, const std::string& account_id);
  ~ArcAuthContext() override;

  // Prepares the context. Calling while an inflight operation exists will
  // cancel the inflight operation.
  // On completion, |true| is passed to the callback. On error, |false|
  // is passed.
  using PrepareCallback = base::Callback<void(bool success)>;
  void Prepare(const PrepareCallback& callback);

  // Creates and starts a request to fetch an access token for the given
  // |scopes|. The caller owns the returned request. |callback| will be
  // called with results if the returned request is not deleted.
  std::unique_ptr<signin::AccessTokenFetcher> CreateAccessTokenFetcher(
      const std::string& consumer_name,
      const identity::ScopeSet& scopes,
      signin::AccessTokenFetcher::TokenCallback callback);

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokensLoaded() override;

  // Ubertoken fetch completion callback.
  void OnUbertokenFetchComplete(GoogleServiceAuthError error,
                                const std::string& uber_token);

  // GaiaAuthConsumer:
  void OnMergeSessionSuccess(const std::string& data) override;
  void OnMergeSessionFailure(const GoogleServiceAuthError& error) override;

  // Skips the merge session, instead calling the callback passed to |Prepare()|
  // once the refresh token is available. Use only in testing.
  void SkipMergeSessionForTesting() { skip_merge_session_for_testing_ = true; }

 private:
  void OnRefreshTokenTimeout();

  void StartFetchers();
  void ResetFetchers();
  void OnFetcherError(const GoogleServiceAuthError& error);

  // Unowned pointer.
  Profile* const profile_;
  const std::string account_id_;
  signin::IdentityManager* const identity_manager_;

  // Whether the merge session should be skipped. Set to true only in testing.
  bool skip_merge_session_for_testing_ = false;

  PrepareCallback callback_;
  bool context_prepared_ = false;

  // Defines retry logic in case of transient error.
  net::BackoffEntry retry_backoff_;

  base::OneShotTimer refresh_token_timeout_;
  base::OneShotTimer retry_timeout_;
  std::unique_ptr<GaiaAuthFetcher> merger_fetcher_;
  std::unique_ptr<signin::UbertokenFetcher> ubertoken_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthContext);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_CONTEXT_H_
