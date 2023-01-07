// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_LOGIN_WILDCARD_LOGIN_CHECKER_H_
#define CHROME_BROWSER_ASH_POLICY_LOGIN_WILDCARD_LOGIN_CHECKER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/user_info_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace policy {

class PolicyOAuth2TokenFetcher;

// Performs online verification whether wildcard login is allowed, i.e. whether
// the user is a hosted user. This class performs an asynchronous check and
// reports the result via a callback.
class WildcardLoginChecker : public UserInfoFetcher::Delegate {
 public:
  // Indicates result of the wildcard login check.
  enum Result {
    RESULT_ALLOWED,  // Wildcard check succeeded, login allowed.
    RESULT_BLOCKED,  // Check completed, but user should be blocked.
    RESULT_FAILED,   // Failure due to network errors etc.
  };

  using StatusCallback = base::OnceCallback<void(Result)>;

  WildcardLoginChecker();

  WildcardLoginChecker(const WildcardLoginChecker&) = delete;
  WildcardLoginChecker& operator=(const WildcardLoginChecker&) = delete;

  virtual ~WildcardLoginChecker();

  // Starts checking with a provided refresh token.
  void StartWithRefreshToken(const std::string& refresh_token,
                             StatusCallback callback);

  // Starts checking with a provided access token.
  void StartWithAccessToken(const std::string& access_token,
                            StatusCallback callback);

  // UserInfoFetcher::Delegate:
  void OnGetUserInfoSuccess(const base::Value::Dict& response) override;
  void OnGetUserInfoFailure(const GoogleServiceAuthError& error) override;

 private:
  // Starts the check after successful token minting.
  void OnPolicyTokenFetched(const std::string& access_token,
                            const GoogleServiceAuthError& error);

  // Starts the user info fetcher.
  void StartUserInfoFetcher(const std::string& access_token);

  // Handles the response of the check and calls ReportResult().
  void OnCheckCompleted(Result result);

  StatusCallback callback_;

  std::unique_ptr<PolicyOAuth2TokenFetcher> token_fetcher_;
  std::unique_ptr<UserInfoFetcher> user_info_fetcher_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_LOGIN_WILDCARD_LOGIN_CHECKER_H_
