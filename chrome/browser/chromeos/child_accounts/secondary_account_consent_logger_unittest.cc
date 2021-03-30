// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/secondary_account_consent_logger.h"

#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kAccountEmail[] = "user@gmail.com";
constexpr char kSecondaryEmail[] = "secondary@gmail.com";
constexpr char kParentObfuscatedGaiaId[] = "parent-obfuscated-gaia-id";
constexpr char kReAuthProofToken[] = "re-auth-proof-token";
constexpr char kChromeSyncId[] = "test-chrome-id";
constexpr char kRequestBodyTemplate[] = R"({
   "chrome_os_consent": {
      "chrome_os_unicorn_edu_coexistence_id": "$1",
      "secondary_account_email": "$2",
      "parent_id": "$3",
      "parent_rapt": "$4",
      "text_version": "$5"
   },
   "person_id": "me"
})";
constexpr char kConsentScreenTextVersion[] = "v2353089";

std::string GetTestRequestBody(
    const std::string& chrome_os_unicorn_edu_coexistence_id,
    const std::string& secondary_account_email,
    const std::string& parent_id,

    const std::string& parent_rapt,
    const std::string& text_version) {
  std::vector<std::string> params;
  params.push_back(chrome_os_unicorn_edu_coexistence_id);
  params.push_back(secondary_account_email);
  params.push_back(parent_id);
  params.push_back(parent_rapt);
  params.push_back(text_version);
  return base::ReplaceStringPlaceholders(kRequestBodyTemplate, params, nullptr);
}

}  // namespace

class SecondaryAccountConsentLoggerTest : public testing::Test {
 public:
  SecondaryAccountConsentLoggerTest() = default;

  void SetUp() {
    SecondaryAccountConsentLogger::RegisterPrefs(local_state_.registry());
    local_state_.SetUserPref(chromeos::prefs::kEduCoexistenceId,
                             std::make_unique<base::Value>(kChromeSyncId));
  }

  void CreateLogger() {
    logger_ = std::make_unique<SecondaryAccountConsentLogger>(
        identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        &local_state_, kSecondaryEmail, kParentObfuscatedGaiaId,
        kReAuthProofToken,
        base::BindOnce(&SecondaryAccountConsentLoggerTest::OnConsentLogged,
                       base::Unretained(this)));
  }

  MOCK_METHOD(void,
              OnConsentLogged,
              (SecondaryAccountConsentLogger::Result result),
              ());

  void CreateAndStartLogging() {
    if (logger_ == nullptr) {
      CreateLogger();
    }
    logger_->StartLogging();
  }

  CoreAccountInfo SetPrimaryAccount() {
    return identity_test_env_.SetUnconsentedPrimaryAccount(kAccountEmail);
  }

  void IssueRefreshTokenForPrimaryAccount() {
    identity_test_env_.MakeUnconsentedPrimaryAccountAvailable(kAccountEmail);
  }

  void SendResponse(int net_error, int response_code) {
    logger_->OnSimpleLoaderCompleteInternal(net_error, response_code);
  }

  void WaitForAccessTokenRequestAndIssueToken() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        identity_test_env_.identity_manager()->GetPrimaryAccountId(
            signin::ConsentLevel::kSignin),
        "access_token", base::Time::Now() + base::TimeDelta::FromHours(1));
  }

  base::DictionaryValue CreateRequestBody() {
    return logger_->CreateRequestBody();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<SecondaryAccountConsentLogger> logger_;
};

TEST_F(SecondaryAccountConsentLoggerTest, LoggingSuccess) {
  IssueRefreshTokenForPrimaryAccount();
  CreateAndStartLogging();
  WaitForAccessTokenRequestAndIssueToken();
  EXPECT_CALL(*this,
              OnConsentLogged(SecondaryAccountConsentLogger::Result::kSuccess));
  SendResponse(net::OK, net::HTTP_OK);
}

TEST_F(SecondaryAccountConsentLoggerTest, NetworkError) {
  IssueRefreshTokenForPrimaryAccount();
  CreateAndStartLogging();
  WaitForAccessTokenRequestAndIssueToken();
  EXPECT_CALL(*this, OnConsentLogged(
                         SecondaryAccountConsentLogger::Result::kNetworkError));
  SendResponse(-320 /*INVALID_RESPONSE*/, net::HTTP_OK);
}

TEST_F(SecondaryAccountConsentLoggerTest, ResponceError) {
  IssueRefreshTokenForPrimaryAccount();
  CreateAndStartLogging();
  WaitForAccessTokenRequestAndIssueToken();
  EXPECT_CALL(*this, OnConsentLogged(
                         SecondaryAccountConsentLogger::Result::kNetworkError));
  SendResponse(net::OK, 400 /*BAD_REQUEST*/);
}

TEST_F(SecondaryAccountConsentLoggerTest, TokenError) {
  IssueRefreshTokenForPrimaryAccount();
  CreateAndStartLogging();

  // On failure to get an access token we expect a token error.
  EXPECT_CALL(*this, OnConsentLogged(
                         SecondaryAccountConsentLogger::Result::kTokenError));
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

TEST_F(SecondaryAccountConsentLoggerTest, SuccessAfterWaitingForRefreshToken) {
  // Early set the primary account so that the fetcher is created with a proper
  // account_id. We don't use IssueRefreshToken() as it also sets a refresh
  // token for the primary account and that's something we don't want for this
  // test.
  CoreAccountInfo account_info = SetPrimaryAccount();
  CreateAndStartLogging();

  // Since there is no refresh token yet, we should not get a request for an
  // access token at this point.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_test_env_.SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());

  // In this case we don't directly call IssueRefreshToken() as it sets the
  // primary account. We already have a primary account set so we cannot set
  // another one.
  identity_test_env_.SetRefreshTokenForAccount(account_info.account_id);

  // Do reset the callback for access token request before using the Wait* APIs.
  identity_test_env_.SetCallbackForNextAccessTokenRequest(base::OnceClosure());
  WaitForAccessTokenRequestAndIssueToken();

  EXPECT_CALL(*this,
              OnConsentLogged(SecondaryAccountConsentLogger::Result::kSuccess));
  SendResponse(net::OK, net::HTTP_OK);
}

TEST_F(SecondaryAccountConsentLoggerTest, NoRefreshToken) {
  // Set the primary account before creating the fetcher to allow it to properly
  // retrieve the primary account_id from IdentityManager. We don't call
  // IssueRefreshToken because we don't want it to precisely issue a refresh
  // token for the primary account, just set it.
  SetPrimaryAccount();
  CreateAndStartLogging();
  identity_test_env_.MakeAccountAvailable(kSecondaryEmail);

  // Credentials for a different user should be ignored, i.e. not result in a
  // request for an access token.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_test_env_.SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());
}

TEST_F(SecondaryAccountConsentLoggerTest, RequestBody) {
  CoreAccountInfo account_info = SetPrimaryAccount();
  CreateLogger();
  base::Optional<base::Value> test_request_body =
      base::JSONReader::Read(GetTestRequestBody(
          kChromeSyncId, kSecondaryEmail, kParentObfuscatedGaiaId,
          kReAuthProofToken, kConsentScreenTextVersion));
  base::DictionaryValue request_body = CreateRequestBody();
  EXPECT_EQ(test_request_body.value(), request_body);
}
