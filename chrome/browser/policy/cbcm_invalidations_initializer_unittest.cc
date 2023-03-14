// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cbcm_invalidations_initializer.h"

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

static const char kFirstRefreshToken[] = "first_refresh_token";
static const char kSecondRefreshToken[] = "second_refresh_token";
static const char kFirstAccessToken[] = "first_access_token";
static const char kSecondAccessToken[] = "second_access_token";
static const char kServiceAccountEmail[] =
    "service_account@system.gserviceaccount.com";
static const char kOtherServiceAccountEmail[] =
    "other_service_account@system.gserviceaccount.com";
static const char kDMToken[] = "dm_token";
static const char kAuthCode[] = "auth_code";

}  // namespace

class FakeCloudPolicyClient : public MockCloudPolicyClient {
 public:
  void FetchRobotAuthCodes(
      DMAuth auth,
      enterprise_management::DeviceServiceApiAccessRequest::DeviceType
          device_type,
      const std::set<std::string>& oauth_scopes,
      RobotAuthCodeCallback callback) override {
    std::move(callback).Run(DM_STATUS_SUCCESS, kAuthCode);
  }
};

class CBCMInvalidationsInitializerTest
    : public testing::Test,
      public CBCMInvalidationsInitializer::Delegate {
 public:
  CBCMInvalidationsInitializerTest() = default;

  void RefreshTokenSavedCallbackExpectSuccess(bool success) {
    EXPECT_TRUE(success);

    ++num_refresh_tokens_saved_;
  }

  // CBCMInvalidationsInitializer::Delegate:
  void StartInvalidations() override { ++num_invalidations_started_; }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return test_url_loader_factory_.GetSafeWeakWrapper();
  }

  bool IsInvalidationsServiceStarted() const override {
    return num_invalidations_started_ > 0;
  }

 protected:
  int num_refresh_tokens_saved() const { return num_refresh_tokens_saved_; }

  int num_invalidations_started() const { return num_invalidations_started_; }

  FakeCloudPolicyClient* policy_client() { return &mock_policy_client_; }

  TestingPrefServiceSimple* testing_local_state() {
    return &testing_local_state_;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  std::string MakeTokensFromAuthCodesResponse(const std::string& refresh_token,
                                              const std::string& access_token) {
    base::Value::Dict dict;
    dict.Set("access_token", access_token);
    dict.Set("refresh_token", refresh_token);
    dict.Set("expires_in", 9999);

    std::string json;
    base::JSONWriter::Write(dict, &json);
    return json;
  }

 private:
  void SetUp() override {
    DeviceOAuth2TokenStoreDesktop::RegisterPrefs(
        testing_local_state_.registry());
    DeviceOAuth2TokenServiceFactory::Initialize(GetURLLoaderFactory(),
                                                &testing_local_state_);
    OSCryptMocker::SetUp();
    mock_policy_client_.SetDMToken(kDMToken);
  }

  void TearDown() override {
    DeviceOAuth2TokenServiceFactory::Shutdown();
    OSCryptMocker::TearDown();
  }

  int num_refresh_tokens_saved_ = 0;
  int num_invalidations_started_ = 0;

  FakeCloudPolicyClient mock_policy_client_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple testing_local_state_;
};

TEST_F(CBCMInvalidationsInitializerTest, InvalidationsStartDisabled) {
  CBCMInvalidationsInitializer initializer(this);

  EXPECT_FALSE(IsInvalidationsServiceStarted());
}

TEST_F(CBCMInvalidationsInitializerTest,
       InvalidationsStartIfRefreshTokenPresent) {
  CBCMInvalidationsInitializer initializer(this);

  DeviceOAuth2TokenServiceFactory::Get()->SetServiceAccountEmail(
      kServiceAccountEmail);
  DeviceOAuth2TokenServiceFactory::Get()->SetAndSaveRefreshToken(
      kFirstRefreshToken,
      base::BindRepeating(&CBCMInvalidationsInitializerTest::
                              RefreshTokenSavedCallbackExpectSuccess,
                          base::Unretained(this)));

  EXPECT_EQ(1, num_refresh_tokens_saved());
  EXPECT_TRUE(
      DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable());

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);

  EXPECT_TRUE(IsInvalidationsServiceStarted());
}

TEST_F(CBCMInvalidationsInitializerTest,
       InvalidationsStartIfRefreshTokenAbsent) {
  CBCMInvalidationsInitializer initializer(this);

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);
  EXPECT_EQ(1, test_url_loader_factory()->NumPending());
  EXPECT_EQ(GaiaUrls::GetInstance()->oauth2_token_url().spec(),
            test_url_loader_factory()->GetPendingRequest(0)->request.url);

  EXPECT_TRUE(IsInvalidationsServiceStarted());

  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      MakeTokensFromAuthCodesResponse(kFirstRefreshToken, kFirstAccessToken)));

  EXPECT_TRUE(IsInvalidationsServiceStarted());
}

