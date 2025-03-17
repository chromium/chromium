// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_CHECKER_H_
#define CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_CHECKER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

extern const char kTokenCheckResponseTime[];

// This class is responsible for executing token handle checks on
// behalf of `TokenHandleStore`.
class TokenHandleChecker : public gaia::GaiaOAuthClient::Delegate {
 public:
  // Status of the token handle.
  enum class Status {
    kValid,    // The token is valid and reauthentication is not required.
    kInvalid,  // The token is invalid and reauthentication is required.
    kExpired,  // The token is valid but expired. This can happen if the user
               // changed password on the same device.
    kUnknown,  // The token status is unknown.
  };

  using OnTokenChecked = base::OnceCallback<
      void(const AccountId&, const std::string&, const Status&)>;

  TokenHandleChecker(
      const AccountId& account_id,
      const std::string& token,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  TokenHandleChecker(const TokenHandleChecker&) = delete;
  TokenHandleChecker& operator=(const TokenHandleChecker&) = delete;

  ~TokenHandleChecker() override;

  // Start the token handle check by querying the tokeninfo endpoint with
  // `token_`.
  void StartCheck(OnTokenChecked callback);

 private:
  // gaia::GaiaOAuthClient::Delegate overrides.
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;
  void OnGetTokenInfoResponse(const base::Value::Dict& token_info) override;

  // Completes the validation request.
  // `outcome` denotes the status of token handle.
  // `should_record_response_time` denotes if response time metrics should be
  // logged.
  void SendCallbackResponse(const Status& outcome,
                            bool should_record_response_time);

  void RecordTokenCheckResponseTime();

  const AccountId account_id_;
  const std::string token_;
  base::TimeTicks tokeninfo_response_start_time_;
  gaia::GaiaOAuthClient gaia_client_;
  OnTokenChecked callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SIGNIN_TOKEN_HANDLE_CHECKER_H_
