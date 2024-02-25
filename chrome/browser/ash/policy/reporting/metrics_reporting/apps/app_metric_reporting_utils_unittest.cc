// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_metric_reporting_utils.h"

#include <optional>
#include <string>

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kTestAppId[] = "TestApp";
constexpr char kTestAppPublisherId[] = "com.google.test";

class AppMetricReportingUtilsTest
    : public ::apps::AppPlatformMetricsServiceTestBase {};

TEST_F(AppMetricReportingUtilsTest, AppWithNoPublisherId) {
  InstallOneApp(kTestAppId, ::apps::AppType::kChromeApp, /*publisher_id=*/"",
                ::apps::Readiness::kReady,
                ::apps::InstallSource::kChromeWebStore);
  const std::optional<std::string> publisher_id =
      GetPublisherIdForApp(kTestAppId, profile());
  EXPECT_FALSE(publisher_id.has_value());
}

TEST_F(AppMetricReportingUtilsTest, AppWithPublisherId) {
  InstallOneApp(kTestAppId, ::apps::AppType::kArc, kTestAppPublisherId,
                ::apps::Readiness::kReady, ::apps::InstallSource::kPlayStore);
  const std::optional<std::string> publisher_id =
      GetPublisherIdForApp(kTestAppId, profile());
  ASSERT_TRUE(publisher_id.has_value());
  EXPECT_THAT(publisher_id.value(), StrEq(kTestAppPublisherId));
}

TEST_F(AppMetricReportingUtilsTest, UnregisteredApp) {
  const std::optional<std::string> publisher_id =
      GetPublisherIdForApp(kTestAppId, profile());
  EXPECT_FALSE(publisher_id.has_value());
}

}  // namespace
}  // namespace reporting
