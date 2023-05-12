// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_auth_token_getter.h"
#include <functional>
#include <optional>

#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"

IpProtectionAuthTokenGetter::IpProtectionAuthTokenGetter(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  CHECK(identity_manager);
}

IpProtectionAuthTokenGetter::~IpProtectionAuthTokenGetter() = default;

void IpProtectionAuthTokenGetter::TryGetAuthToken(
    TryGetAuthTokenCallback callback) {
  on_token_recieved_callback_ = std::move(callback);
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(on_token_recieved_callback_).Run(absl::nullopt);
    return;
  }
  RequestOAuthToken();
}

void IpProtectionAuthTokenGetter::RequestOAuthToken() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kIpProtectionAuthScope);

  // Waits for the account to have a refresh token before making the request.
  auto mode =
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable;

  // Create the OAuth token fetcher and call OnRequestCompleted when
  // complete.
  // base::Unretained() is safe since `this` owns `access_token_fetcher_`
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"IpProtectionService", identity_manager_, scopes,
          base::BindOnce(&IpProtectionAuthTokenGetter::OnRequestCompleted,
                         base::Unretained(this)),
          mode, signin::ConsentLevel::kSignin);
}

void IpProtectionAuthTokenGetter::OnRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  // If we fail to get an OAuth token don't attempt to fetch from Phosphor as
  // the request is guaranteed to fail.
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(on_token_recieved_callback_).Run(absl::nullopt);
    return;
  }

  access_token_ = access_token_info;
}
