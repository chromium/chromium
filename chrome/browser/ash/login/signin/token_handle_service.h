// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_SERVICE_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;
class AccountId;

namespace ash {

class TokenHandleService : public KeyedService,
                           public signin::IdentityManager::Observer {
 public:
  explicit TokenHandleService(Profile* profile);

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
                            const std::string& access_token);

 private:
  // signin::IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;

  // KeyedService:
  void Shutdown() override;

  void StartObserving();

  raw_ptr<Profile> profile_;
  raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_SERVICE_H_
