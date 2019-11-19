// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <utility>

#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/token_encryptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class DeviceOAuth2TokenServiceTest : public testing::Test {
 public:
  DeviceOAuth2TokenServiceTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  // Most tests just want a noop crypto impl with a dummy refresh token value in
  // Local State (if the value is an empty string, it will be ignored).
  void SetUpDefaultValues() {
    SetDeviceRefreshTokenInLocalState("device_refresh_token_4_test");
    SetRobotAccountId("service_acct@g.com");
    CreateService();
    AssertConsumerTokensAndErrors(0, 0);

    base::RunLoop().RunUntilIdle();
  }

  void SetUpWithPendingSalt() {
    FakeCryptohomeClient::Get()->set_system_salt(std::vector<uint8_t>());
    FakeCryptohomeClient::Get()->SetServiceIsAvailable(false);
    SetUpDefaultValues();
  }

  void SetRobotAccountId(const std::string& account_id) {
    device_policy_.policy_data().set_service_account_identity(account_id);
    device_policy_.Build();
    session_manager_client_.set_device_policy(device_policy_.GetBlob());
    DeviceSettingsService::Get()->Load();
    content::RunAllTasksUntilIdle();
  }

  std::unique_ptr<OAuth2AccessTokenManager::Request> StartTokenRequest() {
    return oauth2_service_->StartAccessTokenRequest(
        oauth2_service_->GetRobotAccountId(), std::set<std::string>(),
        &consumer_);
  }

  void SetUp() override {
    CryptohomeClient::InitializeFake();
    FakeCryptohomeClient::Get()->SetServiceIsAvailable(true);
    FakeCryptohomeClient::Get()->set_system_salt(
        FakeCryptohomeClient::GetStubSystemSalt());

    SystemSaltGetter::Initialize();

    scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_(
        new ownership::MockOwnerKeyUtil());
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    DeviceSettingsService::Get()->SetSessionManager(&session_manager_client_,
                                                    owner_key_util_);
  }

  void TearDown() override {
    oauth2_service_.reset();
    base::ThreadPoolInstance::Get()->FlushForTesting();
    DeviceSettingsService::Get()->UnsetSessionManager();
    SystemSaltGetter::Shutdown();
    CryptohomeClient::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void CreateService() {
    oauth2_service_.reset(new DeviceOAuth2TokenService(
        test_url_loader_factory_.GetSafeWeakWrapper(),
        scoped_testing_local_state_.Get()));
    oauth2_service_->max_refresh_token_validation_retries_ = 0;
    oauth2_service_->GetAccessTokenManager()
        ->set_max_authorization_token_fetch_retries_for_testing(0);
  }

  // Utility method to set a value in Local State for the device refresh token
  // (it must have a non-empty value or it won't be used).
  void SetDeviceRefreshTokenInLocalState(const std::string& refresh_token) {
    scoped_testing_local_state_.Get()->SetUserPref(
        prefs::kDeviceRobotAnyApiRefreshToken,
        std::make_unique<base::Value>(refresh_token));
  }

  std::string GetValidTokenInfoResponse(const std::string& email) {
    return "{ \"email\": \"" + email + "\","
           "  \"user_id\": \"1234567890\" }";
  }

  bool RefreshTokenIsAvailable() {
    return oauth2_service_->RefreshTokenIsAvailable(
        oauth2_service_->GetRobotAccountId());
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
  ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ScopedTestingLocalState scoped_testing_local_state_;
  ScopedTestDeviceSettingsService scoped_device_settings_service_;
  ScopedTestCrosSettings scoped_test_cros_settings_{
      scoped_testing_local_state_.Get()};
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeSessionManagerClient session_manager_client_;
  policy::DevicePolicyBuilder device_policy_;
  std::unique_ptr<DeviceOAuth2TokenService, TokenServiceDeleter>
      oauth2_service_;
  TestingOAuth2AccessTokenManagerConsumer consumer_;
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
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("scoped_access_token", 3600));
}

