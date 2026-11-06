// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_controller.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/android/dangerous_download_dialog_bridge.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_source.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

class TestMockDownloadItem : public download::MockDownloadItem {
 public:
  TestMockDownloadItem() {
    ON_CALL(*this, RemoveObserver(_))
        .WillByDefault([this](download::DownloadItem::Observer* observer) {
          download::MockDownloadItem::RemoveObserver(observer);
        });
  }

  MOCK_METHOD(void,
              RemoveObserver,
              (download::DownloadItem::Observer*),
              (override));
};

class DownloadControllerTest : public testing::Test {
 public:
  DownloadControllerTest() {
    content::DownloadItemUtils::AttachInfoForTesting(&item_, &profile_,
                                                     nullptr);
  }

  DownloadControllerTest(const DownloadControllerTest&) = delete;
  DownloadControllerTest& operator=(const DownloadControllerTest&) = delete;

  void SetUp() override {
    profile_.SetDownloadManagerForTesting(
        std::make_unique<NiceMock<content::MockDownloadManager>>());
    DownloadCoreServiceFactory::GetForBrowserContext(&profile_)
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(&profile_));
  }

  void TearDown() override {
    DownloadCoreServiceFactory::GetForBrowserContext(&profile_)
        ->SetDownloadManagerDelegateForTesting(nullptr);
  }

  TestMockDownloadItem* item() { return &item_; }
  Profile* profile() { return &profile_; }

  DownloadController* controller() { return &controller_; }

  DangerousDownloadDialogBridge* dangerous_download_bridge() {
    return controller_.dangerous_download_bridge_.get();
  }

  bool ShouldShowAppVerificationPrompt(download::DownloadItem* item) {
    return controller_.ShouldShowAppVerificationPrompt(item);
  }

  void OnDownloadUpdated(download::DownloadItem* item) {
    controller_.OnDownloadUpdated(item);
  }

  void OnDownloadFailed(download::DownloadItem* item) {
    controller_.OnDownloadFailed(item);
  }

  void StubDownloadInfoGetters() {
    EXPECT_CALL(item_, GetDownloadSource())
        .WillRepeatedly(Return(download::DownloadSource::NAVIGATION));
    EXPECT_CALL(item_, IsDone()).WillRepeatedly(Return(false));
    EXPECT_CALL(item_, GetGuid())
        .WillRepeatedly(ReturnRefOfCopy(std::string("test-guid")));
    EXPECT_CALL(item_, GetFileNameToReportUser())
        .WillRepeatedly(Return(base::FilePath(FILE_PATH_LITERAL("test.pdf"))));
    EXPECT_CALL(item_, GetTargetFilePath())
        .WillRepeatedly(
            ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("test.pdf"))));
    EXPECT_CALL(item_, GetURL())
        .WillRepeatedly(ReturnRefOfCopy(GURL("https://example.com/test.pdf")));
    EXPECT_CALL(item_, GetOriginalUrl())
        .WillRepeatedly(ReturnRefOfCopy(GURL("https://example.com/test.pdf")));
    EXPECT_CALL(item_, GetReferrerUrl())
        .WillRepeatedly(ReturnRefOfCopy(GURL()));
    EXPECT_CALL(item_, GetTabUrl())
        .WillRepeatedly(ReturnRefOfCopy(GURL("https://example.com/path")));
    EXPECT_CALL(item_, GetMimeType()).WillRepeatedly(Return("application/pdf"));
    EXPECT_CALL(item_, GetTotalBytes()).WillRepeatedly(Return(1024));
    EXPECT_CALL(item_, IsDangerous()).WillRepeatedly(Return(false));
    EXPECT_CALL(item_, IsTransient()).WillRepeatedly(Return(true));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  DownloadController controller_;
  TestMockDownloadItem item_;
};

TEST_F(DownloadControllerTest, ShouldShowAppVerificationPrompt) {
  EXPECT_FALSE(ShouldShowAppVerificationPrompt(item()));

  EXPECT_CALL(*item(), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  EXPECT_TRUE(ShouldShowAppVerificationPrompt(item()));

  {
    ScopedListPrefUpdate update(
        profile()->GetPrefs(), prefs::kDownloadAppVerificationPromptTimestamps);
    update->Append(base::TimeToValue(base::Time::Now()));
    update->Append(base::TimeToValue(base::Time::Now()));
    update->Append(base::TimeToValue(base::Time::Now()));
  }

  EXPECT_FALSE(ShouldShowAppVerificationPrompt(item()));
}

TEST_F(DownloadControllerTest, CleanupOldTimestamps) {
  EXPECT_CALL(*item(), GetDangerType())
      .WillOnce(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));

  {
    ScopedListPrefUpdate update(
        profile()->GetPrefs(), prefs::kDownloadAppVerificationPromptTimestamps);
    base::Time long_ago = base::Time::Now() - base::Days(500);
    update->Append(base::TimeToValue(long_ago));
    update->Append(base::TimeToValue(long_ago));
    update->Append(base::TimeToValue(long_ago));
  }

  EXPECT_TRUE(ShouldShowAppVerificationPrompt(item()));

  EXPECT_THAT(profile()->GetPrefs()->GetList(
                  prefs::kDownloadAppVerificationPromptTimestamps),
              IsEmpty());
}

TEST_F(DownloadControllerTest,
       ShowDangerousDownloadDialog_NullWindowAndroid_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(safe_browsing::kMaliciousApkDownloadCheck);

  EXPECT_CALL(*item(), GetDownloadSource())
      .WillRepeatedly(Return(download::DownloadSource::NAVIGATION));

  EXPECT_FALSE(controller()->ShowDangerousDownloadDialog(item()));
}

