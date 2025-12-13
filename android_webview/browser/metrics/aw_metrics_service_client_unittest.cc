// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <memory>

#include "android_webview/common/aw_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/persistent_histograms.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client_initializer.h"
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

// For client ID format, see:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
const char kTestClientId[] = "01234567-89ab-40cd-80ef-0123456789ab";
const char kTestFilename[] = "test_metric_file";

class TestClient : public AwMetricsServiceClient {
 public:
  TestClient()
      : AwMetricsServiceClient(
            std::make_unique<AwMetricsServiceClientTestDelegate>()) {}

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  ~TestClient() override = default;

  bool IsRecordingActive() {
    auto* service = GetMetricsService();
    if (service) {
      return service->recording_active();
    }
    return false;
  }

  void SetUnfilteredSampleRatePerMille(int per_mille) {
    unfiltered_sampled_in_rate_per_mille_ = per_mille;
  }

  void SetInUnfilteredSample(bool value) {
    unfiltered_sampled_in_rate_per_mille_ = value ? 1000 : 0;
  }

  void SetRecordPackageNameForAppType(bool value) {
    record_package_name_for_app_type_ = value;
  }

  void SetSampleBucketValue(int per_mille) { sample_bucket_value_ = per_mille; }

 protected:
  int GetSampleBucketValue() const override { return sample_bucket_value_; }

  int GetUnfilteredSampleRatePerMille() const override {
    return unfiltered_sampled_in_rate_per_mille_;
  }

  bool CanRecordPackageNameForAppType() override {
    return record_package_name_for_app_type_;
  }

 private:
  int sample_bucket_value_ = 0;
  int unfiltered_sampled_in_rate_per_mille_ = 1000;
  bool record_package_name_for_app_type_ = true;
};

class SampleBucketValueTestClient : public AwMetricsServiceClient {
 public:
  SampleBucketValueTestClient()
      : AwMetricsServiceClient(
            std::make_unique<AwMetricsServiceClientTestDelegate>()) {}

  SampleBucketValueTestClient(const SampleBucketValueTestClient&) = delete;
  SampleBucketValueTestClient& operator=(const SampleBucketValueTestClient&) =
      delete;

  ~SampleBucketValueTestClient() override = default;

  using AwMetricsServiceClient::GetSampleBucketValue;
};

std::unique_ptr<TestingPrefServiceSimple> CreateTestPrefs() {
  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  AwMetricsServiceClient::RegisterMetricsPrefs(prefs->registry());
  return prefs;
}

std::unique_ptr<TestClient> CreateAndInitTestClient(PrefService* prefs) {
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs);
  client->SetUpMetricsDir();
  return client;
}

}  // namespace

class AwMetricsServiceClientTest : public testing::Test {
 public:
  AwMetricsServiceClientTest()
      : test_begin_time_(base::Time::Now().ToTimeT()),
        task_runner_(new base::TestSimpleTaskRunner) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
    // Needed because RegisterMetricsProvidersAndInitState() checks for this.
    metrics::SubprocessMetricsProvider::CreateInstance();

    base::PathService::Get(base::DIR_ANDROID_APP_DATA, &old_metrics_dir_);
    new_metrics_dir_ = AwMetricsServiceClient::GetNoBackupFilesDir();
    base::CreateDirectory(new_metrics_dir_);
    old_spare_file_ = GetPersistentHistogramsSpareFilePath(old_metrics_dir_);
    new_spare_file_ = GetPersistentHistogramsSpareFilePath(new_metrics_dir_);
    old_upload_dir_ = old_metrics_dir_.AppendASCII(kBrowserMetricsName);
    new_upload_dir_ = new_metrics_dir_.AppendASCII(kBrowserMetricsName);
  }

  AwMetricsServiceClientTest(const AwMetricsServiceClientTest&) = delete;
  AwMetricsServiceClientTest& operator=(const AwMetricsServiceClientTest&) =
      delete;

  const int64_t test_begin_time_;

  // Used for metrics migration tests.
  base::FilePath old_metrics_dir_;
  base::FilePath new_metrics_dir_;
  base::FilePath old_spare_file_;
  base::FilePath new_spare_file_;
  base::FilePath old_upload_dir_;
  base::FilePath new_upload_dir_;

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 protected:
  ~AwMetricsServiceClientTest() override {
    // The global allocator has to be detached here so that no metrics created
    // by code called below get stored in it as that would make for potential
    // use-after-free operations if that code is called again.
    base::GlobalHistogramAllocator::ReleaseForTesting();
  }

 private:
  // Needed since starting metrics reporting triggers code that uses content::
  // objects.
  content::TestContentClientInitializer test_content_initializer_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

