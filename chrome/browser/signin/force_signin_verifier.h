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
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class Profile;
class SigninManager;

extern const char kForceSigninVerificationMetricsName[];
extern const char kForceSigninVerificationSuccessTimeMetricsName[];
extern const char kForceSigninVerificationFailureTimeMetricsName[];

// ForceSigninVerifier will verify profile's auth token when profile is loaded
// into memory by the first time via gaia server. It will retry on any transient
// error.
class ForceSigninVerifier
    : public OAuth2TokenService::Consumer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit ForceSigninVerifier(Profile* profile);
  ~ForceSigninVerifier() override;

  // override OAuth2TokenService::Consumer
  void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

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

  OAuth2TokenService::Request* GetRequestForTesting();
  net::BackoffEntry* GetBackoffEntryForTesting();
  base::OneShotTimer* GetOneShotTimerForTesting();

 private:
  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;

  // Indicates whether the verification is finished successfully or with a
  // persistent error.
  bool has_token_verified_;
  net::BackoffEntry backoff_entry_;
  base::OneShotTimer backoff_request_timer_;
  base::TimeTicks creation_time_;

  OAuth2TokenService* oauth2_token_service_;
  SigninManager* signin_manager_;

  DISALLOW_COPY_AND_ASSIGN(ForceSigninVerifier);
};

#endif  // CHROME_BROWSER_SIGNIN_FORCE_SIGNIN_VERIFIER_H_
