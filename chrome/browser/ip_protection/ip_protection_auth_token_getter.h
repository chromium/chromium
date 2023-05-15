// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_GETTER_H_
#define CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_GETTER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"

class IpProtectionAuthTokenGetter {
 public:
  using TryGetAuthTokenCallback =
      base::OnceCallback<void(const absl::optional<std::string>& ipp_header)>;

  // `identity_manager` must outlive `this`
  explicit IpProtectionAuthTokenGetter(
      signin::IdentityManager* identity_manager);

  ~IpProtectionAuthTokenGetter();

  void TryGetAuthToken(TryGetAuthTokenCallback callback);

 private:
  // Calls the IdentityManager asynchronously to request the OAuth token for the
  // logged in user.
  void RequestOAuthToken();

  // Gets the access token and caches the result.
  void OnRequestCompleted(GoogleServiceAuthError error,
                          signin::AccessTokenInfo access_token_info);

  // `FetchBlindSignedToken` calls into the quiche::BlindSignAuth library to
  // request an Auth token for use at the IP Protection proxies. Once retrieved,
  // the method will call the `on_token_recieved_callback_` to send the token
  // back to the network process.
  void FetchBlindSignedToken();

  signin::AccessTokenInfo access_token_;

  const raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  // Used to notify the network process that tokens have been fetched.
  TryGetAuthTokenCallback on_token_recieved_callback_;
};

#endif  // CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_AUTH_TOKEN_GETTER_H_