// Verify that Chrome does not start watching for browser crashes before setting
// up field trials. For Android embedders, Chrome should not watch for crashes
// then because, at the time of field trial set-up, it is not possible to know
// whether the embedder will come to foreground. The embedder may remain in the
// background for the browser process lifetime, and in this case, Chrome should
// not watch for crashes so that exiting is not considered a crash. Embedders
// start watching for crashes when foregrounding via
// MetricsService::OnAppEnterForeground().
TEST_F(AwMetricsServiceClientTest, DoNotWatchForCrashesBeforeFieldTrialSetUp) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  EXPECT_TRUE(client->metrics_state_manager()
                  ->clean_exit_beacon()
                  ->GetUserDataDirForTesting()
                  .empty());
  EXPECT_TRUE(client->metrics_state_manager()
                  ->clean_exit_beacon()
                  ->GetBeaconFilePathForTesting()
                  .empty());
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(true, true);
  client->Initialize(prefs.get());
  client->SetUpMetricsDir();
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_TRUE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(false, false);
  client->Initialize(prefs.get());
  client->SetUpMetricsDir();
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_FALSE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_GE(prefs->GetInt64(metrics::prefs::kMetricsReportingEnabledTimestamp),
            test_begin_time_);
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_FALSE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

// If there is already a valid client ID and enabled date, they should be
// reused.
TEST_F(AwMetricsServiceClientTest, TestKeepExistingClientIdAndEnabledDate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  int64_t enabled_date = 12345;
  prefs->SetInt64(metrics::prefs::kMetricsReportingEnabledTimestamp,
                  enabled_date);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_EQ(kTestClientId, prefs->GetString(metrics::prefs::kMetricsClientID));
  EXPECT_EQ(enabled_date,
            prefs->GetInt64(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseClearsIdAndEnabledDate) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_FALSE(
      prefs->HasPrefPath(metrics::prefs::kMetricsReportingEnabledTimestamp));
}

TEST_F(AwMetricsServiceClientTest, TestShouldNotUploadPackageName_AppType) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(false);
  std::string package_name = client->GetAppPackageNameIfLoggable();
  EXPECT_TRUE(package_name.empty());
}

TEST_F(AwMetricsServiceClientTest, TestCanUploadPackageName) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(true);
  std::string package_name = client->GetAppPackageNameIfLoggable();
  EXPECT_FALSE(package_name.empty());
}

TEST_F(AwMetricsServiceClientTest, TestGetPackageNameInternal) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  // Make sure GetPackageName returns a non-empty string.
  EXPECT_FALSE(client->GetAppPackageName().empty());
}

TEST_F(AwMetricsServiceClientTest, TestCanForceEnableMetrics) {
  metrics::ForceEnableMetricsReportingForTesting();

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  // Flag should have higher precedence than sampling or user consent (but not
  // app consent, so we set that to 'true' for this case).
  client->SetInUnfilteredSample(false);
  client->SetHaveMetricsConsent(false, /* app_consent */ true);

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_FALSE(client->ShouldApplyMetricsFiltering());
}

TEST_F(AwMetricsServiceClientTest, TestCanForceEnableMetricsIfAlreadyEnabled) {
  metrics::ForceEnableMetricsReportingForTesting();

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  // This is a sanity check: flip consent and sampling to true, just to make
  // sure the flag continues to work.
  client->SetInUnfilteredSample(true);
  client->SetHaveMetricsConsent(true, true);

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_FALSE(client->ShouldApplyMetricsFiltering());
}

TEST_F(AwMetricsServiceClientTest, TestCannotForceEnableMetricsIfAppOptsOut) {
  metrics::ForceEnableMetricsReportingForTesting();

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  // Even with the flag, app consent should be respected.
  client->SetHaveMetricsConsent(true, /* app_consent */ false);

  EXPECT_FALSE(client->IsReportingEnabled());
  EXPECT_FALSE(client->IsRecordingActive());
}

TEST_F(AwMetricsServiceClientTest, TestBrowserMetricsDirClearedIfNoConsent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPersistentHistogramsFeature, {{"storage", "MappedFile"}});

  base::FilePath metrics_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &metrics_dir));
  InstantiatePersistentHistogramsWithFeaturesAndCleanup(metrics_dir);
  base::FilePath upload_dir = metrics_dir.AppendASCII(kBrowserMetricsName);
  ASSERT_TRUE(base::PathExists(upload_dir));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  // No consent should delete data regardless of sampling.
  client->SetInUnfilteredSample(true);
  client->SetHaveMetricsConsent(/* user_consent= */ false,
                                /* app_consent= */ false);
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(base::PathExists(upload_dir));
}

