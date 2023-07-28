// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <memory>

#include "android_webview/common/aw_features.h"
#include "android_webview/common/metrics/app_package_name_logging_rule.h"
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

using AppPackageNameLoggingRuleStatus =
    AwMetricsServiceClient::AppPackageNameLoggingRuleStatus;

using InstallerPackageType =
    metrics::AndroidMetricsServiceClient::InstallerPackageType;

namespace {

constexpr char kTestAllowlistVersion[] = "123.456.789.10";

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

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName_CacheNotSet) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      android_webview::features::kWebViewAppsPackageNamesServerSideAllowlist);

  AwMetricsServiceClient* client = GetClient();
  EXPECT_FALSE(client->ShouldRecordPackageName());
  EXPECT_FALSE(client->GetCachedAppPackageNameLoggingRule().has_value());

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 0);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNotLoadedNoCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire", 0);
}

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName_WithCache) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      android_webview::features::kWebViewAppsPackageNamesServerSideAllowlist);

  AwMetricsServiceClient* client = GetClient();
  TestingPrefServiceSimple* prefs = GetPrefs();

  base::TimeDelta expiry_time = base::Days(1);
  AppPackageNameLoggingRule expected_record(
      base::Version(kTestAllowlistVersion), base::Time::Now() + expiry_time);
  prefs->SetDict(prefs::kMetricsAppPackageNameLoggingRule,
                 expected_record.ToDictionary());

  absl::optional<AppPackageNameLoggingRule> cached_record =
      client->GetCachedAppPackageNameLoggingRule();
  EXPECT_TRUE(client->ShouldRecordPackageName());
  ASSERT_TRUE(cached_record.has_value());
  EXPECT_TRUE(expected_record.IsSameAs(cached_record.value()));

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 0);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNotLoadedUseCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire", 1);
  histogram_tester.ExpectUniqueSample(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire",
      expiry_time.InHours(), 1);
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldRecordPackageName_TestShouldNotRecordPackageName) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      android_webview::features::kWebViewAppsPackageNamesServerSideAllowlist);

  AwMetricsServiceClient* client = GetClient();
  AppPackageNameLoggingRule expected_record(
      base::Version(kTestAllowlistVersion), base::Time::Min());
  client->SetAppPackageNameLoggingRule(expected_record);
  absl::optional<AppPackageNameLoggingRule> cached_record =
      client->GetCachedAppPackageNameLoggingRule();

  EXPECT_FALSE(client->ShouldRecordPackageName());
  ASSERT_TRUE(cached_record.has_value());
  EXPECT_TRUE(expected_record.IsSameAs(cached_record.value()));

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 1);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNewVersionLoaded, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire", 0);
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldRecordPackageName_TestShouldRecordPackageName) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      android_webview::features::kWebViewAppsPackageNamesServerSideAllowlist);

  AwMetricsServiceClient* client = GetClient();

  base::TimeDelta expiry_time = base::Days(1);
  AppPackageNameLoggingRule expected_record(
      base::Version(kTestAllowlistVersion), base::Time::Now() + expiry_time);
  client->SetAppPackageNameLoggingRule(expected_record);
  absl::optional<AppPackageNameLoggingRule> cached_record =
      client->GetCachedAppPackageNameLoggingRule();

  EXPECT_TRUE(client->ShouldRecordPackageName());
  ASSERT_TRUE(cached_record.has_value());
  EXPECT_TRUE(expected_record.IsSameAs(cached_record.value()));

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 1);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNewVersionLoaded, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire", 1);
  histogram_tester.ExpectUniqueSample(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire",
      expiry_time.InHours(), 1);
}

TEST_F(
    AwMetricsServiceClientTest,
    TestServerSideAllowlist_TestShouldRecordPackageNameWithServerSideAllowlistEnabled) {
  AwMetricsServiceClient* client = GetClient();
  EXPECT_TRUE(client->ShouldRecordPackageName());
  EXPECT_FALSE(client->GetCachedAppPackageNameLoggingRule().has_value());
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldRecordPackageName_TestFailureAfterValidResult) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      android_webview::features::kWebViewAppsPackageNamesServerSideAllowlist);

  AwMetricsServiceClient* client = GetClient();

  base::TimeDelta expiry_time = base::Days(1);
  AppPackageNameLoggingRule expected_record(
      base::Version(kTestAllowlistVersion), base::Time::Now() + expiry_time);
  client->SetAppPackageNameLoggingRule(expected_record);
  client->SetAppPackageNameLoggingRule(
      absl::optional<AppPackageNameLoggingRule>());
  absl::optional<AppPackageNameLoggingRule> cached_record =
      client->GetCachedAppPackageNameLoggingRule();

  EXPECT_TRUE(client->ShouldRecordPackageName());
  ASSERT_TRUE(cached_record.has_value());
  EXPECT_TRUE(expected_record.IsSameAs(cached_record.value()));

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 1);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNewVersionFailedUseCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire", 1);
  histogram_tester.ExpectUniqueSample(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire",
      expiry_time.InHours(), 1);
}

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName_FailedResult) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      android_webview::features::kWebViewAppsPackageNamesServerSideAllowlist);

  AwMetricsServiceClient* client = GetClient();
  client->SetAppPackageNameLoggingRule(
      absl::optional<AppPackageNameLoggingRule>());

  EXPECT_FALSE(client->ShouldRecordPackageName());

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 0);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kNewVersionFailedNoCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire", 0);
}

