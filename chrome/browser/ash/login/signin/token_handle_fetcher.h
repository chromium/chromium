// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace ash {

// This class is responsible for obtaining new token handles for a user.
class TokenHandleFetcher : public gaia::GaiaOAuthClient::Delegate {
 public:
  TokenHandleFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const AccountId& account_id);

  TokenHandleFetcher(const TokenHandleFetcher&) = delete;
  TokenHandleFetcher& operator=(const TokenHandleFetcher&) = delete;

  ~TokenHandleFetcher() override;

  using TokenFetchCallback = base::OnceCallback<
      void(const AccountId&, bool success, const std::string& token)>;

  // Fetch token handle for a user.
  void Fetch(const std::string& access_token,
             const std::string& refresh_token_hash,
             TokenFetchCallback callback);

 private:
  // GaiaOAuthClient::Delegate overrides:
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;
  void OnGetTokenInfoResponse(const base::Value::Dict& token_info) override;

  void SendCallbackResponse(bool success, const std::string& token);

  AccountId account_id_;
  base::TimeTicks tokeninfo_response_start_time_;
  std::string refresh_token_hash_;
  gaia::GaiaOAuthClient gaia_client_;
  TokenFetchCallback callback_;

  base::WeakPtrFactory<TokenHandleFetcher> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_
