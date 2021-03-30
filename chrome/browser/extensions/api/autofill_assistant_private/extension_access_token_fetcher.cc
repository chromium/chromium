// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_assistant_private/extension_access_token_fetcher.h"

#include "base/callback.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"

namespace extensions {

ExtensionAccessTokenFetcher::ExtensionAccessTokenFetcher(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {}

ExtensionAccessTokenFetcher::~ExtensionAccessTokenFetcher() = default;

void ExtensionAccessTokenFetcher::FetchAccessToken(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  // TODO(b/143736397): Use a more flexible logic to pick this account.
  auto account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);

  callback_ = std::move(callback);
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleUserInfoProfile);
  auto mode = signin::AccessTokenFetcher::Mode::kImmediate;
  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_info.account_id,
      /*consumer_name=*/"AccessTokenFetcher", scopes,
      base::BindOnce(&ExtensionAccessTokenFetcher::OnCompleted,
                     // It is safe to use base::Unretained as
                     // |this| owns |access_token_fetcher_|.
                     base::Unretained(this)),
      mode);
}

void ExtensionAccessTokenFetcher::InvalidateAccessToken(
    const std::string& access_token) {
  // TODO(b/143736397) Implement this by providing the data required for
  // RemoveAccessTokenFromCache?
}

void ExtensionAccessTokenFetcher::OnCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (!callback_)
    return;

  if (error.state() == GoogleServiceAuthError::NONE) {
    std::move(callback_).Run(true, access_token_info.token);
  } else {
    VLOG(2) << "Access token fetching failed with error state " << error.state()
            << " and message " << error.ToString();
    std::move(callback_).Run(false, "");
  }
}

}  // namespace extensions
