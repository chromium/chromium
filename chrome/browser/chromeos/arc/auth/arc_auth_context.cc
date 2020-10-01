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
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace arc {

namespace {

constexpr base::TimeDelta kRefreshTokenTimeout =
    base::TimeDelta::FromSeconds(10);

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

void ArcAuthContext::Prepare(const PrepareCallback& callback) {
  if (context_prepared_) {
    callback.Run(true);
    return;
  }

  callback_ = callback;
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
