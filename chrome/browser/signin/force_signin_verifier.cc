// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/signin/force_signin_verifier.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/scope_set.h"
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

signin::ConsentLevel GetProfileConsentLevelToVerify(Profile* profile) {
  // TODO(crbug.com/40280466): Condition to remove when we decide to
  // align requirements for Managed vs Consumer accounts.
  return enterprise_util::UserAcceptedAccountManagement(profile)
             ? signin::ConsentLevel::kSignin
             : signin::ConsentLevel::kSync;
}

}  // namespace

ForceSigninVerifier::ForceSigninVerifier(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    base::OnceCallback<void(bool)> on_token_fetch_complete)
    : has_token_verified_(false),
      backoff_entry_(&kForceSigninVerifierBackoffPolicy),
      creation_time_(base::TimeTicks::Now()),
      profile_(profile),
      identity_manager_(identity_manager),
      on_token_fetch_complete_(std::move(on_token_fetch_complete)) {
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  // Most of time (~94%), sign-in token can be verified with server.
  SendRequest();

  identity_manager_observer.Observe(identity_manager);
}

ForceSigninVerifier::~ForceSigninVerifier() {
  Cancel();
}

void ForceSigninVerifier::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    if (error.IsPersistentError()) {
      // Based on the obsolete UMA Signin.ForceSigninVerificationTime.Failure,
      // about 7% verifications are failed. Most of them are finished within
      // 113ms but some of them (<3%) could take longer than 3 minutes.
      has_token_verified_ = true;
      content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
          this);
      Cancel();
      std::move(on_token_fetch_complete_).Run(false);
      // Do nothing after this point, as `this` might be deleted.
    } else {
      backoff_entry_.InformOfRequest(false);
      backoff_request_timer_.Start(
          FROM_HERE, backoff_entry_.GetTimeUntilRelease(),
          base::BindOnce(&ForceSigninVerifier::SendRequest,
                         weak_factory_.GetWeakPtr()));
      access_token_fetcher_.reset();
    }
    return;
  }

  // Based on the obsolete UMA Signin.ForceSigninVerificationTime.Success, about
  // 93% verifications are succeeded. Most of them are finished ~1 second but
  // some of them (<3%) could take longer than 3 minutes.
  has_token_verified_ = true;
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  Cancel();
  std::move(on_token_fetch_complete_).Run(true);
  // Do nothing after this point, as `this` might be deleted.
}

void ForceSigninVerifier::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  // Try again immediately once the network is back and cancel any pending
  // request.
  backoff_entry_.Reset();
  if (backoff_request_timer_.IsRunning()) {
    backoff_request_timer_.Stop();
  }

  SendRequestIfNetworkAvailable(type);
}

void ForceSigninVerifier::Cancel() {
  backoff_entry_.Reset();
  backoff_request_timer_.Stop();
  access_token_fetcher_.reset();
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

void ForceSigninVerifier::SendRequest() {
  auto type = network::mojom::ConnectionType::CONNECTION_NONE;
  if (content::GetNetworkConnectionTracker()->GetConnectionType(
          &type,
          base::BindOnce(&ForceSigninVerifier::SendRequestIfNetworkAvailable,
                         weak_factory_.GetWeakPtr()))) {
    SendRequestIfNetworkAvailable(type);
  }
}

void ForceSigninVerifier::SendRequestIfNetworkAvailable(
    network::mojom::ConnectionType network_type) {
  if (!identity_manager_ || !identity_manager_->AreRefreshTokensLoaded()) {
    request_waiting_for_refresh_tokens_ = true;
    return;
  }

  if (network_type == network::mojom::ConnectionType::CONNECTION_NONE ||
      !ShouldSendRequest()) {
    return;
  }

  signin::ScopeSet oauth2_scopes;
  oauth2_scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "force_signin_verifier", identity_manager_, oauth2_scopes,
          base::BindOnce(&ForceSigninVerifier::OnAccessTokenFetchComplete,
                         weak_factory_.GetWeakPtr()),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          GetProfileConsentLevelToVerify(profile_));
}

bool ForceSigninVerifier::ShouldSendRequest() {
  return !has_token_verified_ && access_token_fetcher_.get() == nullptr &&
         identity_manager_ &&
         identity_manager_->HasPrimaryAccount(
             GetProfileConsentLevelToVerify(profile_));
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

void ForceSigninVerifier::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observer.Reset();

  identity_manager_ = nullptr;
}

void ForceSigninVerifier::OnRefreshTokensLoaded() {
  if (request_waiting_for_refresh_tokens_) {
    SendRequest();
    request_waiting_for_refresh_tokens_ = false;
  }
}

bool ForceSigninVerifier::GetRequestIsWaitingForRefreshTokensForTesting()
    const {
  return request_waiting_for_refresh_tokens_;
}
