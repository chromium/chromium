// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/client/oauth_token_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/boca/babelorca/client/token_data_wrapper.h"
#include "chrome/browser/ash/boca/babelorca/client/token_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace babelorca {

OAuthTokenFetcher::OAuthTokenFetcher(signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {}

OAuthTokenFetcher::~OAuthTokenFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// TokenFetcher:
void OAuthTokenFetcher::fetchToken(TokenFetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (access_token_fetcher_) {
    LOG(ERROR) << "Tachyon oauth token fetch is already in progress.";
    std::move(callback).Run(std::nullopt);
    return;
  }
  CHECK(identity_manager_);
  static constexpr char kOauthConsumerName[] = "babel_orca";
  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      kOauthConsumerName, {GaiaConstants::kTachyonOAuthScope},
      base::BindOnce(
          &OAuthTokenFetcher::OnOAuthTokenRequestCompleted,
          // base::Unretained is safe, `this` owns `access_token_fetcher_`.
          base::Unretained(this), std::move(callback)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void OAuthTokenFetcher::OnOAuthTokenRequestCompleted(
    TokenFetchCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE) {
    std::move(callback).Run(TokenDataWrapper(
        std::move(access_token_info.token), access_token_info.expiration_time));
    return;
  }
  std::move(callback).Run(std::nullopt);
}

}  // namespace babelorca
