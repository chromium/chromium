// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

namespace reporting {
namespace {

class AppPlatformMetricsRetrieverTest
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
    app_platform_metrics_service_ =
        app_service_proxy_->AppPlatformMetricsService();

    // Configure the app platform metrics retriever.
    app_platform_metrics_retriever_ =
        std::make_unique<AppPlatformMetricsRetriever>(profile()->GetWeakPtr());
  }

  // Pointer to the `AppServiceProxy` used by the test.
  raw_ptr<::apps::AppServiceProxy> app_service_proxy_;

  // Pointer to the `AppPlatformMetricsService` used by the test.
  raw_ptr<::apps::AppPlatformMetricsService, DanglingUntriaged>
      app_platform_metrics_service_;

  // `AppPlatformMetrics` retriever used by the test.
  std::unique_ptr<AppPlatformMetricsRetriever> app_platform_metrics_retriever_;
};

TEST_F(AppPlatformMetricsRetrieverTest,
       ObserveAppPlatformMetricsInitWhenUninitialized) {
  // Verify `AppPlatformMetrics` component not initialized for the given profile
  // initially.
  ASSERT_THAT(app_platform_metrics_service_->AppPlatformMetrics(), IsNull());

  // Verify retriever starts observing component init when requested.
  ::apps::AppPlatformMetrics* initialized_app_platform_metrics = nullptr;
  app_platform_metrics_retriever_->GetAppPlatformMetrics(
      base::BindLambdaForTesting(
          [&](::apps::AppPlatformMetrics* app_platform_metrics) {
            initialized_app_platform_metrics = app_platform_metrics;
          }));
  ASSERT_TRUE(app_platform_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service_));

  // Simulate init and verify callback is triggered with initialized component.
  app_platform_metrics_service_->Start(
      app_service_proxy_->AppRegistryCache(),
      app_service_proxy_->InstanceRegistry(),
      app_service_proxy_->AppCapabilityAccessCache());
  ASSERT_THAT(initialized_app_platform_metrics,
              Eq(app_platform_metrics_service_->AppPlatformMetrics()));
  EXPECT_FALSE(app_platform_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service_));
}

TEST_F(AppPlatformMetricsRetrieverTest, ReturnAppPlatformMetricsIfInitialized) {
  // Simulate `AppPlatformMetrics` component initialization before we test this
  // scenario.
  app_platform_metrics_service_->Start(
      app_service_proxy_->AppRegistryCache(),
      app_service_proxy_->InstanceRegistry(),
      app_service_proxy_->AppCapabilityAccessCache());
  ASSERT_THAT(app_platform_metrics_service_->AppPlatformMetrics(), NotNull());

  // Verify retriever returns initialized component without observing init.
  ::apps::AppPlatformMetrics* initialized_app_platform_metrics = nullptr;
  app_platform_metrics_retriever_->GetAppPlatformMetrics(
      base::BindLambdaForTesting(
          [&](::apps::AppPlatformMetrics* app_platform_metrics) {
            initialized_app_platform_metrics = app_platform_metrics;
          }));
  ASSERT_FALSE(app_platform_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service_));
  EXPECT_THAT(initialized_app_platform_metrics,
              Eq(app_platform_metrics_service_->AppPlatformMetrics()));
}

TEST_F(AppPlatformMetricsRetrieverTest,
       ChainCallbacksWithMultipleRequestsDuringInitObservation) {
  // Verify `AppPlatformMetrics` component not initialized for the given
  // profile.
  ASSERT_THAT(app_platform_metrics_service_->AppPlatformMetrics(), IsNull());

  // Trigger two requests for the component while it is not yet initialized
  // and verify the retriever starts observing init.
  ::apps::AppPlatformMetrics* initialized_app_platform_metrics_1 = nullptr;
  ::apps::AppPlatformMetrics* initialized_app_platform_metrics_2 = nullptr;
  app_platform_metrics_retriever_->GetAppPlatformMetrics(
      base::BindLambdaForTesting(
          [&](::apps::AppPlatformMetrics* app_platform_metrics) {
            initialized_app_platform_metrics_1 = app_platform_metrics;
          }));
  ASSERT_TRUE(app_platform_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service_));
  app_platform_metrics_retriever_->GetAppPlatformMetrics(
      base::BindLambdaForTesting(
          [&](::apps::AppPlatformMetrics* app_platform_metrics) {
            initialized_app_platform_metrics_2 = app_platform_metrics;
          }));
  ASSERT_TRUE(app_platform_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service_));

  // Simulate init and verify both callbacks are triggered with initialized
  // component.
  app_platform_metrics_service_->Start(
      app_service_proxy_->AppRegistryCache(),
      app_service_proxy_->InstanceRegistry(),
      app_service_proxy_->AppCapabilityAccessCache());
  ASSERT_THAT(initialized_app_platform_metrics_1,
              Eq(app_platform_metrics_service_->AppPlatformMetrics()));
  ASSERT_THAT(initialized_app_platform_metrics_2,
              Eq(app_platform_metrics_service_->AppPlatformMetrics()));
  EXPECT_FALSE(app_platform_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service_));
}

TEST_F(AppPlatformMetricsRetrieverTest,
       AppPlatformMetricsServiceDestructedDuringObservation) {
  // Verify `AppPlatformMetrics` component not initialized for the given profile
  // initially.
  ASSERT_THAT(app_platform_metrics_service_->AppPlatformMetrics(), IsNull());

  // Verify retriever starts observing component init when requested.
  ::apps::AppPlatformMetrics* initialized_app_platform_metrics = nullptr;
  app_platform_metrics_retriever_->GetAppPlatformMetrics(
      base::BindLambdaForTesting(
          [&](::apps::AppPlatformMetrics* app_platform_metrics) {
            initialized_app_platform_metrics = app_platform_metrics;
          }));
  ASSERT_TRUE(app_platform_metrics_retriever_->IsObservingSourceForTest(
      app_platform_metrics_service_));

  // Simulate destruction and verify callback is triggered.
  app_service_proxy_->SetAppPlatformMetricsServiceForTesting(
      std::unique_ptr<::apps::AppPlatformMetricsService>(nullptr));
  ASSERT_THAT(initialized_app_platform_metrics, IsNull());
}

}  // namespace
}  // namespace reporting
