// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_filtering_status_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/aw_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace android_webview {

namespace {

class AwMetricsServiceClientTestDelegate
    : public AwMetricsServiceClient::Delegate {
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override {}
  void AddWebViewAppStateObserver(WebViewAppStateObserver* observer) override {}
  bool HasAwContentsEverCreated() const override { return false; }
};

class TestClient : public AwMetricsServiceClient {
 public:
  TestClient()
      : AwMetricsServiceClient(
            std::make_unique<AwMetricsServiceClientTestDelegate>()) {}
  ~TestClient() override = default;

  bool ShouldApplyMetricsFiltering() const override {
    return should_apply_histogram_filtering_;
  }

  void SetShouldApplyMetricsFiltering(bool should_apply_histogram_filtering) {
    should_apply_histogram_filtering_ = should_apply_histogram_filtering;
  }

 private:
  bool should_apply_histogram_filtering_ = false;
};

}  // namespace

TEST(AwMetricsFilteringStatusMetricsProviderTest, TestAllMetricsAnnotation) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestClient client;
  client.SetShouldApplyMetricsFiltering(false);
  AwMetricsFilteringStatusMetricsProvider test_provider(&client);
  metrics::ChromeUserMetricsExtension uma_proto;

  test_provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());
  EXPECT_TRUE(
      uma_proto.mutable_system_profile()->has_metrics_filtering_status());
  EXPECT_EQ(uma_proto.mutable_system_profile()->metrics_filtering_status(),
            metrics::SystemProfileProto::METRICS_ALL);
}

TEST(AwMetricsFilteringStatusMetricsProviderTest,
     TestOnlyCriticalMetricsAnnotation) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestClient client;
  client.SetShouldApplyMetricsFiltering(true);
  AwMetricsFilteringStatusMetricsProvider test_provider(&client);
  metrics::ChromeUserMetricsExtension uma_proto;

  test_provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());
  EXPECT_TRUE(
      uma_proto.mutable_system_profile()->has_metrics_filtering_status());
  EXPECT_EQ(uma_proto.mutable_system_profile()->metrics_filtering_status(),
            metrics::SystemProfileProto::METRICS_ONLY_CRITICAL);
}

}  // namespace android_webview
