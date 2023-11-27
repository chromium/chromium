// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_server_side_allowlist_metrics_provider.h"

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/aw_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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

class TestClient : public AwMetricsServiceClient {
 public:
  TestClient()
      : AwMetricsServiceClient(
            std::make_unique<AwMetricsServiceClientTestDelegate>()) {}
  ~TestClient() override = default;

  InstallerPackageType GetInstallerPackageType() override {
    return installer_type_;
  }

  void SetInstallerPackageType(InstallerPackageType installer_type) {
    installer_type_ = installer_type;
  }

 private:
  InstallerPackageType installer_type_;
};

class AwServerSideAllowlistMetricsProviderTest : public testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

}  // namespace

}  // namespace android_webview

namespace android_webview {

TEST_F(AwServerSideAllowlistMetricsProviderTest,
       TestServerSideAllowlist_TestServerSideFilteringRequired) {
  TestClient client;
  AwServerSideAllowlistMetricsProvider test_provider(&client);
  metrics::ChromeUserMetricsExtension uma_proto;

  client.SetInstallerPackageType(InstallerPackageType::GOOGLE_PLAY_STORE);
  test_provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());
  EXPECT_TRUE(uma_proto.mutable_system_profile()
                  ->has_app_package_name_allowlist_filter());
  EXPECT_TRUE(
      uma_proto.mutable_system_profile()->app_package_name_allowlist_filter() ==
      metrics::SystemProfileProto::SERVER_SIDE_FILTER_REQUIRED);
}

TEST_F(AwServerSideAllowlistMetricsProviderTest,
       TestServerSideAllowlist_TestNoServerSideFilteringForSystemApp) {
  TestClient client;
  AwServerSideAllowlistMetricsProvider test_provider(&client);
  metrics::ChromeUserMetricsExtension uma_proto;

  client.SetInstallerPackageType(InstallerPackageType::SYSTEM_APP);
  test_provider.ProvideSystemProfileMetrics(uma_proto.mutable_system_profile());
  EXPECT_TRUE(uma_proto.mutable_system_profile()
                  ->has_app_package_name_allowlist_filter());
  EXPECT_TRUE(
      uma_proto.mutable_system_profile()->app_package_name_allowlist_filter() ==
      metrics::SystemProfileProto::
          NO_SERVER_SIDE_FILTER_REQUIRED_FOR_SYSTEM_APPS);
}

}  // namespace android_webview
