// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_access_token_fetcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace {

using ::base::BindOnce;
using ::base::expected;
using ::base::NoDestructor;
using ::base::unexpected;
using ::base::Unretained;
using ::signin::AccessTokenFetcher;
using ::signin::AccessTokenInfo;
using ::signin::ConsentLevel;
using ::signin::IdentityManager;
using ::signin::PrimaryAccountAccessTokenFetcher;

expected<AccessTokenInfo, GoogleServiceAuthError> ToSingleReturnValue(
    GoogleServiceAuthError error,
    AccessTokenInfo access_token_info) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    return access_token_info;
  }
  return unexpected(error);
}

}  // namespace

KidsAccessTokenFetcher::KidsAccessTokenFetcher(
    IdentityManager& identity_manager,
    Consumer consumer)
    : consumer_(std::move(consumer)) {
  // base::Unretained(.) is safe, because no extra on-destroyed semantics are
  // needed and this instance must outlive the callback execution.
  primary_account_access_token_fetcher_ =
      std::make_unique<PrimaryAccountAccessTokenFetcher>(
          "family_info_fetcher", &identity_manager, Scopes(),
          BindOnce(&KidsAccessTokenFetcher::OnAccessTokenFetchComplete,
                   Unretained(this)),
          PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          ConsentLevel::kSignin);
}
KidsAccessTokenFetcher::~KidsAccessTokenFetcher() = default;

void KidsAccessTokenFetcher::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  std::move(consumer_).Run(ToSingleReturnValue(error, access_token_info));
}

const OAuth2AccessTokenManager::ScopeSet& KidsAccessTokenFetcher::Scopes() {
  static auto nonce = NoDestructor<OAuth2AccessTokenManager::ScopeSet>(
      {GaiaConstants::kKidFamilyReadonlyOAuth2Scope});
  return *nonce;
}
