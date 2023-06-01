// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/device_oauth2_token_service.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kRobotEmail[] = "service_acct@system.gserviceaccount.com";
const char kWrongRobotEmail[] = "WRONG_service_acct@system.gserviceaccount.com";
}  // namespace

class MockDeviceOAuth2TokenStore : public DeviceOAuth2TokenStore {
 public:
  MockDeviceOAuth2TokenStore() = default;
  ~MockDeviceOAuth2TokenStore() override = default;

  // DeviceOAuth2TokenStore:
  void Init(InitCallback callback) override {
    pending_init_callback_ = std::move(callback);
  }
  CoreAccountId GetAccountId() const override { return account_id_; }
  std::string GetRefreshToken() const override { return refresh_token_; }

  void SetAndSaveRefreshToken(const std::string& refresh_token,
                              StatusCallback result_callback) override {
    refresh_token_ = refresh_token;
    pending_status_callback_ = std::move(result_callback);
  }

  void PrepareTrustedAccountId(TrustedAccountIdCallback callback) override {
    pending_trusted_account_id_callback_ = std::move(callback);
    TriggerTrustedAccountIdCallback(true);
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void SetAccountEmail(const std::string& account_email) override {
    account_id_ = CoreAccountId::FromRobotEmail(account_email);
  }
#endif

  // Mock-specific functions:
  void SetRefreshTokenForTesting(const std::string& token) {
    refresh_token_ = token;
  }

  void SetAccountIdForTesting(CoreAccountId account_id) {
    account_id_ = account_id;
  }

  void TriggerInitCallback(bool success, bool validation_required) {
    std::move(pending_init_callback_).Run(success, validation_required);
  }

  void TriggerStatusCallback(bool success) {
    std::move(pending_status_callback_).Run(success);
  }

  void TriggerTrustedAccountIdCallback(bool account_present) {
    std::move(pending_trusted_account_id_callback_).Run(account_present);
  }

 private:
  CoreAccountId account_id_;
  std::string refresh_token_;

  InitCallback pending_init_callback_;
  StatusCallback pending_status_callback_;
  TrustedAccountIdCallback pending_trusted_account_id_callback_;
};

class DeviceOAuth2TokenServiceTest : public testing::Test {
 public:
  DeviceOAuth2TokenServiceTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  // Most tests just want a noop crypto impl with a dummy refresh token value in
  // Local State (if the value is an empty string, it will be ignored).
  void SetUpDefaultValues() {
    CreateService();
    token_store_->SetRefreshTokenForTesting("device_refresh_token_4_test");
    SetRobotAccountId(kRobotEmail);
    AssertConsumerTokensAndErrors(0, 0);

    token_store_->TriggerInitCallback(true, true);
  }

  void SetRobotAccountId(const std::string& robot_email) {
    token_store_->SetAccountIdForTesting(
        CoreAccountId::FromRobotEmail(robot_email));
  }

  std::unique_ptr<OAuth2AccessTokenManager::Request> StartTokenRequest() {
    return oauth2_service_->StartAccessTokenRequest(std::set<std::string>(),
                                                    &consumer_);
  }

  void SetUp() override {}

  void TearDown() override {
    oauth2_service_.reset();
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  MockDeviceOAuth2TokenStore* token_store() { return token_store_; }

  void CreateService() {
    auto store = std::make_unique<MockDeviceOAuth2TokenStore>();
    token_store_ = store.get();

    oauth2_service_.reset(new DeviceOAuth2TokenService(
        test_url_loader_factory_.GetSafeWeakWrapper(), std::move(store)));
    oauth2_service_->max_refresh_token_validation_retries_ = 0;
    oauth2_service_->GetAccessTokenManager()
        ->set_max_authorization_token_fetch_retries_for_testing(0);
  }

  std::string GetValidTokenInfoResponse(const std::string& email) {
    return "{ \"email\": \"" + email +
           "\","
           "  \"user_id\": \"1234567890\" }";
  }

  std::string GetInvalidScopeResponse(const std::string& scope) {
    return "{ \"error\": \"invalid_scope\", "
           "\"error_description\": \"Some requested scopes were invalid. "
           "{invalid\\u003d[" +
           scope +
           "}\", "
           "\"error_uri\": "
           "\"https://developers.google.com/identity/protocols/oauth2\""
           "}";
  }

  bool RefreshTokenIsAvailable() {
    return oauth2_service_->RefreshTokenIsAvailable();
  }

  std::string GetRefreshToken() {
    if (!RefreshTokenIsAvailable())
      return std::string();
    return oauth2_service_->GetRefreshToken();
  }

  // A utility method to return fake URL results, for testing the refresh token
  // validation logic.  For a successful validation attempt, this method will be
  // called three times for the steps listed below.
  //
  // Step 1a: fetch the access token for the tokeninfo API.
  // Step 1b: call the tokeninfo API.
  // Step 2:  Fetch the access token for the requested scope
  //          (in this case, cloudprint).
  void ReturnOAuthUrlFetchResults(const std::string& url,
                                  net::HttpStatusCode response_code,
                                  const std::string& response_string);

  // Generates URL fetch replies with the specified results for requests
  // generated by the token service.
  void PerformURLFetchesWithResults(
      net::HttpStatusCode tokeninfo_access_token_status,
      const std::string& tokeninfo_access_token_response,
      net::HttpStatusCode tokeninfo_fetch_status,
      const std::string& tokeninfo_fetch_response,
      net::HttpStatusCode service_access_token_status,
      const std::string& service_access_token_response);

  // Generates URL fetch replies for the success path.
  void PerformURLFetches();

  void AssertConsumerTokensAndErrors(int num_tokens, int num_errors);

 protected:
  // This is here because DeviceOAuth2TokenService's destructor is private;
  // base::DefaultDeleter therefore doesn't work. However, the test class is
  // declared friend in DeviceOAuth2TokenService, so this deleter works.
  struct TokenServiceDeleter {
    inline void operator()(DeviceOAuth2TokenService* ptr) const { delete ptr; }
  };

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_testing_local_state_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DeviceOAuth2TokenService, TokenServiceDeleter>
      oauth2_service_;
  TestingOAuth2AccessTokenManagerConsumer consumer_;
  raw_ptr<MockDeviceOAuth2TokenStore, DanglingUntriaged> token_store_;
};

void DeviceOAuth2TokenServiceTest::ReturnOAuthUrlFetchResults(
    const std::string& url,
    net::HttpStatusCode response_code,
    const std::string& response_string) {
  if (test_url_loader_factory_.IsPending(url)) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(url), network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(response_code), response_string);
  }
}