TEST_F(AwMetricsServiceClientTest,
       TestBrowserMetricsDirExistsIfReportingEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPersistentHistogramsFeature, {{"storage", "MappedFile"}});

  base::FilePath metrics_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &metrics_dir));
  InstantiatePersistentHistogramsWithFeaturesAndCleanup(metrics_dir);
  base::FilePath upload_dir = metrics_dir.AppendASCII(kBrowserMetricsName);
  ASSERT_TRUE(base::PathExists(upload_dir));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  // We should still set up the data even if the client is filtered.
  client->SetInUnfilteredSample(false);
  client->SetHaveMetricsConsent(/* user_consent= */ true,
                                /* app_consent= */ true);
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(base::PathExists(upload_dir));
}

TEST_F(AwMetricsServiceClientTest,
       MetricsServiceCreatedFromInitializeWithNoConsent) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  EXPECT_FALSE(client->IsReportingEnabled());
  EXPECT_TRUE(client->GetMetricsService());
}

TEST_F(AwMetricsServiceClientTest, GetMetricsServiceIfStarted) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  EXPECT_EQ(nullptr, client->GetMetricsServiceIfStarted());
  client->SetHaveMetricsConsent(/* user_consent= */ true,
                                /* app_consent= */ true);
  EXPECT_TRUE(client->GetMetricsServiceIfStarted());
}

TEST_F(AwMetricsServiceClientTest, ShouldComputeCorrectSampleBucketValues) {
  // The following sample values were generated by using
  // https://www.uuidgenerator.net/version4
  struct {
    const char* client_uuid;
    int expected_sample_bucket_value;
  } test_cases[] = {{"01234567-89ab-40cd-80ef-0123456789ab", 946},
                    {"00aa37bf-7fba-47a7-9180-e334f5c69a8e", 607},
                    {"a7a68d68-8ba3-486d-832b-a0cded65fea2", 995},
                    {"5aed7b5d-b827-400d-9d28-5d23dcc076dc", 802},
                    {"fa5f5bd4-aae7-4d94-ab84-69c8ca40f400", 100}};

  for (const auto& test : test_cases) {
    auto prefs = CreateTestPrefs();
    prefs->SetString(metrics::prefs::kMetricsClientID, test.client_uuid);
    auto client = std::make_unique<SampleBucketValueTestClient>();
    client->SetHaveMetricsConsent(/*user_consent=*/true, /*app_consent=*/true);
    client->Initialize(prefs.get());
    client->SetUpMetricsDir();

    EXPECT_EQ(client->GetSampleBucketValue(),
              test.expected_sample_bucket_value);
  }
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldApplyMetricsFilteringFeatureOn_AllMetrics) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  client->SetUnfilteredSampleRatePerMille(20);
  client->SetSampleBucketValue(19);
  // Both metrics consent and app consent true;
  client->SetHaveMetricsConsent(true, true);

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_FALSE(client->ShouldApplyMetricsFiltering());
}

TEST_F(AwMetricsServiceClientTest,
       TestShouldApplyMetricsFilteringFeatureOn_OnlyCriticalMetrics) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  client->SetUnfilteredSampleRatePerMille(20);
  client->SetSampleBucketValue(20);
  // Both metrics consent and app consent true;
  client->SetHaveMetricsConsent(true, true);

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(client->ShouldApplyMetricsFiltering());
}

TEST_F(AwMetricsServiceClientTest, TestMetricsDirMigration_NoSpareFile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewPersistentMetricsInNoBackupDir);

  ASSERT_TRUE(base::DeleteFile(old_spare_file_));
  ASSERT_TRUE(base::DeleteFile(new_spare_file_));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  EXPECT_FALSE(base::PathExists(old_spare_file_));
  EXPECT_FALSE(base::PathExists(new_spare_file_));
}

TEST_F(AwMetricsServiceClientTest, TestMetricsDirMigration_OldSpareFile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewPersistentMetricsInNoBackupDir);

  ASSERT_TRUE(base::WriteFile(old_spare_file_, ""));
  ASSERT_TRUE(base::DeleteFile(new_spare_file_));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  EXPECT_FALSE(base::PathExists(old_spare_file_));
  EXPECT_TRUE(base::PathExists(new_spare_file_));
}

