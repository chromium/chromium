// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_lacros.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

namespace reporting {
namespace {

// Test user id.
constexpr char kTestUserId[] = "123";

class WebsiteMetricsRetrieverLacrosTest : public ::testing::Test {
 protected:
  WebsiteMetricsRetrieverLacrosTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kTestUserId);

    // Configure app service proxy for the test.
    app_service_proxy_ =
        ::apps::AppServiceProxyFactory::GetForProfile(profile_);
    auto website_metrics_service =
        std::make_unique<::apps::WebsiteMetricsServiceLacros>(profile_);
    app_service_proxy_->SetWebsiteMetricsServiceForTesting(
        std::move(website_metrics_service));

    // Configure the website metrics retriever.
    website_metrics_retriever_ =
        std::make_unique<WebsiteMetricsRetrieverLacros>(profile_->GetWeakPtr());
  }

  ::content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<::apps::AppServiceProxy> app_service_proxy_;
  std::unique_ptr<WebsiteMetricsRetrieverLacros> website_metrics_retriever_;
};

TEST_F(WebsiteMetricsRetrieverLacrosTest,
       ObserveWebsiteMetricsInitWhenUninitialized) {
  // Verify `WebsiteMetrics` component is uninitialized for the given profile
  // initially.
  ::apps::WebsiteMetricsServiceLacros* const website_metrics_service =
      app_service_proxy_->WebsiteMetricsService();
  ASSERT_THAT(website_metrics_service->WebsiteMetrics(), IsNull());

  // Verify retriever starts observing component init when requested.
  ::apps::WebsiteMetrics* initialized_website_metrics = nullptr;
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics = website_metrics;
      }));
  ASSERT_TRUE(website_metrics_retriever_->IsObservingSourceForTest(
      website_metrics_service));

  // Simulate init and verify callback is triggered with initialized component.
  website_metrics_service->SetWebsiteMetricsForTesting(
      std::make_unique<::apps::WebsiteMetrics>(profile_,
                                               /*user_type_by_device_type=*/0));
  website_metrics_service->Start();
  ASSERT_THAT(initialized_website_metrics,
              Eq(website_metrics_service->WebsiteMetrics()));
  EXPECT_FALSE(website_metrics_retriever_->IsObservingSourceForTest(
      website_metrics_service));
}

TEST_F(WebsiteMetricsRetrieverLacrosTest, ReturnWebsiteMetricsIfInitialized) {
  // Simulate `WebsiteMetrics` component initialization before we test this
  // scenario.
  ::apps::WebsiteMetricsServiceLacros* const website_metrics_service =
      app_service_proxy_->WebsiteMetricsService();
  website_metrics_service->SetWebsiteMetricsForTesting(
      std::make_unique<::apps::WebsiteMetrics>(profile_,
                                               /*user_type_by_device_type=*/0));
  website_metrics_service->Start();

  // Verify retriever returns initialized component without observing init.
  ::apps::WebsiteMetrics* initialized_website_metrics = nullptr;
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics = website_metrics;
      }));
  ASSERT_FALSE(website_metrics_retriever_->IsObservingSourceForTest(
      website_metrics_service));
  EXPECT_THAT(initialized_website_metrics,
              Eq(website_metrics_service->WebsiteMetrics()));
}

TEST_F(WebsiteMetricsRetrieverLacrosTest,
       ChainCallbacksWithMultipleRequestsDuringInitObservation) {
  // Verify `WebsiteMetrics` component is uninitialized for the given profile
  // initially.
  ::apps::WebsiteMetricsServiceLacros* const website_metrics_service =
      app_service_proxy_->WebsiteMetricsService();
  ASSERT_THAT(website_metrics_service->WebsiteMetrics(), IsNull());

  // Trigger two requests for the component while it is not yet initialized
  // and verify the retriever starts observing init.
  ::apps::WebsiteMetrics* initialized_website_metrics_1 = nullptr;
  ::apps::WebsiteMetrics* initialized_website_metrics_2 = nullptr;
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics_1 = website_metrics;
      }));
  ASSERT_TRUE(website_metrics_retriever_->IsObservingSourceForTest(
      website_metrics_service));
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics_2 = website_metrics;
      }));
  ASSERT_TRUE(website_metrics_retriever_->IsObservingSourceForTest(
      website_metrics_service));

  // Simulate init and verify both callbacks are triggered with the initialized
  // component.
  website_metrics_service->SetWebsiteMetricsForTesting(
      std::make_unique<::apps::WebsiteMetrics>(profile_,
                                               /*user_type_by_device_type=*/0));
  website_metrics_service->Start();
  ASSERT_THAT(initialized_website_metrics_1,
              Eq(website_metrics_service->WebsiteMetrics()));
  ASSERT_THAT(initialized_website_metrics_2,
              Eq(website_metrics_service->WebsiteMetrics()));
  EXPECT_FALSE(website_metrics_retriever_->IsObservingSourceForTest(
      website_metrics_service));
}

TEST_F(WebsiteMetricsRetrieverLacrosTest,
       AppPlatformMetricsServiceDestructedDuringObservation) {
  // Verify `WebsiteMetrics` component is uninitialized for the given profile
  // initially.
  ::apps::WebsiteMetricsServiceLacros* const website_metrics_service =
      app_service_proxy_->WebsiteMetricsService();
  ASSERT_THAT(website_metrics_service->WebsiteMetrics(), IsNull());

  // Verify retriever starts observing component init when requested.
  ::apps::WebsiteMetrics* initialized_website_metrics = nullptr;
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics = website_metrics;
      }));
  ASSERT_TRUE(website_metrics_retriever_->IsObservingSourceForTest(
      website_metrics_service));

  // Simulate destruction and verify callback is triggered.
  app_service_proxy_->SetWebsiteMetricsServiceForTesting(
      std::unique_ptr<::apps::WebsiteMetricsServiceLacros>(nullptr));
  ASSERT_THAT(initialized_website_metrics, IsNull());
}

}  // namespace
}  // namespace reporting
