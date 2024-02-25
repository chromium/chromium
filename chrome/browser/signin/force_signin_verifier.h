// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_
#define CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class Profile;

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

// ForceSigninVerifier will verify profile's auth token when profile is loaded
// into memory by the first time via gaia server. It will retry on any transient
// error.
class ForceSigninVerifier
    : public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public signin::IdentityManager::Observer {
 public:
  explicit ForceSigninVerifier(
      Profile* profile,
      signin::IdentityManager* identity_manager,
      base::OnceCallback<void(bool)> on_token_fetch_complete);

  ForceSigninVerifier(const ForceSigninVerifier&) = delete;
  ForceSigninVerifier& operator=(const ForceSigninVerifier&) = delete;

  ~ForceSigninVerifier() override;

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  // override network::NetworkConnectionTracker::NetworkConnectionObserver
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Cancel any pending or ongoing verification.
  void Cancel();

  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 protected:
  // Send the token verification request. The request will be sent only if
  //   - The token has never been verified before.
  //   - There is no on going verification.
  //   - There is network connection.
  //   - The profile has signed in.
  //   - The identity manager has loaded the refresh tokens.
  void SendRequest();

  // Send the request if |network_type| is not CONNECTION_NONE and
  // ShouldSendRequest returns true.
  void SendRequestIfNetworkAvailable(
      network::mojom::ConnectionType network_type);

  bool ShouldSendRequest();

  signin::PrimaryAccountAccessTokenFetcher* GetAccessTokenFetcherForTesting();
  net::BackoffEntry* GetBackoffEntryForTesting();
  base::OneShotTimer* GetOneShotTimerForTesting();
  bool GetRequestIsWaitingForRefreshTokensForTesting() const;

 private:
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Indicates whether the verification is finished successfully or with a
  // persistent error.
  bool has_token_verified_ = false;
  net::BackoffEntry backoff_entry_;
  base::OneShotTimer backoff_request_timer_;
  base::TimeTicks creation_time_;
  // Used to check if the refresh tokens were valid when requesting the signin
  // call. If not, on next `OnRefreshTokensLoaded()` the request will be sent.
  bool request_waiting_for_refresh_tokens_ = false;

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  base::OnceCallback<void(bool)> on_token_fetch_complete_;

  // We need this observer in order to reset the value of the reference
  // to the `identity_manager_`.
  // `ForceSigninVerifier` instance lives in `ChromeSigninClient`, for which
  // `IdentityManager` already has a dependency. Therefore we cannot add a
  // regular KeyedService factory as it would create a circular dependency.
  // Also observing the refresh tokens validity to send the request.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer{this};

  base::WeakPtrFactory<ForceSigninVerifier> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_
