// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_IMPL_H_

#include <string>

#include "ash/public/cpp/token_handle_store.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/signin/token_handle_checker.h"
#include "components/account_id/account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

// This class is responsible for orchestrating token handle checks and fetches.
// It is the sole writer to the token handle pref.
// TODO(387248794): Rename to `TokenHandleStore` as part of cleanup.
class TokenHandleStoreImpl : public TokenHandleStore {
 public:
  static TokenHandleStoreImpl* Get();

  TokenHandleStoreImpl(const TokenHandleStoreImpl&) = delete;
  TokenHandleStoreImpl& operator=(const TokenHandleStoreImpl&) = delete;

  // TokenHandleStore:
  bool HasToken(const AccountId& account_id) const override;
  bool IsRecentlyChecked(const AccountId& account_id) const override;
  bool ShouldObtainHandle(const AccountId& account_id) const override;
  void IsReauthRequired(
      const AccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TokenValidationCallback callback) override;
  void StoreTokenHandle(const AccountId& account_id,
                        const std::string& handle) override;
  void SetInvalidTokenForTesting(const char* token) override;
  void SetLastCheckedPrefForTesting(const AccountId& account_id,
                                    base::Time time) override;

  void MaybeFetchTokenHandle(const AccountId& account_id);

 private:
  friend base::NoDestructor<TokenHandleStoreImpl>;

  TokenHandleStoreImpl();
  ~TokenHandleStoreImpl() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_IMPL_H_