void DeviceOAuth2TokenServiceTest::AssertConsumerTokensAndErrors(
    int num_tokens,
    int num_errors) {
  EXPECT_EQ(num_tokens, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(num_errors, consumer_.number_of_errors_);
}

TEST_F(DeviceOAuth2TokenServiceTest, SaveEncryptedToken) {
  CreateService();

  // The token service won't report there being a token if the robot account ID
  // is not set, which would cause the expectation below to fail.
  SetRobotAccountId("service_acct@g.com");

  oauth2_service_->SetAndSaveRefreshToken(
      "test-token", DeviceOAuth2TokenService::StatusCallback());
  EXPECT_EQ("test-token", GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenServiceTest, SaveEncryptedTokenEarly) {
  // Set a new refresh token without the system salt available.
  SetUpWithPendingSalt();

  oauth2_service_->SetAndSaveRefreshToken(
      "test-token", DeviceOAuth2TokenService::StatusCallback());
  EXPECT_EQ("test-token", GetRefreshToken());

  // Make the system salt available.
  FakeCryptohomeClient::Get()->set_system_salt(
      FakeCryptohomeClient::GetStubSystemSalt());
  FakeCryptohomeClient::Get()->SetServiceIsAvailable(true);
  base::RunLoop().RunUntilIdle();

  // The original token should still be present.
  EXPECT_EQ("test-token", GetRefreshToken());

  // Reloading shouldn't change the token either.
  CreateService();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("test-token", GetRefreshToken());
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
  SetUpWithPendingSalt();

  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();
  PerformURLFetches();
  AssertConsumerTokensAndErrors(0, 0);

  FakeCryptohomeClient::Get()->set_system_salt(
      FakeCryptohomeClient::GetStubSystemSalt());
  FakeCryptohomeClient::Get()->SetServiceIsAvailable(true);
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

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_NoSalt) {
  FakeCryptohomeClient::Get()->set_system_salt(std::vector<uint8_t>());
  FakeCryptohomeClient::Get()->SetServiceIsAvailable(true);
  SetUpDefaultValues();

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

  PerformURLFetchesWithResults(
      net::HTTP_UNAUTHORIZED, "",
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoAccessTokenInvalidResponse) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, "invalid response",
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoApiCallHttpError) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_INTERNAL_SERVER_ERROR, "",
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoApiCallInvalidResponse) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, "invalid response",
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_CloudPrintAccessTokenHttpError) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
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
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, "invalid request");

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Failure_BadOwner) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  SetRobotAccountId("WRONG_service_acct@g.com");

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Retry) {
  SetUpDefaultValues();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request =
      StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_INTERNAL_SERVER_ERROR, "",
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);

  // Retry should succeed.
  request = StartTokenRequest();
  PerformURLFetches();
  AssertConsumerTokensAndErrors(1, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest, DoNotAnnounceTokenWithoutAccountID) {
  CreateService();

  auto callback_without_id = base::BindRepeating(
      [](const CoreAccountId& account_id) { EXPECT_TRUE(false); });
  oauth2_service_->SetRefreshTokenAvailableCallback(
      std::move(callback_without_id));

  // Make a token available during enrollment. Verify that the token is not
  // announced yet.
  oauth2_service_->SetAndSaveRefreshToken(
      "test-token", DeviceOAuth2TokenService::StatusCallback());

  base::RunLoop run_loop;
  auto callback_with_id =
      base::BindRepeating([](base::RunLoop* loop,
                             const CoreAccountId& account_id) { loop->Quit(); },
                          &run_loop);
  oauth2_service_->SetRefreshTokenAvailableCallback(
      std::move(callback_with_id));

  // Also make the robot account ID available. Verify that the token is
  // announced now.
  SetRobotAccountId("robot@example.com");
  run_loop.Run();
}

}  // namespace chromeos