TEST_F(AwMetricsServiceClientTest, TestShouldRecordPackageName_SameAsCache) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      android_webview::features::kWebViewAppsPackageNamesServerSideAllowlist);

  AwMetricsServiceClient* client = GetClient();
  TestingPrefServiceSimple* prefs = GetPrefs();

  base::TimeDelta expiry_time = base::Days(1);
  AppPackageNameLoggingRule record(base::Version(kTestAllowlistVersion),
                                   base::Time::Now() + expiry_time);
  prefs->SetDict(prefs::kMetricsAppPackageNameLoggingRule,
                 record.ToDictionary());
  client->SetAppPackageNameLoggingRule(record);

  EXPECT_TRUE(client->ShouldRecordPackageName());

  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.ResultReceivingDelay", 0);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus",
      AppPackageNameLoggingRuleStatus::kSameVersionAsCache, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.RecordStatus", 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire", 1);
  histogram_tester.ExpectUniqueSample(
      "Android.WebView.Metrics.PackagesAllowList.TimeToExpire",
      expiry_time.InHours(), 1);
}

TEST_F(AwMetricsServiceClientTest, TestGetAppPackageNameIfLoggable) {
  class TestClient : public AwMetricsServiceClient {
   public:
    TestClient()
        : AwMetricsServiceClient(
              std::make_unique<AwMetricsServiceClientTestDelegate>()) {}
    ~TestClient() override = default;

    bool ShouldRecordPackageName() override {
      return should_record_package_name_;
    }

    void SetShouldRecordPackageName(bool value) {
      should_record_package_name_ = value;
    }

    InstallerPackageType GetInstallerPackageType() override {
      return installer_type_;
    }

    void SetInstallerPackageType(InstallerPackageType installer_type) {
      installer_type_ = installer_type;
    }

   private:
    bool should_record_package_name_;
    InstallerPackageType installer_type_;
  };

  TestClient client;

  // Package names of system apps are always loggable even if they are not in
  // the allowlist of apps.
  client.SetInstallerPackageType(InstallerPackageType::SYSTEM_APP);
  client.SetShouldRecordPackageName(false);
  EXPECT_FALSE(client.GetAppPackageNameIfLoggable().empty());
  client.SetShouldRecordPackageName(true);
  EXPECT_FALSE(client.GetAppPackageNameIfLoggable().empty());

  // Package names of APPs that are installed by the Play Store are loggable if
  // they are in the allowlist of apps.
  client.SetInstallerPackageType(InstallerPackageType::GOOGLE_PLAY_STORE);
  client.SetShouldRecordPackageName(false);
  EXPECT_TRUE(client.GetAppPackageNameIfLoggable().empty());
  client.SetShouldRecordPackageName(true);
  EXPECT_FALSE(client.GetAppPackageNameIfLoggable().empty());

  // Package names of APPs that are not system apps nor installed by the Play
  // Store are not loggable.
  client.SetInstallerPackageType(InstallerPackageType::OTHER);
  client.SetShouldRecordPackageName(false);
  EXPECT_TRUE(client.GetAppPackageNameIfLoggable().empty());
  client.SetShouldRecordPackageName(true);
  EXPECT_TRUE(client.GetAppPackageNameIfLoggable().empty());
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

TEST_F(AwMetricsServiceClientTest, TestShouldApplyMetricsFilteringFeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      android_webview::features::kWebViewMetricsFiltering);

  // Both metrics consent and app consent true;
  GetClient()->SetHaveMetricsConsent(true, true);

  EXPECT_EQ(GetClient()->GetSampleRatePerMille(), 20);
  EXPECT_EQ(GetClient()->ShouldApplyMetricsFiltering(), false);
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldApplyMetricsFilteringFeatureOn_AllMetrics) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      android_webview::features::kWebViewMetricsFiltering);

  // Both metrics consent and app consent true;
  GetClient()->SetHaveMetricsConsent(true, true);
  GetClient()->SetSampleBucketValue(19);

  EXPECT_EQ(GetClient()->GetSampleRatePerMille(), 1000);
  EXPECT_FALSE(GetClient()->ShouldApplyMetricsFiltering());
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldApplyMetricsFilteringFeatureOn_OnlyCriticalMetrics) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      android_webview::features::kWebViewMetricsFiltering);

  // Both metrics consent and app consent true;
  GetClient()->SetHaveMetricsConsent(true, true);
  GetClient()->SetSampleBucketValue(20);

  EXPECT_EQ(GetClient()->GetSampleRatePerMille(), 1000);
  EXPECT_TRUE(GetClient()->ShouldApplyMetricsFiltering());
}

}  // namespace android_webview
