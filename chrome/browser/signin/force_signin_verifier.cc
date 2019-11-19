// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/signin/force_signin_verifier.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
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

ForceSigninVerifier::ForceSigninVerifier(
    signin::IdentityManager* identity_manager)
    : has_token_verified_(false),
      backoff_entry_(&kForceSigninVerifierBackoffPolicy),
      creation_time_(base::TimeTicks::Now()),
      identity_manager_(identity_manager) {
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  UMA_HISTOGRAM_BOOLEAN(kForceSigninVerificationMetricsName,
                        ShouldSendRequest());
  SendRequest();
}

ForceSigninVerifier::~ForceSigninVerifier() {
  Cancel();
}

void ForceSigninVerifier::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
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
          base::BindOnce(&ForceSigninVerifier::SendRequest,
                         base::Unretained(this)));
      access_token_fetcher_.reset();
    }
    return;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES(kForceSigninVerificationSuccessTimeMetricsName,
                             base::TimeTicks::Now() - creation_time_);
  has_token_verified_ = true;
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  Cancel();
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
  access_token_fetcher_.reset();
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

  identity::ScopeSet oauth2_scopes;
  oauth2_scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  // It is safe to use Unretained(this) here given that the callback
  // will not be invoked if this object is deleted.
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "force_signin_verifier", identity_manager_, oauth2_scopes,
          base::BindOnce(&ForceSigninVerifier::OnAccessTokenFetchComplete,
                         base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

bool ForceSigninVerifier::ShouldSendRequest() {
  return !has_token_verified_ && access_token_fetcher_.get() == nullptr &&
         identity_manager_->HasPrimaryAccount();
}

void ForceSigninVerifier::CloseAllBrowserWindows() {
  // Do not close window if there is ongoing reauth. If it fails later, the
  // signin process should take care of the signout.
  auto* primary_account_mutator = identity_manager_->GetPrimaryAccountMutator();
  if (!primary_account_mutator)
    return;
  primary_account_mutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
      signin_metrics::AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
}

signin::PrimaryAccountAccessTokenFetcher*
ForceSigninVerifier::GetAccessTokenFetcherForTesting() {
  return access_token_fetcher_.get();
}

net::BackoffEntry* ForceSigninVerifier::GetBackoffEntryForTesting() {
  return &backoff_entry_;
}

base::OneShotTimer* ForceSigninVerifier::GetOneShotTimerForTesting() {
  return &backoff_request_timer_;
}
