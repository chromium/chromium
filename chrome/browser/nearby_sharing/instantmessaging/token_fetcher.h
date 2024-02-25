// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_TOKEN_FETCHER_H_
#define CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

// TokenFetcher is used for fetching oauth tokens for interacting with
// InstantMessaging servers to send and receive messages.
class TokenFetcher {
 public:
  explicit TokenFetcher(signin::IdentityManager* identity_manager);
  virtual ~TokenFetcher();

  // Fetches oauth access token for tachyon scope and passes the token in
  // |callback|.
  virtual void GetAccessToken(
      base::OnceCallback<void(const std::string& token)> callback);

 private:
  void OnOAuthTokenFetched(
      base::OnceCallback<void(const std::string& token)> callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  raw_ptr<signin::IdentityManager, LeakedDanglingUntriaged> identity_manager_;
  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;

  base::WeakPtrFactory<TokenFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_TOKEN_FETCHER_H_
