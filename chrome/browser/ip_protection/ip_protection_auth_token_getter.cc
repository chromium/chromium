// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_auth_token_getter.h"
#include <functional>
#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/message.h"

IpProtectionAuthTokenGetter::IpProtectionAuthTokenGetter(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  CHECK(identity_manager);
}

IpProtectionAuthTokenGetter::~IpProtectionAuthTokenGetter() = default;

void IpProtectionAuthTokenGetter::TryGetAuthTokens(
    uint32_t batch_size,
    TryGetAuthTokensCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (try_get_auth_token_callback_) {
    mojo::ReportBadMessage(
        "Concurrent calls to TryGetAuthTokens are not allowed");
    return;
  }
  // The `batch_size` is cast to an `int` for use by BlindSignAuth, so check for
  // overflow here.
  if (batch_size == 0 || batch_size > INT_MAX) {
    mojo::ReportBadMessage("Invalid batch_size");
    return;
  }
  try_get_auth_token_callback_ = std::move(callback);
  batch_size_ = batch_size;
  RequestOAuthToken();
}

void IpProtectionAuthTokenGetter::RequestOAuthToken() {
  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(try_get_auth_token_callback_).Run(absl::nullopt);
    return;
  }

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kIpProtectionAuthScope);

  // Waits for the account to have a refresh token before making the request.
  auto mode =
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable;

  // Create the OAuth token fetcher and call `OnRequestOAuthTokenCompleted()`
  // when complete. base::Unretained() is safe since `this` owns
  // `access_token_fetcher_`
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"IpProtectionService", identity_manager_, scopes,
          base::BindOnce(
              &IpProtectionAuthTokenGetter::OnRequestOAuthTokenCompleted,
              base::Unretained(this)),
          mode, signin::ConsentLevel::kSignin);
}

void IpProtectionAuthTokenGetter::OnRequestOAuthTokenCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  // If we fail to get an OAuth token don't attempt to fetch from Phosphor as
  // the request is guaranteed to fail.
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(try_get_auth_token_callback_).Run(absl::nullopt);
    return;
  }

  FetchBlindSignedToken(access_token_info);
}

void IpProtectionAuthTokenGetter::FetchBlindSignedToken(
    signin::AccessTokenInfo access_token_info) {
  DCHECK(bsa_);
  bsa_->GetTokens(
      access_token_info.token, batch_size_,
      [this](absl::StatusOr<absl::Span<quiche::BlindSignToken>> tokens) {
        OnFetchBlindSignedTokenCompleted(tokens);
      });
}

void IpProtectionAuthTokenGetter::OnFetchBlindSignedTokenCompleted(
    absl::StatusOr<absl::Span<quiche::BlindSignToken>> tokens) {
  if (!tokens.ok() || tokens.value().size() == 0) {
    std::move(try_get_auth_token_callback_).Run(absl::nullopt);
    return;
  }

  std::vector<network::mojom::BlindSignedAuthTokenPtr> result;
  std::transform(tokens->begin(), tokens->end(), std::back_inserter(result),
                 [](quiche::BlindSignToken bsa_token) {
                   base::Time expiration = base::Time::FromTimeT(
                       absl::ToTimeT(bsa_token.expiration));
                   return network::mojom::BlindSignedAuthToken::New(
                       bsa_token.token, expiration);
                 });

  std::move(try_get_auth_token_callback_).Run(std::move(result));
}

void IpProtectionAuthTokenGetter::Shutdown() {
  identity_manager_ = nullptr;
}

/*static*/
IpProtectionAuthTokenGetter* IpProtectionAuthTokenGetter::Get(
    Profile* profile) {
  return IpProtectionAuthTokenGetterFactory::GetForProfile(profile);
}
