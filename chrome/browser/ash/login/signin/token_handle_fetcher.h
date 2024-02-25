// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_oauth_client.h"

class Profile;
class PrefRegistrySimple;

namespace signin {
class IdentityManager;
}

namespace ash {
class TokenHandleUtil;

// This class is responsible for obtaining new token handle for user.
// It can be used in two ways. When a user has just used Gaia signin there is
// an OAuth2 token available. If there is profile already loaded, then
// minting additional access token might be required.
class TokenHandleFetcher : public gaia::GaiaOAuthClient::Delegate {
 public:
  TokenHandleFetcher(Profile* profile,
                     TokenHandleUtil* util,
                     const AccountId& account_id);

  TokenHandleFetcher(const TokenHandleFetcher&) = delete;
  TokenHandleFetcher& operator=(const TokenHandleFetcher&) = delete;

  ~TokenHandleFetcher() override;

  using TokenFetchingCallback =
      base::OnceCallback<void(const AccountId&, bool success)>;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Fetch token handle for a user who has just signed in via Gaia online auth.
  void FillForNewUser(const std::string& access_token,
                      const std::string& refresh_token_hash,
                      TokenFetchingCallback callback);

  // Fetch token handle for an existing user.
  void BackfillToken(TokenFetchingCallback callback);

  void DiagnoseTokenHandleMapping(const AccountId& account_id,
                                  const std::string& token);
  void OnGetTokenHash(const std::string& token,
                      const std::string& account_manager_stored_hash);

  static void EnsureFactoryBuilt();

 private:
  // AccessTokenFetcher::TokenCallback for PrimaryAccountAccessTokenFetcher.
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  // GaiaOAuthClient::Delegate overrides:
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;
  void OnGetTokenInfoResponse(const base::Value::Dict& token_info) override;

  void FillForAccessToken(const std::string& access_token,
                          const std::string& refresh_token_hash);
  void StoreTokenHandleMapping(const std::string& token_handle);

  // This is called before profile is detroyed.
  void OnProfileDestroyed();

  const raw_ptr<Profile> profile_;
  const raw_ptr<TokenHandleUtil> token_handle_util_;
  AccountId account_id_;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  base::TimeTicks tokeninfo_response_start_time_ = base::TimeTicks();
  std::string refresh_token_hash_;
  TokenFetchingCallback callback_;
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_client_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  base::CallbackListSubscription profile_shutdown_subscription_;

  base::WeakPtrFactory<TokenHandleFetcher> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_FETCHER_H_
