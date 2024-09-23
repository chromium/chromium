// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/platform_auth/mock_platform_auth_provider.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

namespace enterprise_auth {

void EnableManager(PlatformAuthProviderManager& manager, bool enabled) {
  base::MockCallback<base::OnceClosure> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run()).WillOnce([&run_loop]() {
    run_loop.QuitWhenIdle();
  });
  manager.SetEnabled(enabled, callback.Get());
  EXPECT_EQ(manager.IsEnabled(), enabled);
  run_loop.Run();
  EXPECT_EQ(manager.IsEnabled(), enabled);
}

class PlatformAuthProviderManagerTest : public ::testing::Test {
 protected:
  PlatformAuthProviderManagerTest() : mock_provider_(owned_provider_.get()) {
    // Set up a default that will clear the raw pointer upon destruction.
    ON_CALL(*mock_provider_, Die()).WillByDefault([this]() {
      this->mock_provider_ = nullptr;
    });

    // Expect the provider to be destroyed at some point.
    EXPECT_CALL(*mock_provider_, Die());
  }

  void SetUp() override {
    EXPECT_CALL(*mock_provider(), SupportsOriginFiltering())
        .WillOnce(::testing::Return(true));
  }

  std::unique_ptr<PlatformAuthProvider> TakeProvider() {
    return std::move(owned_provider_);
  }

  ::testing::StrictMock<MockPlatformAuthProvider>* mock_provider() {
    return mock_provider_;
  }

  std::unique_ptr<::testing::StrictMock<MockPlatformAuthProvider>>
      owned_provider_{
          std::make_unique<::testing::StrictMock<MockPlatformAuthProvider>>()};
  raw_ptr<::testing::StrictMock<MockPlatformAuthProvider>> mock_provider_ =
      nullptr;
  base::test::TaskEnvironment task_environment_;
};

// Tests that the manager is disabled by default.
TEST_F(PlatformAuthProviderManagerTest, DefaultDisabled) {
  PlatformAuthProviderManager manager(TakeProvider());

  EXPECT_FALSE(manager.IsEnabled());
  EXPECT_EQ(manager.GetOriginsForTesting(), std::vector<url::Origin>());
}

// Tests that disabling when already disabled does nothing.
TEST_F(PlatformAuthProviderManagerTest, DisableNoop) {
  PlatformAuthProviderManager manager(TakeProvider());

  EnableManager(manager, false);
  EXPECT_FALSE(manager.IsEnabled());
  EXPECT_EQ(manager.GetOriginsForTesting(), std::vector<url::Origin>());
}

// Tests that enabling queries the provider and handles null. A second
// enablement does not query again.
TEST_F(PlatformAuthProviderManagerTest, NotSupported) {
  PlatformAuthProviderManager manager(TakeProvider());

  EXPECT_CALL(*mock_provider(), FetchOrigins(_))
      .WillOnce([](PlatformAuthProvider::FetchOriginsCallback callback) {
        std::move(callback).Run(nullptr);
      });
  EnableManager(manager, true);
  EXPECT_EQ(mock_provider(), nullptr);
  EXPECT_EQ(manager.GetOriginsForTesting(), std::vector<url::Origin>());
  EnableManager(manager, true);
}

// Tests that enabling queries the provider and handles an empty set of origins.
// A second enablement repeats the query.
TEST_F(PlatformAuthProviderManagerTest, SupportedWithEmptyOrigins) {
  PlatformAuthProviderManager manager(TakeProvider());

  EXPECT_CALL(*mock_provider(), FetchOrigins(_))
      .Times(2)
      .WillRepeatedly([](PlatformAuthProvider::FetchOriginsCallback callback) {
        std::move(callback).Run(std::make_unique<std::vector<url::Origin>>());
      });
  EnableManager(manager, true);
  EXPECT_NE(mock_provider(), nullptr);
  EXPECT_EQ(manager.GetOriginsForTesting(), std::vector<url::Origin>());
  EnableManager(manager, true);
  EXPECT_NE(mock_provider(), nullptr);
  EXPECT_EQ(manager.GetOriginsForTesting(), std::vector<url::Origin>());
}

