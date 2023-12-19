// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <memory>

#include "android_webview/common/aw_features.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

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

// Adds support for setting whether metric reporting is in sample.
class AwMetricsServiceTestClient : public AwMetricsServiceClient {
 public:
  explicit AwMetricsServiceTestClient(std::unique_ptr<Delegate> delegate)
      : AwMetricsServiceClient(std::move(delegate)) {}
  void SetInSample(bool in_sample) { in_sample_ = in_sample; }
  void SetSampleBucketValue(int sample_bucket_value) {
    sample_bucket_value_ = sample_bucket_value;
  }

 protected:
  bool IsInSample() const override { return in_sample_; }
  int GetSampleBucketValue() const override { return sample_bucket_value_; }

 private:
  bool in_sample_ = false;
  int sample_bucket_value_ = 0;
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
        client_(std::make_unique<AwMetricsServiceTestClient>(
            std::make_unique<AwMetricsServiceClientTestDelegate>())) {
    base::SetRecordActionTaskRunner(task_runner_);
    AwMetricsServiceTestClient::RegisterMetricsPrefs(prefs_->registry());
    // Needed because RegisterMetricsProvidersAndInitState() checks for this.
    metrics::SubprocessMetricsProvider::CreateInstance();

    client_->Initialize(prefs_.get());
  }

  AwMetricsServiceTestClient* GetClient() { return client_.get(); }
  TestingPrefServiceSimple* GetPrefs() { return prefs_.get(); }

  void TriggerDelayedRecordAppDataDirectorySize() {
    task_environment_.FastForwardBy(kRecordAppDataDirectorySizeDelay);
  }

 private:
  // Needed since starting metrics reporting triggers code that uses content::
  // objects.
  content::TestContentClientInitializer test_content_initializer_;
  // Needed since starting metrics reporting triggers code that expects to be
  // running on the browser UI thread. Also needed for its FastForwardBy method.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<AwMetricsServiceTestClient> client_;
};

}  // namespace

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName) {
  AwMetricsServiceClient* client = GetClient();
  EXPECT_TRUE(client->ShouldRecordPackageName());
}

TEST_F(
    AwMetricsServiceClientTest,
    TestAppDataDirectorySize_RecordedIfFeatureEnabledConsentGrantedAndInSample) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewRecordAppDataDirectorySize);
  base::HistogramTester histogram_tester;

  GetClient()->SetInSample(true);
  GetClient()->SetHaveMetricsConsent(true, true);
  TriggerDelayedRecordAppDataDirectorySize();

  histogram_tester.ExpectTotalCount("Android.WebView.AppDataDirectory.Size", 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.AppDataDirectory.TimeToComputeSize", 1);
}

TEST_F(AwMetricsServiceClientTest,
       TestAppDataDirectorySize_NotRecordedIfFeatureDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      android_webview::features::kWebViewRecordAppDataDirectorySize);
  base::HistogramTester histogram_tester;

  GetClient()->SetInSample(true);
  GetClient()->SetHaveMetricsConsent(true, true);
  TriggerDelayedRecordAppDataDirectorySize();

  histogram_tester.ExpectTotalCount("Android.WebView.AppDataDirectory,Size", 0);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.AppDataDirectory.TimeToComputeSize", 0);
}

TEST_F(AwMetricsServiceClientTest,
       TestAppDataDirectorySize_NotRecordedIfConsentNotGranted) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewRecordAppDataDirectorySize);
  base::HistogramTester histogram_tester;

  GetClient()->SetInSample(true);
  GetClient()->SetHaveMetricsConsent(true, false);
  TriggerDelayedRecordAppDataDirectorySize();

  histogram_tester.ExpectTotalCount("Android.WebView.AppDataDirectory.Size", 0);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.AppDataDirectory.TimeToComputeSize", 0);
}

TEST_F(AwMetricsServiceClientTest,
       TestAppDataDirectorySize_NotRecordedIfNotInSample) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      android_webview::features::kWebViewRecordAppDataDirectorySize);
  base::HistogramTester histogram_tester;

  GetClient()->SetInSample(false);
  GetClient()->SetHaveMetricsConsent(true, true);
  TriggerDelayedRecordAppDataDirectorySize();

  histogram_tester.ExpectTotalCount("Android.WebView.AppDataDirectory.Size", 0);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.AppDataDirectory.TimeToComputeSize", 0);
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldApplyMetricsFilteringFeatureOn_AllMetrics) {
  // Both metrics consent and app consent true;
  GetClient()->SetHaveMetricsConsent(true, true);
  GetClient()->SetSampleBucketValue(19);

  EXPECT_EQ(GetClient()->GetSampleRatePerMille(), 1000);
  EXPECT_FALSE(GetClient()->ShouldApplyMetricsFiltering());
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldApplyMetricsFilteringFeatureOn_OnlyCriticalMetrics) {
  // Both metrics consent and app consent true;
  GetClient()->SetHaveMetricsConsent(true, true);
  GetClient()->SetSampleBucketValue(20);

  EXPECT_EQ(GetClient()->GetSampleRatePerMille(), 1000);
  EXPECT_TRUE(GetClient()->ShouldApplyMetricsFiltering());
}

}  // namespace android_webview
