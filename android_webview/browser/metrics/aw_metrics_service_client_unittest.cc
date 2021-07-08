// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <memory>

#include "android_webview/common/aw_features.h"
#include "base/metrics/user_metrics.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {

class AwMetricsServiceClientTestDelegate
    : public AwMetricsServiceClient::Delegate {
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override {}
  void AddWebViewAppStateObserver(WebViewAppStateObserver* observer) override {}
  bool HasAwContentsEverCreated() const override { return false; }
};

class AwMetricsServiceClientTest : public testing::Test {
  AwMetricsServiceClientTest& operator=(const AwMetricsServiceClientTest&) =
      delete;
  AwMetricsServiceClientTest(AwMetricsServiceClientTest&&) = delete;
  AwMetricsServiceClientTest& operator=(AwMetricsServiceClientTest&&) = delete;

 protected:
  AwMetricsServiceClientTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        prefs_(std::make_unique<TestingPrefServiceSimple>()),
        client_(std::make_unique<AwMetricsServiceClient>(
            std::make_unique<AwMetricsServiceClientTestDelegate>())) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
    AwMetricsServiceClient::RegisterMetricsPrefs(prefs_->registry());
    client_->Initialize(prefs_.get());
  }

  AwMetricsServiceClient* GetClient() { return client_.get(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<AwMetricsServiceClient> client_;
};

}  // namespace

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName_CacheNotSet) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  EXPECT_FALSE(GetClient()->ShouldRecordPackageName());
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldRecordPackageName_TestExpiredResult) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  auto* client = GetClient();
  auto one_day_ago = base::Time::Now() - base::TimeDelta::FromDays(1);
  client->SetShouldRecordPackageName(/* expiry_date= */ one_day_ago);
  EXPECT_FALSE(client->ShouldRecordPackageName());
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldRecordPackageName_TestValidResult) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  auto* client = GetClient();
  auto one_day_from_now = base::Time::Now() + base::TimeDelta::FromDays(1);
  client->SetShouldRecordPackageName(/* expiry_date= */ one_day_from_now);
  EXPECT_TRUE(client->ShouldRecordPackageName());
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldRecordPackageName_TestInvalidResult) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewAppsPackageNamesAllowlist);

  auto* client = GetClient();
  auto one_day_from_now = base::Time::Now() + base::TimeDelta::FromDays(1);
  client->SetShouldRecordPackageName(/* expiry_date= */ one_day_from_now);
  client->SetShouldRecordPackageName(
      /* expiry_date= */ absl::optional<base::Time>());
  EXPECT_TRUE(client->ShouldRecordPackageName());
}

}  // namespace android_webview
