// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_ai/private_ai_service.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/private_ai/test_private_ai_service.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/phosphor/token_fetcher_helper.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

constexpr char kTestEmail[] = "test@example.com";

}  // namespace

class PrivateAiServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        kPrivateAi, {{kPrivateAiApiKey.name, "test-api-key"}});
    auto test_bsa_factory = std::make_unique<TestBlindSignAuthFactory>();
    auto* test_bsa_factory_ptr = test_bsa_factory.get();
    private_ai_service_ = std::make_unique<TestPrivateAiService>(
        identity_test_env_.identity_manager(), profile_.GetPrefs(), &profile_,
        test_bsa_factory_ptr, std::move(test_bsa_factory));
  }

  void TearDown() override {
    private_ai_service_->Shutdown();
    private_ai_service_.reset();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;

  TestingProfile profile_;

 private:
  base::test::ScopedFeatureList feature_list_;

 protected:
  std::unique_ptr<TestPrivateAiService> private_ai_service_;
};

TEST_F(PrivateAiServiceTest, RequestOAuthTokenSuccess) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSync);

  private_ai_service_->RequestOAuthToken(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  EXPECT_EQ(future.Get<0>(), phosphor::GetAuthnTokensResult::kSuccess);
  EXPECT_EQ(future.Get<1>(), "access_token");
}

TEST_F(PrivateAiServiceTest, RequestOAuthTokenNoAccount) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  private_ai_service_->RequestOAuthToken(future.GetCallback());
  EXPECT_EQ(future.Get<0>(), phosphor::GetAuthnTokensResult::kFailedNoAccount);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(PrivateAiServiceTest, RequestOAuthTokenTransientError) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSync);

  private_ai_service_->RequestOAuthToken(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  EXPECT_EQ(future.Get<0>(),
            phosphor::GetAuthnTokensResult::kFailedOAuthTokenTransient);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(PrivateAiServiceTest, RequestOAuthTokenPersistentError) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSync);

  private_ai_service_->RequestOAuthToken(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  EXPECT_EQ(future.Get<0>(),
            phosphor::GetAuthnTokensResult::kFailedOAuthTokenPersistent);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

class PrivateAiServiceUtilTest : public testing::Test {};

TEST_F(PrivateAiServiceUtilTest, GetApiKey) {
  // If API key is set in feature params, it should be returned.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPrivateAi, {{"api-key", "provided-api-key"}}}}, {});
    EXPECT_EQ(PrivateAiService::GetApiKey(), "provided-api-key");
  }

  // If API key is not set, it should return default Chrome API key (if used) or
  // empty. This depends on the build configuration.
  {
    if (google_apis::IsGoogleChromeAPIKeyUsed()) {
      EXPECT_EQ(PrivateAiService::GetApiKey(),
                google_apis::GetAPIKey(chrome::GetChannel()));
    } else {
      EXPECT_EQ(PrivateAiService::GetApiKey(), "");
    }
  }
}

TEST_F(PrivateAiServiceUtilTest, CanPrivateAiBeEnabled) {
  // Enabled by default in features.cc, but needs API key.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPrivateAi, {{"api-key", "test-key"}}}}, {});
    EXPECT_TRUE(PrivateAiService::CanPrivateAiBeEnabled());
  }

  // Disabled if API key is empty AND not using default key.
  {
    // If google_apis::IsGoogleChromeAPIKeyUsed() is true, it will still be
    // enabled because GetApiKey() will return the default key. We need to
    // account for that.
    if (google_apis::IsGoogleChromeAPIKeyUsed()) {
      EXPECT_TRUE(PrivateAiService::CanPrivateAiBeEnabled());
    } else {
      EXPECT_FALSE(PrivateAiService::CanPrivateAiBeEnabled());
    }
  }
}

}  // namespace private_ai
