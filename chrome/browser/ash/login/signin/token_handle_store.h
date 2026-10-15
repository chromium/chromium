// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/signin/token_handle_checker.h"
#include "chrome/browser/ash/login/signin/token_handle_fetcher.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace account_manager {
class AccountManager;
}  // namespace account_manager

class PrefRegistrySimple;
class PrefService;

namespace ash {

// This class is responsible for orchestrating token handle checks and fetches.
// It is the sole writer to the token handle pref.
// Token Handle is an access token associated with OAuth refresh token that has
// exactly the same lifetime. It is not secure, and its only purpose is checking
// validity of corresponding refresh token in an insecure environment, eg. login
// screen.
class TokenHandleStore {
 public:
  // `account_id`: The account for which the token handle check was performed.
  // `token`: The token which was checked. Empty if we could not find a token
  // handle for `account_id`.
  // `reauth_required`: Result of `IsReauthRequired()`. `true` means that
  // reauthentication is required for `account_id`.
  using TokenValidationCallback =
      base::OnceCallback<void(const AccountId& account_id,
                              const std::string& token,
                              bool reauth_required)>;

  // Takes an account id, and a callback.
  // Responds on the callback with -
  // `true` if the given account id uses Gaia password for authentication.
  // `false` if the given account id does not use Gaia password for
  // authentication. `nullopt` if the given account id uses an unknown
  // authentication scheme.
  using DoesUserHaveGaiaPasswordCallback = base::RepeatingCallback<
      void(const AccountId&, base::OnceCallback<void(std::optional<bool>)>)>;

  TokenHandleStore(
      std::unique_ptr<user_manager::KnownUser> known_user,
      DoesUserHaveGaiaPasswordCallback does_user_have_gaia_password);
  ~TokenHandleStore();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  TokenHandleStore(const TokenHandleStore&) = delete;
  TokenHandleStore& operator=(const TokenHandleStore&) = delete;

  // Returns true if UserManager has token handle associated with `account_id`.
  bool HasToken(const AccountId& account_id) const;

  // Returns true if the token status for `account_id` was checked recently.
  bool IsRecentlyChecked(const AccountId& account_id) const;

  // Indicates if token handle for `account_id` is missing, marked as invalid,
  // or stale.
  bool ShouldObtainHandle(const AccountId& account_id) const;

  // Performs token handle check for `account_id`. Will call `callback` with
  // corresponding result. See `TokenValidationCallback` for details.
  void IsReauthRequired(
      const AccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TokenValidationCallback callback);

  // Given the token `handle` store it for `account_id`.
  void StoreTokenHandle(const AccountId& account_id, const std::string& handle);

  // Sets the token handle to stale.
  void SetTokenHandleStale(const AccountId& account_id);

  // Depending on the state of the current token handle, execute a token handle
  // fetch and store it.
  void MaybeFetchTokenHandle(
      PrefService* token_handle_mapping_store,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const AccountId& account_id,
      const std::string& access_token,
      const std::string& refresh_token_hash);

  // Records whether or not the current refresh-token-hash stored in account
  // manager matches the refresh-token-hash for `token`. This helps with
  // diagnosing token handle checks performed on older token handles.
  void DiagnoseTokenHandleMapping(
      PrefService* token_handle_mapping_store,
      account_manager::AccountManager* account_manager,
      const AccountId& account_id,
      const std::string& token) const;

  // Testing methods:
  void SetInvalidTokenForTesting(const char* token);

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

  // We schedule the delete for `account_id`'s token handle fetcher to give a
  // chance for the stack to unwind. Otherwise we might return to invalid
  // memory, causing a use-after-free.
  void ScheduleFetcherDelete(const AccountId& account_id);

  // Checks if token handle is explicitly marked as valid for `account_id`.
  bool HasTokenStatusInvalid(const AccountId& account_id) const;

  bool IsTokenHandleStale(const AccountId& account_id) const;

  void StoreTokenHandleMapping(PrefService* token_handle_mapping_store,
                               const std::string& token_handle,
                               const std::string& refresh_token_hash);

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

  base::WeakPtrFactory<TokenHandleStore> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_STORE_H_