TEST_F(CBCMInvalidationsInitializerTest,
       InvalidationsDontRestartOnNextPolicyFetch) {
  CBCMInvalidationsInitializer initializer(this);

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);

  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      MakeTokensFromAuthCodesResponse(kFirstRefreshToken, kFirstAccessToken)));

  EXPECT_TRUE(IsInvalidationsServiceStarted());
  EXPECT_EQ(0, test_url_loader_factory()->NumPending());

  // When the next policy fetch happens, it'll contain the same service account.
  // In this case, avoid starting the invalidations services again.
  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);
  EXPECT_EQ(0, test_url_loader_factory()->NumPending());

  EXPECT_EQ(1, num_invalidations_started());
}

TEST_F(CBCMInvalidationsInitializerTest,
       CanHandleServiceAccountChangedAfterFetchingInSameSession) {
  CBCMInvalidationsInitializer initializer(this);

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  // Simulate that a policy sets a service account and triggers a fetch.
  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);
  EXPECT_TRUE(IsInvalidationsServiceStarted());
  EXPECT_EQ(1, test_url_loader_factory()->NumPending());
  EXPECT_EQ(GaiaUrls::GetInstance()->oauth2_token_url().spec(),
            test_url_loader_factory()->GetPendingRequest(0)->request.url);
  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      MakeTokensFromAuthCodesResponse(kFirstRefreshToken, kFirstAccessToken)));

  EXPECT_EQ(0, test_url_loader_factory()->NumPending());
  EXPECT_TRUE(
      DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable());
  EXPECT_EQ(CoreAccountId::FromRobotEmail(kServiceAccountEmail),
            DeviceOAuth2TokenServiceFactory::Get()->GetRobotAccountId());
  std::string first_refresh_token =
      testing_local_state()->GetString(kCBCMServiceAccountRefreshToken);

  // Simulate that a policy comes in with a different service account. This
  // should trigger a re-initialization of the service account.
  initializer.OnServiceAccountSet(policy_client(), kOtherServiceAccountEmail);
  EXPECT_EQ(1, test_url_loader_factory()->NumPending());
  EXPECT_TRUE(IsInvalidationsServiceStarted());
  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      MakeTokensFromAuthCodesResponse(kSecondRefreshToken,
                                      kSecondAccessToken)));

  EXPECT_EQ(1, num_invalidations_started());
  EXPECT_TRUE(
      DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable());
  // Now a different refresh token and email should be present. The token
  // themselves aren't validated because they're encrypted. Verifying that it
  // changed is sufficient.
  EXPECT_EQ(CoreAccountId::FromRobotEmail(kOtherServiceAccountEmail),
            DeviceOAuth2TokenServiceFactory::Get()->GetRobotAccountId());
  EXPECT_NE(first_refresh_token,
            testing_local_state()->GetString(kCBCMServiceAccountRefreshToken));
}

TEST_F(CBCMInvalidationsInitializerTest,
       CanHandleServiceAccountChangedWhenAccountPresentOnStartup) {
  CBCMInvalidationsInitializer initializer(this);

  // Set up the token service as if there was already a service account set up
  // on start up.
  DeviceOAuth2TokenServiceFactory::Get()->SetServiceAccountEmail(
      kServiceAccountEmail);
  DeviceOAuth2TokenServiceFactory::Get()->SetAndSaveRefreshToken(
      kFirstRefreshToken,
      base::BindRepeating(&CBCMInvalidationsInitializerTest::
                              RefreshTokenSavedCallbackExpectSuccess,
                          base::Unretained(this)));

  EXPECT_EQ(1, num_refresh_tokens_saved());
  EXPECT_TRUE(
      DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable());
  std::string first_refresh_token =
      testing_local_state()->GetString(kCBCMServiceAccountRefreshToken);

  EXPECT_FALSE(IsInvalidationsServiceStarted());
  // On first policy store load, this will be called and invalidations started.
  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);
  EXPECT_EQ(0, test_url_loader_factory()->NumPending());
  EXPECT_TRUE(IsInvalidationsServiceStarted());
  // The same refresh token should be present in local state.
  EXPECT_EQ(first_refresh_token,
            testing_local_state()->GetString(kCBCMServiceAccountRefreshToken));

  // Simulate that a new policy is fetched with a different service account.
  // This should result in a gaia call for the service account initialization.
  initializer.OnServiceAccountSet(policy_client(), kOtherServiceAccountEmail);
  EXPECT_EQ(1, test_url_loader_factory()->NumPending());
  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      MakeTokensFromAuthCodesResponse(kSecondRefreshToken,
                                      kSecondAccessToken)));

  EXPECT_TRUE(IsInvalidationsServiceStarted());
  EXPECT_TRUE(
      DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable());
  // Now a different refresh token and email should be present. The token
  // themselves aren't validated because they're encrypted. Verifying that it
  // changed is sufficient.
  EXPECT_EQ(CoreAccountId::FromRobotEmail(kOtherServiceAccountEmail),
            DeviceOAuth2TokenServiceFactory::Get()->GetRobotAccountId());
  EXPECT_NE(first_refresh_token,
            testing_local_state()->GetString(kCBCMServiceAccountRefreshToken));
}

}  // namespace policy
