// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"

#include "google_apis/gaia/gaia_constants.h"

namespace {
// The oauth token consumer name.
const char kOAuthConsumerName[] = "nearby_sharing";
}  // namespace

TokenFetcher::TokenFetcher(signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {}

TokenFetcher::~TokenFetcher() = default;

void TokenFetcher::GetAccessToken(
    base::OnceCallback<void(const std::string& token)> callback) {
  // Using Mode::Immediate since Nearby Share is only available for signed-in
  // users.

  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      identity_manager_->GetPrimaryAccountId(
          signin::ConsentLevel::kNotRequired),
      kOAuthConsumerName, {GaiaConstants::kTachyonOAuthScope},
      base::BindOnce(&TokenFetcher::OnOAuthTokenFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void TokenFetcher::OnOAuthTokenFetched(
    base::OnceCallback<void(const std::string& token)> callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // Note: We do not do anything special for empty tokens.
  std::move(callback).Run(access_token_info.token);
  token_fetcher_.reset();
}
