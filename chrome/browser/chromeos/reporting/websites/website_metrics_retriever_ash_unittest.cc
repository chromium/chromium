// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_ash.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

namespace reporting {
namespace {

class WebsiteMetricsRetrieverAshTest
    : public ::apps::AppPlatformMetricsServiceTestBase {
 protected:
  void SetUp() override {
    // Set up test user and profile.
    AddRegularUser(/*email=*/"test@test.com");

    // Configure app service proxy for the test.
    auto app_platform_metrics_service =
        std::make_unique<::apps::AppPlatformMetricsService>(profile());
    app_service_proxy_ =
        ::apps::AppServiceProxyFactory::GetForProfile(profile());
    app_service_proxy_->SetAppPlatformMetricsServiceForTesting(
        std::move(app_platform_metrics_service));

    // Configure the website metrics retriever.
    website_metrics_retriever_ =
        std::make_unique<WebsiteMetricsRetrieverAsh>(profile()->GetWeakPtr());
  }

  raw_ptr<::apps::AppServiceProxy> app_service_proxy_;
  std::unique_ptr<WebsiteMetricsRetrieverAsh> website_metrics_retriever_;
};

TEST_F(WebsiteMetricsRetrieverAshTest,
       ObserveWebsiteMetricsInitWhenUninitialized) {
  // Verify `WebsiteMetrics` component is uninitialized for the given profile
  // initially.
  auto* const app_platform_metrics_service =
      app_service_proxy_->AppPlatformMetricsService();
  ASSERT_THAT(app_platform_metrics_service->WebsiteMetrics(), IsNull());

  // Verify retriever starts observing component init when requested.
  ::apps::WebsiteMetrics* initialized_website_metrics = nullptr;
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics = website_metrics;
      }));
  ASSERT_TRUE(website_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service));

  // Simulate init and verify callback is triggered with initialized component.
  app_platform_metrics_service->Start(
      app_service_proxy_->AppRegistryCache(),
      app_service_proxy_->InstanceRegistry(),
      app_service_proxy_->AppCapabilityAccessCache());
  ASSERT_THAT(initialized_website_metrics,
              Eq(app_platform_metrics_service->WebsiteMetrics()));
  EXPECT_FALSE(website_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service));
}

TEST_F(WebsiteMetricsRetrieverAshTest, ReturnWebsiteMetricsIfInitialized) {
  // Simulate `WebsiteMetrics` component initialization before we test this
  // scenario.
  auto* const app_platform_metrics_service =
      app_service_proxy_->AppPlatformMetricsService();
  app_platform_metrics_service->Start(
      app_service_proxy_->AppRegistryCache(),
      app_service_proxy_->InstanceRegistry(),
      app_service_proxy_->AppCapabilityAccessCache());
  ASSERT_THAT(app_platform_metrics_service->WebsiteMetrics(), NotNull());

  // Verify retriever returns initialized component without observing init.
  ::apps::WebsiteMetrics* initialized_website_metrics = nullptr;
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics = website_metrics;
      }));
  ASSERT_FALSE(website_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service));
  EXPECT_THAT(initialized_website_metrics,
              Eq(app_platform_metrics_service->WebsiteMetrics()));
}

TEST_F(WebsiteMetricsRetrieverAshTest,
       ChainCallbacksWithMultipleRequestsDuringInitObservation) {
  // Verify `WebsiteMetrics` component is uninitialized for the given profile
  // initially.
  auto* const app_platform_metrics_service =
      app_service_proxy_->AppPlatformMetricsService();
  ASSERT_THAT(app_platform_metrics_service->WebsiteMetrics(), IsNull());

  // Trigger two requests for the component while it is not yet initialized
  // and verify the retriever starts observing init.
  ::apps::WebsiteMetrics* initialized_website_metrics_1 = nullptr;
  ::apps::WebsiteMetrics* initialized_website_metrics_2 = nullptr;
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics_1 = website_metrics;
      }));
  ASSERT_TRUE(website_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service));
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics_2 = website_metrics;
      }));
  ASSERT_TRUE(website_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service));

  // Simulate init and verify both callbacks are triggered with initialized
  // component.
  app_platform_metrics_service->Start(
      app_service_proxy_->AppRegistryCache(),
      app_service_proxy_->InstanceRegistry(),
      app_service_proxy_->AppCapabilityAccessCache());
  ASSERT_THAT(initialized_website_metrics_1,
              Eq(app_platform_metrics_service->WebsiteMetrics()));
  ASSERT_THAT(initialized_website_metrics_2,
              Eq(app_platform_metrics_service->WebsiteMetrics()));
  EXPECT_FALSE(website_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service));
}

TEST_F(WebsiteMetricsRetrieverAshTest,
       AppPlatformMetricsServiceDestructedDuringObservation) {
  // Verify `WebsiteMetrics` component is uninitialized for the given profile
  // initially.
  auto* const app_platform_metrics_service =
      app_service_proxy_->AppPlatformMetricsService();
  ASSERT_THAT(app_platform_metrics_service->WebsiteMetrics(), IsNull());

  // Verify retriever starts observing component init when requested.
  ::apps::WebsiteMetrics* initialized_website_metrics = nullptr;
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindLambdaForTesting([&](::apps::WebsiteMetrics* website_metrics) {
        initialized_website_metrics = website_metrics;
      }));
  ASSERT_TRUE(website_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service));

  // Simulate destruction and verify callback is triggered.
  app_service_proxy_->SetAppPlatformMetricsServiceForTesting(
      std::unique_ptr<::apps::AppPlatformMetricsService>(nullptr));
  ASSERT_THAT(initialized_website_metrics, IsNull());
}

}  // namespace
}  // namespace reporting
