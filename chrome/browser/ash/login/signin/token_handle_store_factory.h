// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_FACTORY_H_

#include "ash/public/cpp/token_handle_store.h"
#include "base/no_destructor.h"

namespace ash {

// Helper class to switch implementation of TokenHandleStore depending
// on feature flag state.
// TokenHandleStoreFactory just switches the returned implementation by either
// creating a TokenHandleUtil or returning the global instance of
// TokenHandleStoreImpl.
// This class is temporary, and will be removed once we completely migrate to
// TokenHandleStoreImpl.
// TODO(b/387248794): Remove as part of cleanup.
class TokenHandleStoreFactory {
 public:
  TokenHandleStoreFactory(const TokenHandleStoreFactory&) = delete;
  TokenHandleStoreFactory& operator=(const TokenHandleStoreFactory&) = delete;

  static TokenHandleStoreFactory* Get();

  TokenHandleStore* GetTokenHandleStore();

 private:
  friend class base::NoDestructor<TokenHandleStoreFactory>;

  TokenHandleStoreFactory();
  ~TokenHandleStoreFactory();

  std::unique_ptr<TokenHandleStore> token_handle_store_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_FACTORY_H_
