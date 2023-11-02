// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_ACCESS_TOKEN_FETCHER_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_ACCESS_TOKEN_FETCHER_H_

#include <memory>

#include "base/memory/singleton.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

// Responsible for initialising the access token workflow. Executes the
// consuming callback when the fetch is done, and then becomes disposable.
class KidsAccessTokenFetcher {
 public:
  // For convenience, the interface of signin::PrimaryAccountAccessTokenFetcher
  // is wrapped into one value, so the decision how to handle errors is up to
  // consumers of access token fetcher.
  using Consumer = base::OnceCallback<void(
      base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>)>;
  // Non copyable.
  KidsAccessTokenFetcher() = delete;
  explicit KidsAccessTokenFetcher(signin::IdentityManager& identity_manager,
                                  Consumer consumer);
  KidsAccessTokenFetcher(const KidsAccessTokenFetcher&) = delete;
  KidsAccessTokenFetcher& operator=(const KidsAccessTokenFetcher&) = delete;
  ~KidsAccessTokenFetcher();

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);
  static const OAuth2AccessTokenManager::ScopeSet& Scopes();
  Consumer consumer_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      primary_account_access_token_fetcher_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_ACCESS_TOKEN_FETCHER_H_
