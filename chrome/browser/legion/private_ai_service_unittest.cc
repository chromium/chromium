// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/legion/private_ai_service.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/data_types.h"
#include "components/legion/phosphor/mock_blind_sign_auth.h"
#include "components/legion/phosphor/token_fetcher_helper.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace legion {

namespace {

constexpr char kTestEmail[] = "test@example.com";

class TestPrivateAiService : public PrivateAiService {
 public:
  TestPrivateAiService(signin::IdentityManager* identity_manager,
                       PrefService* pref_service,
                       Profile* profile)
      : PrivateAiService(identity_manager, pref_service, profile) {}

  ~TestPrivateAiService() override = default;

  // KeyedService override:
  void Shutdown() override { PrivateAiService::Shutdown(); }

  // phosphor::TokenFetcherImpl::Delegate override:
  std::unique_ptr<quiche::BlindSignAuthInterface> CreateBlindSignAuth(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory) override {
    auto bsa = std::make_unique<phosphor::MockBlindSignAuth>();
    bsa_ = bsa.get();
    return bsa;
  }

  phosphor::MockBlindSignAuth* bsa() { return bsa_; }

 private:
  raw_ptr<phosphor::MockBlindSignAuth> bsa_ = nullptr;
};

}  // namespace

class PrivateAiServiceTest : public testing::Test {
 protected:
  PrivateAiServiceTest()
      : private_ai_service_(identity_test_env_.identity_manager(),
                            profile_.GetPrefs(),
                            &profile_) {
    feature_list_.InitAndEnableFeature(legion::kLegion);
  }

  ~PrivateAiServiceTest() override { private_ai_service_.Shutdown(); }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;

  TestingProfile profile_;

  TestPrivateAiService private_ai_service_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PrivateAiServiceTest, RequestOAuthTokenSuccess) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSync);

  private_ai_service_.RequestOAuthToken(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  EXPECT_EQ(future.Get<0>(), phosphor::GetAuthnTokensResult::kSuccess);
  EXPECT_EQ(future.Get<1>(), "access_token");
}

TEST_F(PrivateAiServiceTest, RequestOAuthTokenNoAccount) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  private_ai_service_.RequestOAuthToken(future.GetCallback());
  EXPECT_EQ(future.Get<0>(), phosphor::GetAuthnTokensResult::kFailedNoAccount);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

TEST_F(PrivateAiServiceTest, RequestOAuthTokenTransientError) {
  base::test::TestFuture<phosphor::GetAuthnTokensResult,
                         std::optional<std::string>>
      future;
  identity_test_env_.MakePrimaryAccountAvailable(kTestEmail,
                                                 signin::ConsentLevel::kSync);

  private_ai_service_.RequestOAuthToken(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));

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

  private_ai_service_.RequestOAuthToken(future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(future.Get<0>(),
            phosphor::GetAuthnTokensResult::kFailedOAuthTokenPersistent);
  EXPECT_EQ(future.Get<1>(), std::nullopt);
}

}  // namespace legion
