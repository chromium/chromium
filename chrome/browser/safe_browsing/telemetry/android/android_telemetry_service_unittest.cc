// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/telemetry/android/android_telemetry_service.h"

#include <memory>

#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

const char kOriginalURL[] = "https://test.site/original_url";
const char kTabURL[] = "https://test.site/tab_url";
const char kItemURL[] = "https://test.site/item_url";
const char kItemHash[] = "test_hash";
const int64_t kItemReceivedBytes = 5;
const char kItemTargetFilePath[] = "file.apk";

const char kApkDownloadTelemetryOutcomeMetric[] =
    "SafeBrowsing.AndroidTelemetry.ApkDownload.Outcome";

}  // namespace

class AndroidTelemetryServiceTest : public testing::Test {
 public:
  AndroidTelemetryServiceTest() = default;

 protected:
  base::HistogramTester* get_histograms() { return &histograms_; }

  Profile* profile() { return profile_.get(); }

  void SetUp() override {
    browser_process_ = TestingBrowserProcess::GetGlobal();

    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(
        GetSafeBrowsingServiceFactory());
    // TODO(crbug/925153): Port consumers of the |sb_service_| to use
    // the interface in components/safe_browsing, and remove this cast.
    sb_service_ = static_cast<SafeBrowsingService*>(
        safe_browsing::SafeBrowsingService::CreateSafeBrowsingService());
    browser_process_->SetSafeBrowsingService(sb_service_.get());
    sb_service_->Initialize();
    base::RunLoop().RunUntilIdle();

    download_item_.reset(new ::testing::NiceMock<download::MockDownloadItem>());
    profile_.reset(new TestingProfile());

    telemetry_service_ =
        std::make_unique<AndroidTelemetryService>(sb_service_.get(), profile());
  }

  void TearDown() override {
    // Make sure the NetworkContext owned by SafeBrowsingService is destructed
    // before the NetworkService object..
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  bool CanSendPing(download::DownloadItem* item) {
    return telemetry_service_->CanSendPing(item);
  }

  std::unique_ptr<ClientSafeBrowsingReportRequest> GetReport(
      download::DownloadItem* item) {
    return telemetry_service_->GetReport(item);
  }

  void SetOffTheRecordProfile() {
    telemetry_service_->profile_ = profile()->GetOffTheRecordProfile();
  }

  void ResetProfile() { telemetry_service_->profile_ = profile(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingBrowserProcess* browser_process_;
  std::unique_ptr<download::MockDownloadItem> download_item_;
  base::HistogramTester histograms_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AndroidTelemetryService> telemetry_service_;
};

TEST_F(AndroidTelemetryServiceTest, CantSendPing_NonApk) {
  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Simulate non-APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(testing::Return("text/plain"));

  EXPECT_FALSE(CanSendPing(download_item_.get()));

  // No metric is logged in this case.
  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 0);
}

TEST_F(AndroidTelemetryServiceTest, CantSendPing_SafeBrowsingDisabled) {
  // Disable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Simulate APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  EXPECT_FALSE(CanSendPing(download_item_.get()));

  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 1);
  get_histograms()->ExpectBucketCount(
      kApkDownloadTelemetryOutcomeMetric,
      ApkDownloadTelemetryOutcome::NOT_SENT_SAFE_BROWSING_NOT_ENABLED, 1);
}

TEST_F(AndroidTelemetryServiceTest, CantSendPing_IncognitoMode) {
  // No event is triggered if in incognito mode..
  SetOffTheRecordProfile();

  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Simulate APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  EXPECT_FALSE(CanSendPing(download_item_.get()));

  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 1);
  get_histograms()->ExpectBucketCount(
      kApkDownloadTelemetryOutcomeMetric,
      ApkDownloadTelemetryOutcome::NOT_SENT_INCOGNITO, 1);

  ResetProfile();
}

TEST_F(AndroidTelemetryServiceTest, CantSendPing_SBERDisabled) {
  // Disable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    false);

  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Simulate APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  EXPECT_FALSE(CanSendPing(download_item_.get()));

  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 1);
  get_histograms()->ExpectBucketCount(
      kApkDownloadTelemetryOutcomeMetric,
      ApkDownloadTelemetryOutcome::NOT_SENT_EXTENDED_REPORTING_DISABLED, 1);
}

TEST_F(AndroidTelemetryServiceTest, CanSendPing_AllConditionsMet) {
  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Simulate APK download.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  // The ping should be sent.
  EXPECT_TRUE(CanSendPing(download_item_.get()));

  // No metric is logged in this case.
  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 0);
}

TEST_F(AndroidTelemetryServiceTest, GetReport_ValidateAllFields) {
  ON_CALL(*download_item_, IsDone()).WillByDefault(testing::Return(true));
  ON_CALL(*download_item_, GetOriginalUrl())
      .WillByDefault(testing::ReturnRefOfCopy(GURL(kOriginalURL)));
  ON_CALL(*download_item_, GetTabUrl())
      .WillByDefault(testing::ReturnRefOfCopy(GURL(kTabURL)));
  ON_CALL(*download_item_, GetURL())
      .WillByDefault(testing::ReturnRefOfCopy(GURL(kItemURL)));
  ON_CALL(*download_item_, GetHash())
      .WillByDefault(testing::ReturnRefOfCopy(std::string(kItemHash)));
  ON_CALL(*download_item_, GetReceivedBytes())
      .WillByDefault(testing::Return(kItemReceivedBytes));
  ON_CALL(*download_item_, GetTargetFilePath())
      .WillByDefault(testing::ReturnRefOfCopy(
          base::FilePath(FILE_PATH_LITERAL(kItemTargetFilePath))));
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      GetReport(download_item_.get());
  ASSERT_TRUE(report);

  ASSERT_TRUE(report->has_type());
  EXPECT_EQ(ClientSafeBrowsingReportRequest::APK_DOWNLOAD, report->type());

  ASSERT_TRUE(report->has_url());
  EXPECT_EQ(kOriginalURL, report->url());

  ASSERT_TRUE(report->has_page_url());
  EXPECT_EQ(kTabURL, report->page_url());

  ASSERT_TRUE(report->has_download_item_info());
  ASSERT_TRUE(report->download_item_info().has_url());
  EXPECT_EQ(kItemURL, report->download_item_info().url());
  ASSERT_TRUE(report->download_item_info().has_digests());
  ASSERT_TRUE(report->download_item_info().digests().has_sha256());
  EXPECT_EQ(kItemHash, report->download_item_info().digests().sha256());
  ASSERT_TRUE(report->download_item_info().has_length());
  EXPECT_EQ(kItemReceivedBytes, report->download_item_info().length());
  ASSERT_TRUE(report->download_item_info().has_file_basename());
  EXPECT_EQ(kItemTargetFilePath, report->download_item_info().file_basename());

  ASSERT_TRUE(report->has_safety_net_id());
  // Empty since the Safety Net ID couldn't have been fetched in a unittest.
  EXPECT_EQ(0u, report->safety_net_id().length());
}

}  // namespace safe_browsing
