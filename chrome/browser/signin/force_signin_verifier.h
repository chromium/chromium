// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_
#define CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

extern const char kForceSigninVerificationMetricsName[];
extern const char kForceSigninVerificationSuccessTimeMetricsName[];
extern const char kForceSigninVerificationFailureTimeMetricsName[];

// ForceSigninVerifier will verify profile's auth token when profile is loaded
// into memory by the first time via gaia server. It will retry on any transient
// error.
class ForceSigninVerifier
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit ForceSigninVerifier(signin::IdentityManager* identity_manager);
  ~ForceSigninVerifier() override;

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  // override network::NetworkConnectionTracker::NetworkConnectionObserver
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Cancel any pending or ongoing verification.
  void Cancel();

  // Return the value of |has_token_verified_|.
  bool HasTokenBeenVerified();

 protected:
  // Send the token verification request. The request will be sent only if
  //   - The token has never been verified before.
  //   - There is no on going verification.
  //   - There is network connection.
  //   - The profile has signed in.
  //
  void SendRequest();

  // Send the request if |network_type| is not CONNECTION_NONE and
  // ShouldSendRequest returns true.
  void SendRequestIfNetworkAvailable(
      network::mojom::ConnectionType network_type);

  bool ShouldSendRequest();

  virtual void CloseAllBrowserWindows();

  signin::PrimaryAccountAccessTokenFetcher* GetAccessTokenFetcherForTesting();
  net::BackoffEntry* GetBackoffEntryForTesting();
  base::OneShotTimer* GetOneShotTimerForTesting();

 private:
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Indicates whether the verification is finished successfully or with a
  // persistent error.
  bool has_token_verified_ = false;
  net::BackoffEntry backoff_entry_;
  base::OneShotTimer backoff_request_timer_;
  base::TimeTicks creation_time_;

  signin::IdentityManager* identity_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ForceSigninVerifier);
};

#endif  // CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_
