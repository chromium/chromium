// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_FACTORY_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_FACTORY_H_

#include <memory>

#include "ash/public/cpp/token_handle_store.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"

namespace ash {

// Helper class to switch implementation of TokenHandleStore depending
// on feature flag state.
// TokenHandleStoreFactory just switches the returned implementation by either
// creating a TokenHandleUtil or returning the global instance of
// TokenHandleStoreImpl.
// This class is temporary, and will be removed once we completely migrate to
// TokenHandleStoreImpl.
class TokenHandleStoreFactory {
 public:
  TokenHandleStoreFactory(const TokenHandleStoreFactory&) = delete;
  TokenHandleStoreFactory& operator=(const TokenHandleStoreFactory&) = delete;

  static TokenHandleStoreFactory* Get();

  TokenHandleStore* GetTokenHandleStore();

  void DestroyTokenHandleStore();

 private:
  // Functor that determines if a given `account_id` has a gaia password.
  // The class maintains the invariant that at any given time, there is at most
  // one request in flight for a given `account_id`.
  class DoesUserHaveGaiaPassword {
   public:
    using OnUserHasGaiaPasswordDetermined =
        base::OnceCallback<void(std::optional<bool>)>;
    using DoesUserHaveGaiaPasswordCallback = base::RepeatingCallback<void(
        const AccountId&,
        TokenHandleStoreFactory::DoesUserHaveGaiaPassword ::
            OnUserHasGaiaPasswordDetermined)>;

    explicit DoesUserHaveGaiaPassword(
        std::unique_ptr<AuthFactorEditor> factor_editor);
    ~DoesUserHaveGaiaPassword();
    DoesUserHaveGaiaPassword(const DoesUserHaveGaiaPassword&) = delete;
    DoesUserHaveGaiaPassword& operator=(const DoesUserHaveGaiaPassword&) =
        delete;

    void Run(const AccountId& account_id,
             OnUserHasGaiaPasswordDetermined callback);

    DoesUserHaveGaiaPasswordCallback CreateRepeatingCallback();

   private:
    // Callback passed to `AuthFactorEditor::GetAuthFactorConfiguration`.
    void OnGetAuthFactorConfiguration(std::unique_ptr<UserContext> user_context,
                                      std::optional<AuthenticationError> error);

    // Runs cleanup logic after replying to the request for `account_id`.
    void OnRepliedToRequest(const AccountId account_id);

    std::unique_ptr<AuthFactorEditor> factor_editor_;
    base::flat_map<AccountId, OnUserHasGaiaPasswordDetermined> callbacks_;
    base::WeakPtrFactory<DoesUserHaveGaiaPassword> weak_factory_{this};
  };

  friend class base::NoDestructor<TokenHandleStoreFactory>;

  std::unique_ptr<TokenHandleStore> CreateTokenHandleStoreImpl();

  TokenHandleStoreFactory();
  ~TokenHandleStoreFactory();

  DoesUserHaveGaiaPassword does_user_have_gaia_password_;
  std::unique_ptr<TokenHandleStore> token_handle_store_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_FACTORY_H_
