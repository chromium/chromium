// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_IMPL_H_

#include <string>

#include "ash/public/cpp/token_handle_store.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/signin/token_handle_checker.h"
#include "chrome/browser/ash/login/signin/token_handle_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

// This class is responsible for orchestrating token handle checks and fetches.
// It is the sole writer to the token handle pref.
// TODO(387248794): Rename to `TokenHandleStore` as part of cleanup.
class TokenHandleStoreImpl : public TokenHandleStore {
 public:
  // Takes an account id, and a callback.
  // Responds on the callback with -
  // `true` if the given account id uses Gaia password for authentication.
  // `false` if the given account id does not use Gaia password for
  // authentication. `nullopt` if the given account id uses an unknown
  // authentication scheme.
  using DoesUserHaveGaiaPasswordCallback = base::RepeatingCallback<
      void(const AccountId&, base::OnceCallback<void(std::optional<bool>)>)>;

  TokenHandleStoreImpl(
      std::unique_ptr<user_manager::KnownUser> known_user,
      DoesUserHaveGaiaPasswordCallback does_user_have_gaia_password);
  ~TokenHandleStoreImpl() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

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
  void SetTokenHandleStale(const AccountId& account_id) override;
  void SetInvalidTokenForTesting(const char* token) override;
  void SetLastCheckedPrefForTesting(const AccountId& account_id,
                                    base::Time time) override;
  void MaybeFetchTokenHandle(
      PrefService* token_handle_mapping_store,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const AccountId& account_id,
      const std::string& access_token,
      const std::string& refresh_token_hash) override;

 private:
  void OnCheckToken(const AccountId& account_id,
                    const std::string& token,
                    const TokenHandleChecker::Status& status);

  void FetchTokenHandle(
      PrefService* token_handle_mapping_store,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const AccountId& account_id,
      const std::string& access_token,
      const std::string& refresh_token_hash);

  void OnFetchToken(PrefService* token_handle_mapping_store,
                    const std::string& refresh_token_hash,
                    const AccountId& account_id,
                    bool success,
                    const std::string& token);

  // Replies to all pending token handle checks for `account_id`.
  // `user_has_gaia_password` is used to determine whether reauth is required in
  // the case where the token handle has expired. If the user does have a gaia
  // password, an expired token will lead to a reauth.
  void ReplyToTokenHandleCheck(const std::string& token,
                               const AccountId& account_id,
                               const TokenHandleChecker::Status& status,
                               std::optional<bool> user_has_gaia_password);

  // We schedule the delete for `account_id`'s token handle checker and fetcher
  // to give a chance for the stack to unwind. Otherwise we might return to
  // invalid memory, causing a use-after-free.
  void ScheduleCheckerDelete(const AccountId& account_id);
  void ScheduleFetcherDelete(const AccountId& account_id);

  // Checks if token handle is explicitly marked as valid for `account_id`.
  bool HasTokenStatusInvalid(const AccountId& account_id) const;

  bool IsTokenHandleStale(const AccountId& account_id) const;

  void StoreTokenHandleMapping(PrefService* token_handle_mapping_store,
                               const std::string& token_handle,
                               const std::string& refresh_token_hash);

  void DiagnoseTokenHandleMapping(
      PrefService* token_handle_mapping_store,
      account_manager::AccountManager* account_manager,
      const AccountId& account_id,
      const std::string& token) const override;

  void OnGetTokenHash(PrefService* token_handle_mapping_store,
                      const std::string& token,
                      const std::string& account_manager_stored_hash) const;

  // Associates a request_id with a pending token handle fetch.
  base::flat_map<AccountId, std::unique_ptr<TokenHandleFetcher>>
      pending_fetches_;

  // Associates an `AccountId` with a pending token handle check.
  base::flat_map<AccountId, std::unique_ptr<TokenHandleChecker>>
      pending_checks_;

  // Stores a collection of callbacks to clients that initiated a token handle
  // check request that is currently pending. The callbacks are grouped by
  // `AccountId` as concurrent requests are pooled and replied to when the most
  // recent request for a token handle check for `AccountId` returns.
  base::flat_map<AccountId, std::vector<TokenValidationCallback>>
      pending_callbacks_;

  std::unique_ptr<user_manager::KnownUser> known_user_;

  DoesUserHaveGaiaPasswordCallback does_user_have_gaia_password_;

  std::optional<std::string> invalid_token_for_testing_;

  base::WeakPtrFactory<TokenHandleStoreImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_IMPL_H_