TEST_F(DownloadControllerTest,
       ShowDangerousDownloadDialog_NullWindowAndroid_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(safe_browsing::kMaliciousApkDownloadCheck);

  EXPECT_CALL(*item(), GetDownloadSource())
      .WillRepeatedly(Return(download::DownloadSource::NAVIGATION));
  EXPECT_CALL(*item(), IsDone()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item(), GetGuid())
      .WillRepeatedly(ReturnRefOfCopy(std::string("test-guid")));
  EXPECT_CALL(*item(), GetFileNameToReportUser())
      .WillRepeatedly(Return(base::FilePath(FILE_PATH_LITERAL("test.apk"))));
  EXPECT_CALL(*item(), GetTotalBytes()).WillRepeatedly(Return(1024));
  EXPECT_CALL(*item(), GetTabUrl())
      .WillRepeatedly(ReturnRefOfCopy(GURL("https://example.com/path")));
  EXPECT_CALL(*item(), IsDangerous()).WillRepeatedly(Return(true));

  EXPECT_TRUE(controller()->ShowDangerousDownloadDialog(item()));
}

TEST_F(DownloadControllerTest,
       OnDownloadFailed_RemovesObserverWhenNotResumable) {
  StubDownloadInfoGetters();
  EXPECT_CALL(*item(), CanResume()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item(), RemoveObserver(controller()));

  OnDownloadFailed(item());
}

TEST_F(DownloadControllerTest, OnDownloadFailed_KeepsObserverWhenResumable) {
  StubDownloadInfoGetters();
  EXPECT_CALL(*item(), CanResume()).WillRepeatedly(Return(true));
  EXPECT_CALL(*item(), RemoveObserver(_)).Times(0);

  OnDownloadFailed(item());
}

TEST_F(DownloadControllerTest,
       OnDownloadUpdated_DangerousContent_RecordsWarningShownOnce) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(safe_browsing::kMaliciousApkDownloadCheck);
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*item(), IsTemporary()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item(), IsTransient()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item(), IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(*item(), GetState())
      .WillRepeatedly(Return(download::DownloadItem::IN_PROGRESS));
  EXPECT_CALL(*item(), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT));
  EXPECT_CALL(*item(), GetGuid())
      .WillRepeatedly(ReturnRefOfCopy(std::string("test-guid")));
  EXPECT_CALL(*item(), GetTargetFilePath())
      .WillRepeatedly(
          ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("/test.apk"))));
  EXPECT_CALL(*item(), GetURL())
      .WillRepeatedly(ReturnRefOfCopy(GURL("https://example.com/test.apk")));
  EXPECT_CALL(*item(), HasUserGesture()).WillRepeatedly(Return(true));

  OnDownloadUpdated(item());

  histogram_tester.ExpectUniqueSample(
      "Download.ShowedDownloadWarning",
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT, 1);
  histogram_tester.ExpectTotalCount(
      "SBClientDownload.Warning.FileType.Malicious.Shown", 1);
  histogram_tester.ExpectUniqueSample("SBClientDownload.CancelEphemeralWarning",
                                      /*kCancellationScheduled=*/0, 1);

  // Subsequent updates for the same download must not double-count Shown.
  OnDownloadUpdated(item());
  histogram_tester.ExpectUniqueSample(
      "Download.ShowedDownloadWarning",
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT, 1);
}

TEST_F(DownloadControllerTest,
       OnDownloadUpdated_TelemetryOnly_DangerousFileShowsDialog) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kMaliciousApkDownloadCheck,
      {{safe_browsing::kMaliciousApkDownloadCheckTelemetryOnly.name, "true"}});
  base::HistogramTester histogram_tester;

  const std::string kGuid = "test-guid";
  EXPECT_CALL(*item(), IsTemporary()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item(), IsTransient()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item(), IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(*item(), IsDone()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item(), GetState())
      .WillRepeatedly(Return(download::DownloadItem::IN_PROGRESS));
  EXPECT_CALL(*item(), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  EXPECT_CALL(*item(), GetDownloadSource())
      .WillRepeatedly(Return(download::DownloadSource::NAVIGATION));
  EXPECT_CALL(*item(), GetGuid()).WillRepeatedly(ReturnRefOfCopy(kGuid));
  EXPECT_CALL(*item(), GetFileNameToReportUser())
      .WillRepeatedly(Return(base::FilePath(FILE_PATH_LITERAL("test.apk"))));
  EXPECT_CALL(*item(), GetTargetFilePath())
      .WillRepeatedly(
          ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("/test.apk"))));
  EXPECT_CALL(*item(), GetTotalBytes()).WillRepeatedly(Return(1024));
  EXPECT_CALL(*item(), GetTabUrl())
      .WillRepeatedly(ReturnRefOfCopy(GURL("https://example.com/path")));
  EXPECT_CALL(*item(), GetURL())
      .WillRepeatedly(ReturnRefOfCopy(GURL("https://example.com/test.apk")));
  EXPECT_CALL(*item(), HasUserGesture()).WillRepeatedly(Return(true));

  OnDownloadUpdated(item());

  // Must not be treated as an ephemeral warning in telemetry-only mode.
  histogram_tester.ExpectTotalCount("SBClientDownload.CancelEphemeralWarning",
                                    0);
  histogram_tester.ExpectUniqueSample(
      "Download.ShowedDownloadWarning",
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE, 1);

  // Prove ShowDangerousDownloadDialog() was actually reached and queued `item`
  // in DangerousDownloadDialogBridge (would be null if IsEphemeralWarning()
  // had early-returned true).
  ASSERT_NE(nullptr, dangerous_download_bridge());
  EXPECT_CALL(*item(), ValidateDangerousDownload()).Times(1);
  dangerous_download_bridge()->Accepted(kGuid);
}
