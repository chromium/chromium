// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/signin/force_signin_verifier.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {
const net::BackoffEntry::Policy kForceSigninVerifierBackoffPolicy = {
    0,              // Number of initial errors to ignore before applying
                    // exponential back-off rules.
    2000,           // Initial delay in ms.
    2,              // Factor by which the waiting time will be multiplied.
    0.2,            // Fuzzing percentage.
    4 * 60 * 1000,  // Maximum amount of time to delay th request in ms.
    -1,             // Never discard the entry.
    false           // Do not always use initial delay.
};

}  // namespace

const char kForceSigninVerificationMetricsName[] =
    "Signin.ForceSigninVerificationRequest";
const char kForceSigninVerificationSuccessTimeMetricsName[] =
    "Signin.ForceSigninVerificationTime.Success";
const char kForceSigninVerificationFailureTimeMetricsName[] =
    "Signin.ForceSigninVerificationTime.Failure";

ForceSigninVerifier::ForceSigninVerifier(Profile* profile)
    : OAuth2TokenService::Consumer("force_signin_verifier"),
      has_token_verified_(false),
      backoff_entry_(&kForceSigninVerifierBackoffPolicy),
      creation_time_(base::TimeTicks::Now()),
      oauth2_token_service_(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile)),
      signin_manager_(SigninManagerFactory::GetForProfile(profile)) {
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  UMA_HISTOGRAM_BOOLEAN(kForceSigninVerificationMetricsName,
                        ShouldSendRequest());
  SendRequest();
}

ForceSigninVerifier::~ForceSigninVerifier() {
  Cancel();
}

void ForceSigninVerifier::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kForceSigninVerificationSuccessTimeMetricsName,
                             base::TimeTicks::Now() - creation_time_);
  has_token_verified_ = true;
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  Cancel();
}

void ForceSigninVerifier::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  if (error.IsPersistentError()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kForceSigninVerificationFailureTimeMetricsName,
                               base::TimeTicks::Now() - creation_time_);
    has_token_verified_ = true;
    CloseAllBrowserWindows();
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
    Cancel();
  } else {
    backoff_entry_.InformOfRequest(false);
    backoff_request_timer_.Start(
        FROM_HERE, backoff_entry_.GetTimeUntilRelease(),
        base::Bind(&ForceSigninVerifier::SendRequest, base::Unretained(this)));
    access_token_request_.reset();
  }
}

void ForceSigninVerifier::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  // Try again immediately once the network is back and cancel any pending
  // request.
  backoff_entry_.Reset();
  if (backoff_request_timer_.IsRunning())
    backoff_request_timer_.Stop();

  SendRequestIfNetworkAvailable(type);
}

void ForceSigninVerifier::Cancel() {
  backoff_entry_.Reset();
  backoff_request_timer_.Stop();
  access_token_request_.reset();
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

bool ForceSigninVerifier::HasTokenBeenVerified() {
  return has_token_verified_;
}

void ForceSigninVerifier::SendRequest() {
  auto type = network::mojom::ConnectionType::CONNECTION_NONE;
  if (content::GetNetworkConnectionTracker()->GetConnectionType(
          &type,
          base::BindOnce(&ForceSigninVerifier::SendRequestIfNetworkAvailable,
                         base::Unretained(this)))) {
    SendRequestIfNetworkAvailable(type);
  }
}

void ForceSigninVerifier::SendRequestIfNetworkAvailable(
    network::mojom::ConnectionType network_type) {
  if (network_type == network::mojom::ConnectionType::CONNECTION_NONE ||
      !ShouldSendRequest()) {
    return;
  }

  std::string account_id = signin_manager_->GetAuthenticatedAccountId();
  OAuth2TokenService::ScopeSet oauth2_scopes;
  oauth2_scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  access_token_request_ =
      oauth2_token_service_->StartRequest(account_id, oauth2_scopes, this);
}

bool ForceSigninVerifier::ShouldSendRequest() {
  return !has_token_verified_ && access_token_request_.get() == nullptr &&
         signin_manager_->IsAuthenticated();
}

void ForceSigninVerifier::CloseAllBrowserWindows() {
  // Do not close window if there is ongoing reauth. If it fails later, the
  // signin process should take care of the signout.
  if (signin_manager_->AuthInProgress())
    return;
  signin_manager_->SignOutAndRemoveAllAccounts(
      signin_metrics::AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
}

OAuth2TokenService::Request* ForceSigninVerifier::GetRequestForTesting() {
  return access_token_request_.get();
}

net::BackoffEntry* ForceSigninVerifier::GetBackoffEntryForTesting() {
  return &backoff_entry_;
}

base::OneShotTimer* ForceSigninVerifier::GetOneShotTimerForTesting() {
  return &backoff_request_timer_;
}