// Tests that enabling queries the provider and handles non-empty sets of
// origins. A second enablement repeats the query, which then returns no
// origins.
TEST_F(PlatformAuthProviderManagerTest, OriginRemoval) {
  ::testing::Sequence sequence;

  PlatformAuthProviderManager manager(TakeProvider());

  EXPECT_CALL(*mock_provider(), FetchOrigins(_))
      .InSequence(sequence)
      .WillOnce([](PlatformAuthProvider::FetchOriginsCallback callback) {
        std::move(callback).Run(
            std::make_unique<std::vector<url::Origin>>(std::vector<url::Origin>{
                url::Origin::Create(GURL("https://org"))}));
      });
  EXPECT_CALL(*mock_provider(), FetchOrigins(_))
      .InSequence(sequence)
      .WillOnce([](PlatformAuthProvider::FetchOriginsCallback callback) {
        std::move(callback).Run(std::make_unique<std::vector<url::Origin>>());
      });
  EnableManager(manager, true);
  EXPECT_NE(mock_provider(), nullptr);
  EXPECT_EQ(
      manager.GetOriginsForTesting(),
      std::vector<url::Origin>({url::Origin::Create(GURL("https://org"))}));
  EnableManager(manager, true);
  EXPECT_NE(mock_provider(), nullptr);
  EXPECT_EQ(manager.GetOriginsForTesting(), std::vector<url::Origin>());
}

class PlatformAuthProviderManagerNoOriginFilteringTest
    : public PlatformAuthProviderManagerTest {
 protected:
  void SetUp() override {
    EXPECT_CALL(*mock_provider(), SupportsOriginFiltering())
        .WillOnce(::testing::Return(false));
  }
};

// Tests that enabling queries the provider and handles non-empty sets of
// origins. A second enablement repeats the query, which then returns no
// origins.
TEST_F(PlatformAuthProviderManagerNoOriginFilteringTest,
       OriginFilteringNotSupported) {
  EXPECT_CALL(*mock_provider(), FetchOrigins(_)).Times(0);

  PlatformAuthProviderManager manager(TakeProvider());
  EnableManager(manager, true);
  EXPECT_NE(mock_provider(), nullptr);
  EnableManager(manager, false);
  EXPECT_NE(mock_provider(), nullptr);
  EnableManager(manager, true);
  EXPECT_NE(mock_provider(), nullptr);
}

// Verifies that the expected metrics are recorded on a cookie fetch.
TEST(PlatformAuthProviderManagerMetricsTest, Success) {
  const char kOldCookie[] = "old-cookie=old-cookie-data";
  base::test::TaskEnvironment task_environment;
  PlatformAuthProviderManager manager;

  EnableManager(manager, true);
  const auto origins = manager.GetOriginsForTesting();
  if (origins.empty())
    return;  // Nothing to test if there are no IdP/STS origins.

  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kCookie, kOldCookie);
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  base::MockCallback<PlatformAuthProviderManager::GetDataCallback> mock;

  EXPECT_CALL(mock, Run(_))
      .WillOnce([&run_loop, &headers](net::HttpRequestHeaders auth_headers) {
        headers = std::move(auth_headers);
        run_loop.QuitWhenIdle();
      });
  manager.GetData(origins.begin()->GetURL(), mock.Get());
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(&mock);

  ASSERT_TRUE(headers.HasHeader(net::HttpRequestHeaders::kCookie));
  std::string new_cookie = headers.GetHeader(net::HttpRequestHeaders::kCookie)
                               .value_or(std::string());

  if (histogram_tester
          .GetAllSamples("Enterprise.PlatformAuth.GetAuthData.FailureHresult")
          .empty()) {
    EXPECT_NE(kOldCookie, new_cookie);
    // There should be a hit in the count histogram.
    histogram_tester.ExpectTotalCount(
        "Enterprise.PlatformAuth.GetAuthData.Count", 1);
    histogram_tester.ExpectTotalCount(
        "Enterprise.PlatformAuth.GetAuthData.SuccessTime", 1);
  } else {
    EXPECT_EQ(kOldCookie, new_cookie);
    histogram_tester.ExpectTotalCount(
        "Enterprise.PlatformAuth.GetAuthData.FailureHresult", 1);
    histogram_tester.ExpectTotalCount(
        "Enterprise.PlatformAuth.GetAuthData.FailureTime", 1);
  }
}

}  // namespace enterprise_auth
