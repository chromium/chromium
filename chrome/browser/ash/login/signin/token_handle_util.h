// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"

class AccountId;

namespace ash {

// This class is responsible for operations with External Token Handle.
// Handle is an extra token associated with OAuth refresh token that have
// exactly same lifetime. It is not secure, and it's only purpose is checking
// validity of corresponding refresh token in the insecure environment.
class TokenHandleUtil {
 public:
  TokenHandleUtil();

  TokenHandleUtil(const TokenHandleUtil&) = delete;
  TokenHandleUtil& operator=(const TokenHandleUtil&) = delete;

  ~TokenHandleUtil();

  // Status of the token handle.
  enum class Status {
    kValid,    // The token is valid and reauthentication is not required.
    kInvalid,  // The token is invalid and reauthentication is required.
    kExpired,  // The token is valid, but `expires_in` value is negative. This
               // can happen if the user changed password on the same device.
    kUnknown,  // The token status is unknown.
  };

  // `account_id`: The account for which the token handle check was performed.
  // `token`: The token which was checked. Empty if we could not find a token
  // handle for `account_id`.
  // `reauth_required`: Result of the of `IsReauthRequired()`. `true` means that
  // reauthentication is required for `account_id`.
  using TokenValidationCallback =
      base::OnceCallback<void(const AccountId& account_id,
                              const std::string& token,
                              bool reauth_required)>;

  // Returns true if UserManager has token handle associated with `account_id`.
  static bool HasToken(const AccountId& account_id);

  // Returns true if the token status for `account_id` was checked recently
  // (within kCacheStatusTime).
  static bool IsRecentlyChecked(const AccountId& account_id);

  // Indicates if token handle for `account_id` is missing or marked as invalid.
  static bool ShouldObtainHandle(const AccountId& account_id);

  // Performs token handle check for `account_id`. Will call `callback` with
  // corresponding result. See `TokenValidationCallback` for details.
  void IsReauthRequired(
      const AccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TokenValidationCallback callback);

  // Given the token `handle` store it for `account_id`.
  static void StoreTokenHandle(const AccountId& account_id,
                               const std::string& handle);

  static void SetInvalidTokenForTesting(const char* token);

  static void SetLastCheckedPrefForTesting(const AccountId& account_id,
                                           base::Time time);

 private:
  // Associates GaiaOAuthClient::Delegate with User ID and Token.
  class TokenDelegate : public gaia::GaiaOAuthClient::Delegate {
   public:
    using TokenDelegateCallback =
        base::OnceCallback<void(const AccountId& account_id,
                                const std::string& token,
                                const Status& status)>;
    TokenDelegate(
        const base::WeakPtr<TokenHandleUtil>& owner,
        const AccountId& account_id,
        const std::string& token,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        TokenDelegateCallback callback);

    TokenDelegate(const TokenDelegate&) = delete;
    TokenDelegate& operator=(const TokenDelegate&) = delete;

    ~TokenDelegate() override;

    // gaia::GaiaOAuthClient::Delegate overrides.
    void OnOAuthError() override;
    void OnNetworkError(int response_code) override;
    void OnGetTokenInfoResponse(const base::Value::Dict& token_info) override;

    // Completes the validation request at the owner TokenHandleUtil. The bool
    // flag signals if we actually got any data from the Gaia endpoint.
    void NotifyDone(bool request_completed);

   private:
    void RecordTokenCheckResponseTime();

    base::WeakPtr<TokenHandleUtil> owner_;
    AccountId account_id_;
    std::string token_;
    base::TimeTicks tokeninfo_response_start_time_;
    TokenDelegateCallback callback_;
    gaia::GaiaOAuthClient gaia_client_;
  };

  // Callback passed to `TokenDelegate`.
  void OnStatusChecked(TokenValidationCallback callback,
                       const AccountId& account_id,
                       const std::string& token,
                       const Status& status);
  void OnValidationComplete(const std::string& token);

  // Map of pending check operations.
  base::flat_map<std::string, std::unique_ptr<TokenDelegate>>
      validation_delegates_;

  AuthFactorEditor factor_editor_;

  base::WeakPtrFactory<TokenHandleUtil> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_
