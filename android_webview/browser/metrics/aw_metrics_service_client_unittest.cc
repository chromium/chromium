// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_client.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {

// Scales up a uint32_t in the inverse of how UintFallsInBottomPercentOfValues()
// makes its judgment. This is useful so tests can use integers, which itself
// helps to avoid rounding issues.
uint32_t ScaleValue(uint32_t value) {
  DCHECK_GE(value, 0u);
  DCHECK_LE(value, 100u);
  double rate = static_cast<double>(value) / 100;
  return UINT32_MAX * rate;
}

// For client ID format, see:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
const char kTestClientId[] = "01234567-89ab-40cd-80ef-0123456789ab";

class TestClient : public AwMetricsServiceClient {
 public:
  TestClient()
      : sampled_in_rate_(1.00),
        in_sample_(true),
        record_package_name_for_app_type_(true),
        in_package_name_sample_(true) {}
  ~TestClient() override {}

  bool IsRecordingActive() {
    auto* service = GetMetricsService();
    if (service)
      return service->recording_active();
    return false;
  }
  void SetSampleRate(double value) { sampled_in_rate_ = value; }
  void SetInSample(bool value) { in_sample_ = value; }
  void SetRecordPackageNameForAppType(bool value) {
    record_package_name_for_app_type_ = value;
  }
  void SetInPackageNameSample(bool value) { in_package_name_sample_ = value; }

  // Expose the super class implementation for testing.
  bool IsInSample(uint32_t value) override {
    return AwMetricsServiceClient::IsInSample(value);
  }
  bool IsInPackageNameSample(uint32_t value) override {
    return AwMetricsServiceClient::IsInPackageNameSample(value);
  }

 protected:
  double GetSampleRate() override { return sampled_in_rate_; }
  bool IsInSample() override { return in_sample_; }
  bool CanRecordPackageNameForAppType() override {
    return record_package_name_for_app_type_;
  }
  bool IsInPackageNameSample() override { return in_package_name_sample_; }

 private:
  double sampled_in_rate_;
  bool in_sample_;
  bool record_package_name_for_app_type_;
  bool in_package_name_sample_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

std::unique_ptr<TestingPrefServiceSimple> CreateTestPrefs() {
  auto prefs = std::make_unique<TestingPrefServiceSimple>();
  metrics::MetricsService::RegisterPrefs(prefs->registry());
  return prefs;
}

std::unique_ptr<TestClient> CreateAndInitTestClient(PrefService* prefs) {
  auto client = std::make_unique<TestClient>();
  client->Initialize(prefs);
  return client;
}

}  // namespace

class AwMetricsServiceClientTest : public testing::Test {
 public:
  AwMetricsServiceClientTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        notification_service_(content::NotificationService::Create()) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
  }

 protected:
  ~AwMetricsServiceClientTest() override {}

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  // AwMetricsServiceClient::RegisterForNotifications() requires the
  // NotificationService to be up and running. Initialize it here, throw away
  // the value because we don't need it directly.
  std::unique_ptr<content::NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(AwMetricsServiceClientTest);
};

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(true, true);
  client->Initialize(prefs.get());
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseBeforeInit) {
  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();
  client->SetHaveMetricsConsent(false, false);
  client->Initialize(prefs.get());
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentTrueAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseAfterInit) {
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

// If there is already a valid client ID, it should be reused.
TEST_F(AwMetricsServiceClientTest, TestKeepExistingClientId) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  EXPECT_TRUE(client->IsRecordingActive());
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
  EXPECT_EQ(kTestClientId, prefs->GetString(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestSetConsentFalseClearsClientId) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(false, false);
  EXPECT_FALSE(client->IsRecordingActive());
  EXPECT_FALSE(prefs->HasPrefPath(metrics::prefs::kMetricsClientID));
}

TEST_F(AwMetricsServiceClientTest, TestShouldNotUploadPackageName_AppType) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(false);
  client->SetInPackageNameSample(true);
  std::string package_name = client->GetAppPackageName();
  EXPECT_TRUE(package_name.empty());
}

TEST_F(AwMetricsServiceClientTest, TestShouldNotUploadPackageName_SampledOut) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(true);
  client->SetInPackageNameSample(false);
  std::string package_name = client->GetAppPackageName();
  EXPECT_TRUE(package_name.empty());
}

TEST_F(AwMetricsServiceClientTest, TestCanUploadPackageName) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  client->SetHaveMetricsConsent(true, true);
  client->SetRecordPackageNameForAppType(true);
  client->SetInPackageNameSample(true);
  std::string package_name = client->GetAppPackageName();
  EXPECT_FALSE(package_name.empty());
}

