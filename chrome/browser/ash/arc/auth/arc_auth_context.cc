// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/auth/arc_auth_context.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

constexpr base::TimeDelta kRefreshTokenTimeout = base::Seconds(10);

}  // namespace

ArcAuthContext::ArcAuthContext(Profile* profile,
                               const CoreAccountId& account_id)
    : account_id_(account_id),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)) {
  DCHECK(identity_manager_->HasAccountWithRefreshToken(account_id));
}

ArcAuthContext::~ArcAuthContext() {
  identity_manager_->RemoveObserver(this);
}

void ArcAuthContext::Prepare(PrepareCallback callback) {
  if (context_prepared_) {
    std::move(callback).Run(true);
    return;
  }

  callback_ = std::move(callback);
  identity_manager_->RemoveObserver(this);
  refresh_token_timeout_.Stop();

  if (!identity_manager_->HasAccountWithRefreshToken(account_id_)) {
    identity_manager_->AddObserver(this);
    VLOG(1) << "Waiting for refresh token for account " << account_id_;
    refresh_token_timeout_.Start(FROM_HERE, kRefreshTokenTimeout, this,
                                 &ArcAuthContext::OnRefreshTokenTimeout);
    return;
  }

  // Refresh token is already available.
  std::move(callback_).Run(true);
}

std::unique_ptr<signin::AccessTokenFetcher>
ArcAuthContext::CreateAccessTokenFetcher(
    const std::string& consumer_name,
    const signin::ScopeSet& scopes,
    signin::AccessTokenFetcher::TokenCallback callback) {
  DCHECK(identity_manager_->HasAccountWithRefreshToken(account_id_));
  return identity_manager_->CreateAccessTokenFetcherForAccount(
      account_id_, consumer_name, scopes, std::move(callback),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void ArcAuthContext::RemoveAccessTokenFromCache(
    const signin::ScopeSet& scopes,
    const std::string& access_token) {
  identity_manager_->RemoveAccessTokenFromCache(account_id_, scopes,
                                                access_token);
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
  std::move(callback_).Run(true);
}

void ArcAuthContext::OnRefreshTokenTimeout() {
  LOG(WARNING) << "Failed to wait for refresh token.";
  identity_manager_->RemoveObserver(this);
  std::move(callback_).Run(false);
}

}  // namespace arc