TEST_F(AwMetricsServiceClientTest, TestMetricsDirMigration_NewSpareFile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewPersistentMetricsInNoBackupDir);

  ASSERT_TRUE(base::DeleteFile(old_spare_file_));
  ASSERT_TRUE(base::WriteFile(new_spare_file_, ""));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  EXPECT_FALSE(base::PathExists(old_spare_file_));
  EXPECT_TRUE(base::PathExists(new_spare_file_));
}

TEST_F(AwMetricsServiceClientTest, TestMetricsDirMigration_BothSpareFiles) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewPersistentMetricsInNoBackupDir);

  ASSERT_TRUE(base::WriteFile(old_spare_file_, ""));
  ASSERT_TRUE(base::WriteFile(new_spare_file_, ""));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  EXPECT_FALSE(base::PathExists(old_spare_file_));
  EXPECT_TRUE(base::PathExists(new_spare_file_));
}

TEST_F(AwMetricsServiceClientTest, TestMetricsDirMigration_NoUploadDir) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewPersistentMetricsInNoBackupDir);

  ASSERT_TRUE(base::DeletePathRecursively(old_upload_dir_));
  ASSERT_TRUE(base::DeletePathRecursively(new_upload_dir_));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  EXPECT_FALSE(base::PathExists(new_upload_dir_));
  EXPECT_EQ(new_metrics_dir_, client->GetMetricsDir());
  EXPECT_FALSE(base::PathExists(old_upload_dir_));
  EXPECT_TRUE(client->GetOldMetricsDirForTesting().empty());
}

TEST_F(AwMetricsServiceClientTest,
       TestMetricsDirMigration_OldUploadDirNonEmpty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewPersistentMetricsInNoBackupDir);

  ASSERT_TRUE(base::CreateDirectory(old_upload_dir_));
  ASSERT_TRUE(base::WriteFile(old_upload_dir_.AppendASCII(kTestFilename), ""));
  ASSERT_TRUE(base::DeletePathRecursively(new_upload_dir_));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  EXPECT_FALSE(base::PathExists(new_upload_dir_));
  EXPECT_EQ(new_metrics_dir_, client->GetMetricsDir());
  EXPECT_TRUE(base::PathExists(old_upload_dir_));
  EXPECT_EQ(old_metrics_dir_, client->GetOldMetricsDirForTesting());
}

TEST_F(AwMetricsServiceClientTest, TestMetricsDirMigration_OldUploadDirEmpty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewPersistentMetricsInNoBackupDir);

  ASSERT_TRUE(base::DeletePathRecursively(old_upload_dir_));
  ASSERT_TRUE(base::CreateDirectory(old_upload_dir_));
  ASSERT_TRUE(base::DeletePathRecursively(new_upload_dir_));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  EXPECT_FALSE(base::PathExists(new_upload_dir_));
  EXPECT_EQ(new_metrics_dir_, client->GetMetricsDir());
  EXPECT_FALSE(base::PathExists(old_upload_dir_));
  EXPECT_TRUE(client->GetOldMetricsDirForTesting().empty());
}

TEST_F(AwMetricsServiceClientTest, TestMetricsDirMigration_NewUploadDir) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewPersistentMetricsInNoBackupDir);

  ASSERT_TRUE(base::DeletePathRecursively(old_upload_dir_));
  ASSERT_TRUE(base::CreateDirectory(new_upload_dir_));
  ASSERT_TRUE(base::WriteFile(new_upload_dir_.AppendASCII(kTestFilename), ""));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  EXPECT_TRUE(base::PathExists(new_upload_dir_));
  EXPECT_EQ(new_metrics_dir_, client->GetMetricsDir());
  EXPECT_FALSE(base::PathExists(old_upload_dir_));
  EXPECT_TRUE(client->GetOldMetricsDirForTesting().empty());
}

TEST_F(AwMetricsServiceClientTest, TestMetricsDirMigration_BothUploadDirs) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebViewPersistentMetricsInNoBackupDir);

  ASSERT_TRUE(base::CreateDirectory(old_upload_dir_));
  ASSERT_TRUE(base::WriteFile(old_upload_dir_.AppendASCII(kTestFilename), ""));
  ASSERT_TRUE(base::CreateDirectory(new_upload_dir_));
  ASSERT_TRUE(base::WriteFile(new_upload_dir_.AppendASCII(kTestFilename), ""));

  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());

  EXPECT_TRUE(base::PathExists(new_upload_dir_));
  EXPECT_EQ(new_metrics_dir_, client->GetMetricsDir());
  EXPECT_TRUE(base::PathExists(old_upload_dir_));
  EXPECT_EQ(old_metrics_dir_, client->GetOldMetricsDirForTesting());
}

}  // namespace android_webview
