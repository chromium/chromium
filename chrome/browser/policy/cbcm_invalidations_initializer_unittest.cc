// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cbcm_invalidations_initializer.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/features.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

static const char kRefreshToken[] = "refresh_token";
static const char kServiceAccountEmail[] = "service_account@example.com";
static const char kOtherServiceAccountEmail[] =
    "other_service_account@example.com";
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

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  std::string MakeTokensFromAuthCodesResponse() {
    base::DictionaryValue dict;
    dict.SetString("access_token", "access_token");
    dict.SetString("refresh_token", kRefreshToken);
    dict.SetInteger("expires_in", 9999);

    std::string json;
    base::JSONWriter::Write(dict, &json);
    return json;
  }

 private:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kCBCMPolicyInvalidations, features::kCBCMRemoteCommands},
        {});

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
  base::test::ScopedFeatureList feature_list_;
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
      kRefreshToken,
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

TEST_F(CBCMInvalidationsInitializerTest, InvalidationsStartAfterTokenFetched) {
  CBCMInvalidationsInitializer initializer(this);

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);
  EXPECT_EQ(1, test_url_loader_factory()->NumPending());
  EXPECT_EQ(GaiaUrls::GetInstance()->oauth2_token_url().spec(),
            test_url_loader_factory()->GetPendingRequest(0)->request.url);

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      MakeTokensFromAuthCodesResponse()));

  EXPECT_TRUE(IsInvalidationsServiceStarted());
}

TEST_F(CBCMInvalidationsInitializerTest,
       InvalidationsDontRestartOnNextPolicyFetch) {
  CBCMInvalidationsInitializer initializer(this);

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);

  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      MakeTokensFromAuthCodesResponse()));

  EXPECT_TRUE(IsInvalidationsServiceStarted());
  EXPECT_EQ(0, test_url_loader_factory()->NumPending());

  // When the next policy fetch happens, it'll contain the same service account.
  // In this case, avoid starting the invalidations services again.
  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);
  EXPECT_EQ(0, test_url_loader_factory()->NumPending());

  EXPECT_EQ(1, num_invalidations_started());
}

TEST_F(CBCMInvalidationsInitializerTest,
       InvalidationsDontStartTwiceWhenTokenFetchRaces) {
  CBCMInvalidationsInitializer initializer(this);

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  initializer.OnServiceAccountSet(policy_client(), kServiceAccountEmail);
  EXPECT_EQ(1, test_url_loader_factory()->NumPending());
  EXPECT_EQ(GaiaUrls::GetInstance()->oauth2_token_url().spec(),
            test_url_loader_factory()->GetPendingRequest(0)->request.url);

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  // Trying to set the service account again when a request is already pending
  // cancels the old request and starts a new one
  initializer.OnServiceAccountSet(policy_client(), kOtherServiceAccountEmail);
  EXPECT_EQ(1, test_url_loader_factory()->NumPending());

  EXPECT_FALSE(IsInvalidationsServiceStarted());

  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      MakeTokensFromAuthCodesResponse()));

  EXPECT_EQ(1, num_invalidations_started());
  EXPECT_EQ(CoreAccountId::FromEmail(kOtherServiceAccountEmail),
            DeviceOAuth2TokenServiceFactory::Get()->GetRobotAccountId());
}

}  // namespace policy