TEST_F(AwMetricsServiceClientTest, TestPackageNameLogic_SampleRateBelowTen) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  double sample_rate = 0.08;
  client->SetSampleRate(sample_rate);

  // When GetSampleRate() <= 0.10, everything in-sample should also be in the
  // package name sample.
  for (uint32_t value = 0; value < 8; ++value) {
    EXPECT_TRUE(client->IsInSample(ScaleValue(value)))
        << "Value " << value << " should be in-sample";
    EXPECT_TRUE(client->IsInPackageNameSample(ScaleValue(value)))
        << "Value " << value << " should be in the package name sample";
  }
  // After this, the only thing we care about is that we're out of sample (the
  // package name logic shouldn't matter at this point, because we won't upload
  // any records).
  for (uint32_t value = 8; value < 100; ++value) {
    EXPECT_FALSE(client->IsInSample(ScaleValue(value)))
        << "Value " << value << " should be out of sample";
  }
}

TEST_F(AwMetricsServiceClientTest, TestPackageNameLogic_SampleRateAboveTen) {
  auto prefs = CreateTestPrefs();
  prefs->SetString(metrics::prefs::kMetricsClientID, kTestClientId);
  auto client = CreateAndInitTestClient(prefs.get());
  double sample_rate = 0.90;
  client->SetSampleRate(sample_rate);

  // When GetSampleRate() > 0.10, only values up to 0.10 should be in the
  // package name sample.
  for (uint32_t value = 0; value < 10; ++value) {
    EXPECT_TRUE(client->IsInSample(ScaleValue(value)))
        << "Value " << value << " should be in-sample";
    EXPECT_TRUE(client->IsInPackageNameSample(ScaleValue(value)))
        << "Value " << value << " should be in the package name sample";
  }
  // After this (but until we hit the sample rate), clients should be in sample
  // but not upload the package name.
  for (uint32_t value = 10; value < 90; ++value) {
    EXPECT_TRUE(client->IsInSample(ScaleValue(value)))
        << "Value " << value << " should be in-sample";
    EXPECT_FALSE(client->IsInPackageNameSample(ScaleValue(value)))
        << "Value " << value << " should be out of the package name sample";
  }
  // After this, the only thing we care about is that we're out of sample (the
  // package name logic shouldn't matter at this point, because we won't upload
  // any records).
  for (uint32_t value = 90; value < 100; ++value) {
    EXPECT_FALSE(client->IsInSample(ScaleValue(value)))
        << "Value " << value << " should be out of sample";
  }
}

TEST_F(AwMetricsServiceClientTest, TestCanForceEnableMetrics) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Flag should have higher precedence than sampling or user consent (but not
  // app consent, so we set that to 'true' for this case).
  client->SetHaveMetricsConsent(false, /* app_consent */ true);
  client->SetInSample(false);
  client->Initialize(prefs.get());

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
}

TEST_F(AwMetricsServiceClientTest, TestCanForceEnableMetricsIfAlreadyEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // This is a sanity check: flip consent and sampling to true, just to make
  // sure the flag continues to work.
  client->SetHaveMetricsConsent(true, true);
  client->SetInSample(true);
  client->Initialize(prefs.get());

  EXPECT_TRUE(client->IsReportingEnabled());
  EXPECT_TRUE(client->IsRecordingActive());
}

TEST_F(AwMetricsServiceClientTest, TestCannotForceEnableMetricsIfAppOptsOut) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      metrics::switches::kForceEnableMetricsReporting);

  auto prefs = CreateTestPrefs();
  auto client = std::make_unique<TestClient>();

  // Even with the flag, app consent should be respected.
  client->SetHaveMetricsConsent(true, /* app_consent */ false);
  client->SetInSample(true);
  client->Initialize(prefs.get());

  EXPECT_FALSE(client->IsReportingEnabled());
  EXPECT_FALSE(client->IsRecordingActive());
}

// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones.
TEST_F(AwMetricsServiceClientTest, TestPreferPersistedInstallDate) {
  base::HistogramTester histogram_tester;
  auto prefs = CreateTestPrefs();
  int64_t install_date = 12345;
  prefs->SetInt64(metrics::prefs::kInstallDate, install_date);
  auto client = CreateAndInitTestClient(prefs.get());
  EXPECT_EQ(install_date, prefs->GetInt64(metrics::prefs::kInstallDate));

  // Verify the histogram.
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.BackfillInstallDate",
      BackfillInstallDate::kValidInstallDatePref, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.BackfillInstallDate", 1);
}

// TODO(https://crbug.com/1012025): remove this when the kInstallDate pref has
// been persisted for one or two milestones.
TEST_F(AwMetricsServiceClientTest, TestGetInstallDateFromJavaIfMissing) {
  base::HistogramTester histogram_tester;
  auto prefs = CreateTestPrefs();
  auto client = CreateAndInitTestClient(prefs.get());
  // All we can safely assert is the install time is set, since checking the
  // actual time is racy (ex. in the unlikely scenario if this test executes in
  // the same millisecond as when the package was installed).
  EXPECT_TRUE(prefs->HasPrefPath(metrics::prefs::kInstallDate));

  // Verify the histogram.
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Metrics.BackfillInstallDate",
      BackfillInstallDate::kPersistedPackageManagerInstallDate, 1);
  histogram_tester.ExpectTotalCount(
      "Android.WebView.Metrics.BackfillInstallDate", 1);
}

}  // namespace android_webview
