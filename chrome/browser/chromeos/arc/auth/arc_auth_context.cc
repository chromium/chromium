// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/auth/arc_auth_context.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/ubertoken_fetcher.h"
#include "content/public/common/url_constants.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace arc {

namespace {

constexpr int kMaxRetryAttempts = 3;

constexpr base::TimeDelta kRefreshTokenTimeout =
    base::TimeDelta::FromSeconds(10);

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    5000,

    // Factor by which the waiting time will be multiplied.
    2.0,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.0,  // 0%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 15,  // 15 seconds.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

}  // namespace

ArcAuthContext::ArcAuthContext(Profile* profile, const std::string& account_id)
    : profile_(profile),
      account_id_(account_id),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      retry_backoff_(&kRetryBackoffPolicy) {
  DCHECK(identity_manager_->HasAccountWithRefreshToken(account_id));
}

ArcAuthContext::~ArcAuthContext() {
  identity_manager_->RemoveObserver(this);
}

void ArcAuthContext::Prepare(const PrepareCallback& callback) {
  if (context_prepared_) {
    callback.Run(true);
    return;
  }

  callback_ = callback;
  identity_manager_->RemoveObserver(this);
  refresh_token_timeout_.Stop();
  ResetFetchers();
  retry_backoff_.Reset();

  if (!identity_manager_->HasAccountWithRefreshToken(account_id_)) {
    identity_manager_->AddObserver(this);
    VLOG(1) << "Waiting for refresh token for account " << account_id_;
    refresh_token_timeout_.Start(FROM_HERE, kRefreshTokenTimeout, this,
                                 &ArcAuthContext::OnRefreshTokenTimeout);
    return;
  }

  StartFetchers();
}

std::unique_ptr<signin::AccessTokenFetcher>
ArcAuthContext::CreateAccessTokenFetcher(
    const std::string& consumer_name,
    const identity::ScopeSet& scopes,
    signin::AccessTokenFetcher::TokenCallback callback) {
  DCHECK(identity_manager_->HasAccountWithRefreshToken(account_id_));
  return identity_manager_->CreateAccessTokenFetcherForAccount(
      account_id_, consumer_name, scopes, std::move(callback),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void ArcAuthContext::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // There is no need to check |is_valid| here. It is intended to avoid
  // adding the ability to query the persistent error state to the
  // IdentityManager API, which is irrelevant for this case.
  if (account_info.account_id != account_id_)
    return;
  OnRefreshTokensLoaded();
}

void ArcAuthContext::OnRefreshTokensLoaded() {
  identity_manager_->RemoveObserver(this);
  VLOG(1) << "Refresh token for account " << account_id_ << " loaded.";
  refresh_token_timeout_.Stop();
  StartFetchers();
}

void ArcAuthContext::OnRefreshTokenTimeout() {
  LOG(WARNING) << "Failed to wait for refresh token.";
  identity_manager_->RemoveObserver(this);
  std::move(callback_).Run(false);
}

void ArcAuthContext::StartFetchers() {
  DCHECK(!refresh_token_timeout_.IsRunning());
  ResetFetchers();

  if (skip_merge_session_for_testing_) {
    OnMergeSessionSuccess("");
    return;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  ubertoken_fetcher_ = identity_manager->CreateUbertokenFetcherForAccount(
      account_id_,
      base::BindOnce(&ArcAuthContext::OnUbertokenFetchComplete,
                     base::Unretained(this)),
      gaia::GaiaSource::kChromeOS, profile_->GetURLLoaderFactory());
}

void ArcAuthContext::ResetFetchers() {
  merger_fetcher_.reset();
  ubertoken_fetcher_.reset();
  retry_timeout_.Stop();
}

void ArcAuthContext::OnFetcherError(const GoogleServiceAuthError& error) {
  ResetFetchers();
  DCHECK(error.state() != GoogleServiceAuthError::NONE);
  if (error.IsTransientError()) {
    retry_backoff_.InformOfRequest(false);
    if (retry_backoff_.failure_count() <= kMaxRetryAttempts) {
      LOG(WARNING) << "Found transient error. Retry attempt "
                   << retry_backoff_.failure_count() << ".";
      refresh_token_timeout_.Start(FROM_HERE,
                                   retry_backoff_.GetTimeUntilRelease(), this,
                                   &ArcAuthContext::StartFetchers);
      return;
    }
    LOG(WARNING) << "Too many transient errors. Stop retrying.";
  }
  std::move(callback_).Run(false);
}

void ArcAuthContext::OnUbertokenFetchComplete(GoogleServiceAuthError error,
                                              const std::string& token) {
  if (error != GoogleServiceAuthError::AuthErrorNone()) {
    LOG(WARNING) << "Failed to get ubertoken " << error.ToString() << ".";
    OnFetcherError(error);
    return;
  }

  ResetFetchers();
  merger_fetcher_.reset(new GaiaAuthFetcher(this, gaia::GaiaSource::kChromeOS,
                                            profile_->GetURLLoaderFactory()));
  merger_fetcher_->StartMergeSession(token, std::string());
}

void ArcAuthContext::OnMergeSessionSuccess(const std::string& data) {
  VLOG_IF(1, retry_backoff_.failure_count())
      << "Auth context was successfully prepared after retry.";
  context_prepared_ = true;
  ResetFetchers();
  std::move(callback_).Run(true);
}

void ArcAuthContext::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "Failed to merge gaia session " << error.ToString() << ".";
  OnFetcherError(error);
}

}  // namespace arc
