// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_

#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"

class AccountId;

namespace base {
class DictionaryValue;
}

// This class is responsible for operations with External Token Handle.
// Handle is an extra token associated with OAuth refresh token that have
// exactly same lifetime. It is not secure, and it's only purpose is checking
// validity of corresponding refresh token in the insecure environment.
class TokenHandleUtil {
 public:
  TokenHandleUtil();
  ~TokenHandleUtil();

  enum TokenHandleStatus { VALID, INVALID, UNKNOWN };

  using TokenValidationCallback =
      base::Callback<void(const AccountId&, TokenHandleStatus)>;

  // Returns true if UserManager has token handle associated with |account_id|.
  bool HasToken(const AccountId& account_id);

  // Removes token handle for |account_id| from UserManager storage.
  void DeleteHandle(const AccountId& account_id);

  // Marks current handle as invalid, new one should be obtained at next sign
  // in.
  void MarkHandleInvalid(const AccountId& account_id);

  // Indicates if token handle for |account_id| is missing or marked as invalid.
  bool ShouldObtainHandle(const AccountId& account_id);

  // Performs token handle check for |account_id|. Will call |callback| with
  // corresponding result.
  void CheckToken(const AccountId& account_id,
                  const TokenValidationCallback& callback);

  // Given the token |handle| store it for |account_id|.
  void StoreTokenHandle(const AccountId& account_id, const std::string& handle);

 private:
  // Associates GaiaOAuthClient::Delegate with User ID and Token.
  class TokenDelegate : public gaia::GaiaOAuthClient::Delegate {
   public:
    TokenDelegate(const base::WeakPtr<TokenHandleUtil>& owner,
                  const AccountId& account_id,
                  const std::string& token,
                  const TokenValidationCallback& callback);
    ~TokenDelegate() override;
    void OnOAuthError() override;
    void OnNetworkError(int response_code) override;
    void OnGetTokenInfoResponse(
        std::unique_ptr<base::DictionaryValue> token_info) override;
    void NotifyDone();

   private:
    base::WeakPtr<TokenHandleUtil> owner_;
    AccountId account_id_;
    std::string token_;
    base::TimeTicks tokeninfo_response_start_time_;
    TokenValidationCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(TokenDelegate);
  };

  void OnValidationComplete(const std::string& token);

  // Map of pending check operations.
  std::unordered_map<std::string, std::unique_ptr<TokenDelegate>>
      validation_delegates_;

  // Instance of GAIA Client.
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_client_;

  base::WeakPtrFactory<TokenHandleUtil> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TokenHandleUtil);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_