void DeviceOAuth2TokenServiceTest::PerformURLFetchesWithResults(
    net::HttpStatusCode tokeninfo_access_token_status,
    const std::string& tokeninfo_access_token_response,
    net::HttpStatusCode tokeninfo_fetch_status,
    const std::string& tokeninfo_fetch_response,
    net::HttpStatusCode service_access_token_status,
    const std::string& service_access_token_response) {
  ReturnOAuthUrlFetchResults(GaiaUrls::GetInstance()->oauth2_token_url().spec(),
                             tokeninfo_access_token_status,
                             tokeninfo_access_token_response);

  ReturnOAuthUrlFetchResults(
      GaiaUrls::GetInstance()->oauth2_token_info_url().spec(),
      tokeninfo_fetch_status, tokeninfo_fetch_response);

  ReturnOAuthUrlFetchResults(GaiaUrls::GetInstance()->oauth2_token_url().spec(),
                             service_access_token_status,
                             service_access_token_response);
}

void DeviceOAuth2TokenServiceTest::PerformURLFetches() {
  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse(kRobotEmail), net::HTTP_OK,
      GetValidTokenResponse("scoped_access_token", 3600));
}

void DeviceOAuth2TokenServiceTest::AssertConsumerTokensAndErrors(
    int num_tokens,
    int num_errors) {
  EXPECT_EQ(num_tokens, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(num_errors, consumer_.number_of_errors_);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Success) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetches();
  AssertConsumerTokensAndErrors(1, 0);

  EXPECT_EQ("scoped_access_token", consumer_.last_token_);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_SuccessAsyncLoad) {
  CreateService();
  token_store()->SetRefreshTokenForTesting("device_refresh_token_4_test");
  SetRobotAccountId(kRobotEmail);

  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();
  PerformURLFetches();
  AssertConsumerTokensAndErrors(0, 0);

  token_store()->TriggerInitCallback(true, true);
  base::RunLoop().RunUntilIdle();

  PerformURLFetches();
  AssertConsumerTokensAndErrors(1, 0);

  EXPECT_EQ("scoped_access_token", consumer_.last_token_);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Cancel) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();
  request.reset();

  PerformURLFetches();

  // Test succeeds if this line is reached without a crash.
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_InitFailure) {
  CreateService();
  token_store()->SetRefreshTokenForTesting("device_refresh_token_4_test");
  SetRobotAccountId(kRobotEmail);
  token_store()->TriggerInitCallback(false, true);

  EXPECT_FALSE(RefreshTokenIsAvailable());

  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();
  base::RunLoop().RunUntilIdle();

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoAccessTokenHttpError) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(net::HTTP_UNAUTHORIZED, "", net::HTTP_OK,
                               GetValidTokenInfoResponse(kRobotEmail),
                               net::HTTP_OK,
                               GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoAccessTokenInvalidResponse) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(net::HTTP_OK, "invalid response", net::HTTP_OK,
                               GetValidTokenInfoResponse(kRobotEmail),
                               net::HTTP_OK,
                               GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_InvalidScope) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse(kRobotEmail),
      net::HTTP_BAD_REQUEST, GetInvalidScopeResponse("test_scope"));

  AssertConsumerTokensAndErrors(0, 1);
  EXPECT_EQ(consumer_.last_error_.state(),
            GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR);
  EXPECT_EQ(
      consumer_.last_error_.error_message(),
      "{ \"error\": \"invalid_scope\", \"error_description\": \"Some requested "
      "scopes were invalid. {invalid\\u003d[test_scope}\", \"error_uri\": "
      "\"https://developers.google.com/identity/protocols/oauth2\"}");
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoApiCallHttpError) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_INTERNAL_SERVER_ERROR, "", net::HTTP_OK,
      GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoApiCallInvalidResponse) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, "invalid response", net::HTTP_OK,
      GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_CloudPrintAccessTokenHttpError) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse(kRobotEmail),
      net::HTTP_BAD_REQUEST, "");

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_CloudPrintAccessTokenInvalidResponse) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse(kRobotEmail), net::HTTP_OK,
      "invalid request");

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Failure_BadOwner) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  SetRobotAccountId(kWrongRobotEmail);

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse(kRobotEmail), net::HTTP_OK,
      GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Retry) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_INTERNAL_SERVER_ERROR, "", net::HTTP_OK,
      GetValidTokenInfoResponse(kRobotEmail), net::HTTP_OK,
      GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);

  // Retry should succeed.
  request = StartTokenRequest();
  PerformURLFetches();
  AssertConsumerTokensAndErrors(1, 1);
}
