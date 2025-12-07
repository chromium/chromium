// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_SERVICE_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"

class Profile;
class AccountId;

namespace ash {

class TokenHandleStore;

class TokenHandleService : public KeyedService,
                           public signin::IdentityManager::Observer {
 public:
  explicit TokenHandleService(Profile* profile,
                              TokenHandleStore* token_handle_store);

  TokenHandleService(const TokenHandleService&) = delete;
  TokenHandleService& operator=(const TokenHandleService&) = delete;

  ~TokenHandleService() override;

  // Fetches access token for `account_id`, then potentially triggers a
  // token handle fetch.
  void MaybeFetchForExistingUser(const AccountId& account_id);

  // Analog of `MaybeFetchForExistingUser` for new users. This provides
  // a small optimization since we already have `access_token` and we can use
  // it directly.
  void MaybeFetchForNewUser(const AccountId& account_id,
                            const std::string& access_token,
                            const std::string& refresh_token_hash);

 private:
  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  void FetchAccessToken(const AccountId& account_id);
  void OnAccessTokenFetchComplete(const AccountId& account_id,
                                  GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  void GetRefreshTokenHash(const AccountId& account_id,
                           const std::string& access_token);
  void MaybeFetchTokenHandle(const AccountId account_id,
                             const std::string& access_token,
                             const std::string& refresh_token_hash);

  // KeyedService:
  void Shutdown() override;

  void StartObserving();

  raw_ptr<Profile> profile_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<TokenHandleStore> token_handle_store_;

  base::WeakPtrFactory<TokenHandleService> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_SERVICE_H_
