// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_client_side_sampling_status_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/aw_features.h"
#include "base/test/scoped_feature_list.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace android_webview {

using InstallerPackageType =
    metrics::AndroidMetricsServiceClient::InstallerPackageType;

namespace {

class AwMetricsServiceClientTestDelegate
    : public AwMetricsServiceClient::Delegate {
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override {}
  void AddWebViewAppStateObserver(WebViewAppStateObserver* observer) override {}
  bool HasAwContentsEverCreated() const override { return false; }
};

}  // namespace

TEST(AwClientSideSamplingStatusMetricsProviderTest, TestSamplingApplied) {
  base::test::ScopedFeatureList scoped_list;
  AwMetricsServiceClient client(
      std::make_unique<AwMetricsServiceClientTestDelegate>());
  AwClientSideSamplingStatusMetricsProvider test_provider(&client);
  metrics::ChromeUserMetricsExtension uma_proto;

  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewServerSideSampling);
  test_provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());
  EXPECT_TRUE(
      uma_proto.mutable_system_profile()->has_client_side_sampling_status());
  EXPECT_TRUE(
      uma_proto.mutable_system_profile()->client_side_sampling_status() ==
      metrics::SystemProfileProto::SAMPLING_NOT_APPLIED);
}

TEST(AwClientSideSamplingStatusMetricsProviderTest, TestSamplingNotApplied) {
  base::test::ScopedFeatureList scoped_list;
  AwMetricsServiceClient client(
      std::make_unique<AwMetricsServiceClientTestDelegate>());
  AwClientSideSamplingStatusMetricsProvider test_provider(&client);
  metrics::ChromeUserMetricsExtension uma_proto;

  scoped_list.Init();
  test_provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());
  EXPECT_TRUE(
      uma_proto.mutable_system_profile()->has_client_side_sampling_status());
  EXPECT_TRUE(
      uma_proto.mutable_system_profile()->client_side_sampling_status() ==
      metrics::SystemProfileProto::SAMPLING_APPLIED);
}

}  // namespace android_webview
