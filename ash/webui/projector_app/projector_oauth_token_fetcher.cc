// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_oauth_token_fetcher.h"

#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "base/containers/contains.h"
#include "base/containers/flat_tree.h"
#include "base/time/time.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace {

// The cached OAuth token needs to be valid at least until base::Time::Now()
// + `kBufferTime`. The buffer time will be useful to ensure that we don't send
// soon to expire tokens to the Projector app.
const base::TimeDelta kBufferTime = base::Seconds(4);

signin::IdentityManager* GetIdentityManager() {
  return ash::ProjectorAppClient::Get()->GetIdentityManager();
}

OAuth2AccessTokenManager::ScopeSet GetScopeSet() {
  return OAuth2AccessTokenManager::ScopeSet{GaiaConstants::kDriveOAuth2Scope};
}

}  // namespace

namespace ash {

AccessTokenRequests::AccessTokenRequests() = default;

AccessTokenRequests::AccessTokenRequests(AccessTokenRequests&&) = default;

AccessTokenRequests& AccessTokenRequests::operator=(AccessTokenRequests&&) =
    default;

AccessTokenRequests::~AccessTokenRequests() = default;

ProjectorOAuthTokenFetcher::ProjectorOAuthTokenFetcher() = default;

ProjectorOAuthTokenFetcher::~ProjectorOAuthTokenFetcher() = default;

// static
std::vector<AccountInfo> ProjectorOAuthTokenFetcher::GetAccounts() {
  return GetIdentityManager()
      ->GetExtendedAccountInfoForAccountsWithRefreshToken();
}

// static
CoreAccountInfo ProjectorOAuthTokenFetcher::GetPrimaryAccountInfo() {
  return GetIdentityManager()->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
}

void ProjectorOAuthTokenFetcher::GetAccessTokenFor(
    const std::string& email,
    AccessTokenRequestCallback callback) {
  if (base::Contains(fetched_access_tokens_, email)) {
    const auto& access_token_info = fetched_access_tokens_[email];
    if (base::Time::Now() + kBufferTime < access_token_info.expiration_time) {
      std::move(callback).Run(
          email, GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          access_token_info);
      return;
    }

    // Else the stored value is expired. Let's remove its entry.
    fetched_access_tokens_.erase(email);
  }

  // If there is a pending fetch for the email, then append the callback to
  // the pending callbacks.
  if (base::Contains(pending_oauth_token_fetch_, email)) {
    pending_oauth_token_fetch_[email].callbacks.push_back(std::move(callback));
    return;
  }

  InitiateAccessTokenFetchFor(email, std::move(callback));
}

// Removed by token instead of email because the token value stored in
//`fetched_access_tokens_` might be updated to the valid value before this
// function get called.
void ProjectorOAuthTokenFetcher::InvalidateToken(const std::string& token) {
  base::EraseIf(fetched_access_tokens_, [&token](const auto& pair) -> bool {
    return pair.second.token == token;
  });
  GetIdentityManager()->RemoveAccessTokenFromCache(
      GetIdentityManager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GetScopeSet(), token);
}

bool ProjectorOAuthTokenFetcher::HasCachedTokenForTest(
    const std::string& email) {
  return base::Contains(fetched_access_tokens_, email);
}

bool ProjectorOAuthTokenFetcher::HasPendingRequestForTest(
    const std::string& email) {
  return base::Contains(pending_oauth_token_fetch_, email);
}

void ProjectorOAuthTokenFetcher::InitiateAccessTokenFetchFor(
    const std::string& email,
    AccessTokenRequestCallback callback) {
  DCHECK(!base::Contains(pending_oauth_token_fetch_, email));

  // There is no pending fetch for the email. Let's create a new fetch.
  // Let's start creating the oauth2 access token request.

  // kImmediate makes a one-shot immediate request.
  const auto mode = signin::AccessTokenFetcher::Mode::kImmediate;

  // Create the fetcher via |identity_manager|.
  auto* identity_manager = GetIdentityManager();
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher =
      identity_manager->CreateAccessTokenFetcherForAccount(
          identity_manager->FindExtendedAccountInfoByEmailAddress(email)
              .account_id,
          /*oauth_consumer_name=*/"ProjectorOAuthTokenFetcher", GetScopeSet(),
          base::BindOnce(
              &ProjectorOAuthTokenFetcher::OnAccessTokenRequestCompleted,
              // It is safe to use base::Unretained as |this| owns
              // |access_token_fetcher_|.
              base::Unretained(this), email),
          mode);
  AccessTokenRequests& entry = pending_oauth_token_fetch_[email];
  entry.access_token_fetcher = std::move(access_token_fetcher);
  entry.callbacks.push_back(std::move(callback));
}

void ProjectorOAuthTokenFetcher::OnAccessTokenRequestCompleted(
    const std::string& email,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo info) {
  if (!base::Contains(pending_oauth_token_fetch_, email))
    return;

  for (auto& callback : pending_oauth_token_fetch_[email].callbacks)
    std::move(callback).Run(email, error, info);

  if (error.state() == GoogleServiceAuthError::State::NONE)
    fetched_access_tokens_[email] = std::move(info);

  pending_oauth_token_fetch_.erase(email);
}

}  // namespace ash
