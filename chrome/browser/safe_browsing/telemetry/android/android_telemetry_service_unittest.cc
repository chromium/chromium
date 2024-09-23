// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/telemetry/android/android_telemetry_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
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
    // TODO(crbug.com/41437292): Port consumers of the |sb_service_| to use
    // the interface in components/safe_browsing, and remove this cast.
    sb_service_ = static_cast<SafeBrowsingService*>(
        safe_browsing::SafeBrowsingService::CreateSafeBrowsingService());
    browser_process_->SetSafeBrowsingService(sb_service_.get());
    sb_service_->Initialize();
    base::RunLoop().RunUntilIdle();

    download_item_ =
        std::make_unique<::testing::NiceMock<download::MockDownloadItem>>();
    profile_ = std::make_unique<TestingProfile>();

    telemetry_service_ = std::make_unique<AndroidTelemetryService>(profile());
  }

  void TearDown() override {
    // Make sure the NetworkContext owned by SafeBrowsingService is destructed
    // before the NetworkService object..
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  bool CanSendPing(download::DownloadItem* item) {
    return telemetry_service_->CanSendPing(item);
  }

  std::unique_ptr<ClientSafeBrowsingReportRequest> GetReport(
      download::DownloadItem* item) {
    base::test::TestFuture<std::unique_ptr<ClientSafeBrowsingReportRequest>>
        report_future;
    telemetry_service_->GetReport(item, report_future.GetCallback());
    return report_future.Take();
  }

  void SetOffTheRecordProfile() {
    telemetry_service_->profile_ =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  void ResetProfile() { telemetry_service_->profile_ = profile(); }

  void SetVerifyAppsResult(VerifyAppsEnabledResult result) {
    SafeBrowsingApiHandlerBridge::GetInstance()
        .SetVerifyAppsEnableResultForTesting(result);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingBrowserProcess> browser_process_;
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
  ON_CALL(*download_item_, GetFileNameToReportUser())
      .WillByDefault(
          testing::Return(base::FilePath(FILE_PATH_LITERAL("file.txt"))));

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
  ON_CALL(*download_item_, GetFileNameToReportUser())
      .WillByDefault(
          testing::Return(base::FilePath(FILE_PATH_LITERAL("file.apk"))));

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
  ON_CALL(*download_item_, GetFileNameToReportUser())
      .WillByDefault(
          testing::Return(base::FilePath(FILE_PATH_LITERAL("file.apk"))));

  EXPECT_FALSE(CanSendPing(download_item_.get()));

  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 1);
  get_histograms()->ExpectBucketCount(
      kApkDownloadTelemetryOutcomeMetric,
      ApkDownloadTelemetryOutcome::NOT_SENT_INCOGNITO, 1);

  ResetProfile();
}

TEST_F(AndroidTelemetryServiceTest,
       CantSendPing_SBEREnhancedProtectionDisabled) {
  // Disable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    false);
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);

  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Simulate APK download.
  ON_CALL(*download_item_, GetFileNameToReportUser())
      .WillByDefault(
          testing::Return(base::FilePath(FILE_PATH_LITERAL("file.apk"))));

  EXPECT_FALSE(CanSendPing(download_item_.get()));

  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 1);
  get_histograms()->ExpectBucketCount(
      kApkDownloadTelemetryOutcomeMetric,
      ApkDownloadTelemetryOutcome::NOT_SENT_UNCONSENTED, 1);
}

TEST_F(AndroidTelemetryServiceTest, CanSendPing_AllConditionsMet) {
  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Simulate APK download.
  ON_CALL(*download_item_, GetFileNameToReportUser())
      .WillByDefault(
          testing::Return(base::FilePath(FILE_PATH_LITERAL("file.apk"))));

  // The ping should be sent.
  EXPECT_TRUE(CanSendPing(download_item_.get()));

  // No metric is logged in this case, because SENT is logged in another
  // function.
  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 0);
}

TEST_F(AndroidTelemetryServiceTest,
       CanSendPing_AllConditionsMetMimeTypeNotApk) {
  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Simulate APK download. Set file type to APK.
  ON_CALL(*download_item_, GetFileNameToReportUser())
      .WillByDefault(
          testing::Return(base::FilePath(FILE_PATH_LITERAL("file.apk"))));

  // Set MIME type to non-APK.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(testing::Return("text/plain"));

  // The ping should be sent even though the MIME type is not apk.
  EXPECT_TRUE(CanSendPing(download_item_.get()));

  // No metric is logged in this case, because SENT is logged in another
  // function.
  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 0);
}

TEST_F(AndroidTelemetryServiceTest,
       CanSendPing_AllConditionsMetFilePathNotApk) {
  // Enable Safe Browsing.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  // Enable Scout Reporting.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                                    true);
  // Simulate APK download. Set file type to non-APK.
  ON_CALL(*download_item_, GetFileNameToReportUser())
      .WillByDefault(
          testing::Return(base::FilePath(FILE_PATH_LITERAL("file.txt"))));

  // Set MIME type to APK.
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  // The ping should be sent even though the file type is not apk.
  EXPECT_TRUE(CanSendPing(download_item_.get()));

  // No metric is logged in this case, because SENT is logged in another
  // function.
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
  g_browser_process->SetApplicationLocale("en_US");
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      GetReport(download_item_.get());
  ASSERT_TRUE(report);

  ASSERT_TRUE(report->has_type());
  EXPECT_EQ(ClientSafeBrowsingReportRequest::APK_DOWNLOAD, report->type());

  ASSERT_TRUE(report->has_url());
  EXPECT_EQ(kOriginalURL, report->url());

  ASSERT_TRUE(report->has_page_url());
  EXPECT_EQ(kTabURL, report->page_url());

  EXPECT_EQ(report->locale(), "en_US");

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
}

TEST_F(AndroidTelemetryServiceTest, AppVerification) {
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

  SetVerifyAppsResult(VerifyAppsEnabledResult::SUCCESS_ENABLED);
  EXPECT_TRUE(GetReport(download_item_.get())
                  ->client_properties()
                  .app_verification_enabled());

  SetVerifyAppsResult(VerifyAppsEnabledResult::SUCCESS_NOT_ENABLED);
  EXPECT_FALSE(GetReport(download_item_.get())
                   ->client_properties()
                   .app_verification_enabled());

  SetVerifyAppsResult(VerifyAppsEnabledResult::FAILED);
  EXPECT_FALSE(GetReport(download_item_.get())
                   ->client_properties()
                   .has_app_verification_enabled());
}

// Regression test for https://crbug.com/1173145#c17.
TEST_F(AndroidTelemetryServiceTest,
       OnDownloadUpdated_ObserverNotRemovedIfDownloadIsNotCompleted) {
  // Disable Safe Browsing so we can log the telemetry outcome metric.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  download_item_->AddObserver(telemetry_service_.get());

  // Simulate APK download. The file name is not populated yet but the MIME type
  // is APK.
  ON_CALL(*download_item_, GetFileNameToReportUser())
      .WillByDefault(testing::Return(base::FilePath(FILE_PATH_LITERAL(""))));
  ON_CALL(*download_item_, GetMimeType())
      .WillByDefault(
          testing::Return("application/vnd.android.package-archive"));

  // This should trigger OnDownloadUpdated.
  download_item_->NotifyObserversDownloadUpdated();
  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 1);

  // OnDownloadUpdated should still be called.
  download_item_->NotifyObserversDownloadUpdated();
  get_histograms()->ExpectTotalCount(kApkDownloadTelemetryOutcomeMetric, 2);

  download_item_->RemoveObserver(telemetry_service_.get());
}

}  // namespace safe_browsing
