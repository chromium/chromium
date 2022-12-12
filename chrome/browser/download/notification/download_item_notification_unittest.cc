// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_item_notification.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/notification/download_notification_manager.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/common/download_item_reroute_info.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#include "chromeos/startup/browser_init_params.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRefOfCopy;
using testing::ValuesIn;
using Provider = enterprise_connectors::FileSystemServiceProvider;
using RerouteInfo = enterprise_connectors::DownloadItemRerouteInfo;

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kGalleryAppPdfEditNotificationTextParamName[] = "text";
constexpr char kGalleryAppPdfEditNotificationTextParamValue[] =
    "testCommandLabel";
#endif

const base::FilePath kTestPdfFilePath("test.pdf");

const base::FilePath::CharType kDownloadItemTargetPathString[] =
    FILE_PATH_LITERAL("/tmp/TITLE.bin");

bool IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::features::
      IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()
      ->IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled();
#else
  return false;
#endif
}

}  // anonymous namespace

namespace test {

class DownloadItemNotificationTest : public testing::Test {
 public:
  explicit DownloadItemNotificationTest(
      bool
          is_holding_space_in_progress_downloads_notification_suppression_enabled)
      : profile_(nullptr) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kHoldingSpaceInProgressDownloadsNotificationSuppression,
        is_holding_space_in_progress_downloads_notification_suppression_enabled);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    auto init_params(crosapi::mojom::BrowserInitParams::New());
    init_params
        ->is_holding_space_in_progress_downloads_notification_suppression_enabled =
        is_holding_space_in_progress_downloads_notification_suppression_enabled;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
  }

  void SetUp() override {
    testing::Test::SetUp();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user");

    service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);

    download_notification_manager_ =
        std::make_unique<DownloadNotificationManager>(profile_);

    base::FilePath download_item_target_path(kDownloadItemTargetPathString);
    download_item_ = std::make_unique<NiceMock<download::MockDownloadItem>>();
    ON_CALL(*download_item_, GetId()).WillByDefault(Return(12345));
    ON_CALL(*download_item_, GetGuid())
        .WillByDefault(ReturnRefOfCopy(base::GenerateGUID()));
    ON_CALL(*download_item_, GetState())
        .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
    ON_CALL(*download_item_, IsDangerous()).WillByDefault(Return(false));
    ON_CALL(*download_item_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath("TITLE.bin")));
    ON_CALL(*download_item_, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(download_item_target_path));
    ON_CALL(*download_item_, GetRerouteInfo())
        .WillByDefault(ReturnRefOfCopy(RerouteInfo()));
    ON_CALL(*download_item_, GetDangerType())
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    ON_CALL(*download_item_, IsDone()).WillByDefault(Return(false));
    ON_CALL(*download_item_, GetURL())
        .WillByDefault(
            ReturnRefOfCopy(GURL("http://www.example.com/download.bin")));
    content::DownloadItemUtils::AttachInfoForTesting(download_item_.get(),
                                                     profile_, nullptr);
  }

  void TearDown() override {
    download_item_notification_ = nullptr;  // will be free'd in the manager.
    download_notification_manager_.reset();
    profile_manager_.reset();
    testing::Test::TearDown();
  }

 protected:
  std::string notification_id() const {
    return download_item_notification_->notification_->id();
  }

  std::unique_ptr<message_center::Notification> LookUpNotification() const {
    std::vector<message_center::Notification> notifications =
        service_tester_->GetDisplayedNotificationsForType(
            NotificationHandler::Type::TRANSIENT);
    for (const auto& notification : notifications) {
      if (notification.id() == download_item_notification_->GetNotificationId())
        return std::make_unique<message_center::Notification>(notification);
    }
    return nullptr;
  }

  size_t NotificationCount() const {
    return service_tester_
        ->GetDisplayedNotificationsForType(NotificationHandler::Type::TRANSIENT)
        .size();
  }

  void RemoveNotification() {
    service_tester_->RemoveNotification(
        NotificationHandler::Type::TRANSIENT,
        download_item_notification_->GetNotificationId(), false);
  }

  void CreateDownloadItemNotification() {
    offline_items_collection::ContentId id(
        OfflineItemUtils::GetDownloadNamespacePrefix(
            profile_->IsOffTheRecord()),
        download_item_->GetGuid());
    download_notification_manager_->OnNewDownloadReady(download_item_.get());

    download_item_notification_ =
        download_notification_manager_->items_.at(id).get();
  }

  std::u16string GetStatusString() {
    return download_item_notification_->GetStatusString();
  }

  std::unique_ptr<std::vector<DownloadCommands::Command>> GetExtraActions() {
    return download_item_notification_->GetExtraActions();
  }

  std::u16string GetCommandLabel(DownloadCommands::Command command) const {
    return download_item_notification_->GetCommandLabel(command);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;

  std::unique_ptr<NiceMock<download::MockDownloadItem>> download_item_;
  std::unique_ptr<DownloadNotificationManager> download_notification_manager_;
  raw_ptr<DownloadItemNotification> download_item_notification_;
  std::unique_ptr<NotificationDisplayServiceTester> service_tester_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::ScopedLacrosServiceTestHelper scoped_lacros_service_test_helper_;
#endif
};

class DownloadItemNotificationParameterizedTest
    : public DownloadItemNotificationTest,
      public testing::WithParamInterface<
          /*is_holding_space_in_progress_downloads_notification_suppression_enabled=*/
          bool> {
 public:
  DownloadItemNotificationParameterizedTest()
      : DownloadItemNotificationTest(
            /*is_holding_space_in_progress_downloads_notification_suppression_enabled=*/
            GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DownloadItemNotificationParameterizedTest,
    /*is_holding_space_in_progress_downloads_notification_suppression_enabled=*/
    testing::Bool());

TEST_P(DownloadItemNotificationParameterizedTest, ShowAndCloseNotification) {
  // This test is only relevant if holding space in-progress downloads
  // notification suppression is disabled. Otherwise the notification will be
  // suppressed.
  if (IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled())
    return;

  base::HistogramTester histograms;
  EXPECT_EQ(0u, NotificationCount());

  // Shows a notification.
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();
  EXPECT_EQ(1u, NotificationCount());

  // Makes sure the DownloadItem::Cancel() is not called.
  EXPECT_CALL(*download_item_, Cancel(_)).Times(0);
  // Closes it once.
  RemoveNotification();

  // Confirms that the notification is closed.
  EXPECT_EQ(0u, NotificationCount());

  // Makes sure the DownloadItem::Cancel() is never called.
  EXPECT_CALL(*download_item_, Cancel(_)).Times(0);

  // Not logged because the download is safe.
  histograms.ExpectTotalCount("Download.ShowedDownloadWarning", 0);
}

TEST_P(DownloadItemNotificationParameterizedTest,
       ShowAndCloseDangerousNotification) {
  base::HistogramTester histograms;
  EXPECT_CALL(*download_item_, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT));
  EXPECT_CALL(*download_item_, IsDangerous()).WillRepeatedly(Return(true));

  // Shows a notification
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();
  EXPECT_EQ(1u, NotificationCount());

  // Closes it once.
  RemoveNotification();

  // Confirms that the notification is closed.
  EXPECT_EQ(0u, NotificationCount());

  // The download warning showed histogram is logged.
  histograms.ExpectBucketCount("Download.ShowedDownloadWarning",
                               download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
                               1);
}

TEST_P(DownloadItemNotificationParameterizedTest, PauseAndResumeNotification) {
  // This test is only relevant if holding space in-progress downloads
  // notification suppression is disabled. Otherwise the notification will be
  // suppressed.
  if (IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled())
    return;

  // Shows a notification
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();

  // Confirms that the notification is shown as a popup.
  EXPECT_EQ(1u, NotificationCount());

  // Pauses and makes sure the DownloadItem::Pause() is called.
  EXPECT_CALL(*download_item_, Pause()).Times(1);
  EXPECT_CALL(*download_item_, IsPaused()).WillRepeatedly(Return(true));
  download_item_notification_->Click(0, absl::nullopt);
  download_item_->NotifyObserversDownloadUpdated();

  // Resumes and makes sure the DownloadItem::Resume() is called.
  EXPECT_CALL(*download_item_, Resume(true)).Times(1);
  EXPECT_CALL(*download_item_, IsPaused()).WillRepeatedly(Return(false));
  download_item_notification_->Click(0, absl::nullopt);
  download_item_->NotifyObserversDownloadUpdated();
}

TEST_P(DownloadItemNotificationParameterizedTest, OpenDownload) {
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(*download_item_, IsDone()).WillRepeatedly(Return(true));

  // Shows a notification.
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();
  download_item_->NotifyObserversDownloadUpdated();

  // Clicks and confirms that the OpenDownload() is called.
  EXPECT_CALL(*download_item_, OpenDownload()).Times(1);
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(_)).Times(0);
  download_item_notification_->Click(absl::nullopt, absl::nullopt);
}

TEST_P(DownloadItemNotificationParameterizedTest, OpenWhenComplete) {
  // This test is only relevant if holding space in-progress downloads
  // notification suppression is disabled. Otherwise the notification will be
  // suppressed.
  if (IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled())
    return;

  // Shows a notification
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();

  EXPECT_CALL(*download_item_, OpenDownload()).Times(0);

  // Toggles open-when-complete (new value: true).
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true))
      .Times(1)
      .WillOnce(Return());
  download_item_notification_->Click(absl::nullopt, absl::nullopt);
  EXPECT_CALL(*download_item_, GetOpenWhenComplete())
      .WillRepeatedly(Return(true));

  // Toggles open-when-complete (new value: false).
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(false))
      .Times(1)
      .WillOnce(Return());
  download_item_notification_->Click(absl::nullopt, absl::nullopt);
  EXPECT_CALL(*download_item_, GetOpenWhenComplete())
      .WillRepeatedly(Return(false));

  // Sets open-when-complete.
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true))
      .Times(1)
      .WillOnce(Return());
  download_item_notification_->Click(absl::nullopt, absl::nullopt);
  EXPECT_CALL(*download_item_, GetOpenWhenComplete())
      .WillRepeatedly(Return(true));

  // Downloading is completed.
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(*download_item_, IsDone()).WillRepeatedly(Return(true));
  download_item_->NotifyObserversDownloadUpdated();

  // DownloadItem::OpenDownload must not be called since the file opens
  // automatically due to the open-when-complete flag.
}

TEST_P(DownloadItemNotificationParameterizedTest, DisablePopup) {
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();

  // If holding space in-progress downloads notification suppression is enabled,
  // the notification is expected to have been suppressed.
  if (!IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled()) {
    EXPECT_EQ(message_center::DEFAULT_PRIORITY,
              LookUpNotification()->priority());
  } else {
    EXPECT_EQ(0u, NotificationCount());
  }

  download_item_notification_->DisablePopup();

  // If holding space in-progress downloads notification suppression is enabled,
  // the notification is expected to have been suppressed.
  if (!IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled()) {
    // Priority is low.
    EXPECT_EQ(message_center::LOW_PRIORITY, LookUpNotification()->priority());
  } else {
    EXPECT_EQ(0u, NotificationCount());
  }

  // Downloading is completed.
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(*download_item_, IsDone()).WillRepeatedly(Return(true));
  download_item_->NotifyObserversDownloadUpdated();

  // Priority is increased by the download's completion.
  EXPECT_GT(LookUpNotification()->priority(), message_center::LOW_PRIORITY);
}

TEST_P(DownloadItemNotificationParameterizedTest, DeepScanning) {
  // Setup deep scanning in progress.
  EXPECT_CALL(*download_item_, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING));
  auto state =
      std::make_unique<ChromeDownloadManagerDelegate::SafeBrowsingState>();
  download_item_->SetUserData(&ChromeDownloadManagerDelegate::
                                  SafeBrowsingState::kSafeBrowsingUserDataKey,
                              std::move(state));
  CreateDownloadItemNotification();

  // Can't open while scanning.
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                      enterprise_connectors::FILE_DOWNLOADED,
                                      R"(
        {
          "service_provider": "google",
          "enable": [{"url_list": ["*"], "tags": ["malware"]}],
          "block_until_verdict": 1
        }
      )");
  EXPECT_CALL(*download_item_, OpenDownload()).Times(0);
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true)).Times(1);
  EXPECT_EQ(u"TITLE.bin is being scanned.", GetStatusString());
  download_item_notification_->Click(absl::nullopt, absl::nullopt);

  // Can be opened while scanning.
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                      enterprise_connectors::FILE_DOWNLOADED,
                                      R"(
        {
          "service_provider": "google",
          "enable": [{"url_list": ["*"], "tags": ["malware"]}],
          "block_until_verdict": 0
        }
      )");
  EXPECT_CALL(*download_item_, OpenDownload()).Times(1);
  EXPECT_EQ(u"TITLE.bin is being scanned.", GetStatusString());
  download_item_notification_->Click(absl::nullopt, absl::nullopt);

  // Scanning finished, warning.
  EXPECT_CALL(*download_item_, IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(*download_item_, GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING));
  EXPECT_CALL(*download_item_, OpenDownload()).Times(0);
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true)).Times(0);
  download_item_notification_->Click(absl::nullopt, absl::nullopt);

  // Scanning finished, blocked.
  EXPECT_CALL(*download_item_, IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(*download_item_, GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK));
  EXPECT_CALL(*download_item_, OpenDownload()).Times(0);
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true)).Times(0);
  download_item_notification_->Click(absl::nullopt, absl::nullopt);

  // Scanning finished, safe.
  EXPECT_CALL(*download_item_, IsDangerous()).WillRepeatedly(Return(false));
  EXPECT_CALL(*download_item_, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE));
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(*download_item_, OpenDownload()).Times(1);
  download_item_notification_->Click(absl::nullopt, absl::nullopt);
}

// Verifies that download in-progress notifications are suppressed if and only
// if the holding space in-progress downloads notification suppression feature
// is enabled.
TEST_P(DownloadItemNotificationParameterizedTest,
       MaybeSuppressInProgressNotifications) {
  // Creates a download in-progress notification.
  CreateDownloadItemNotification();

  // Confirms that the notification is suppressed if and only if holding space
  // in-progress downloads notification suppression is enabled.
  EXPECT_EQ(IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled()
                ? 0u
                : 1u,
            NotificationCount());

  // Disabling popups should not override notification suppression.
  download_item_notification_->DisablePopup();
  EXPECT_EQ(IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled()
                ? 0u
                : 1u,
            NotificationCount());
}

// Verifies that download in-progress notifications are displayed even if the
// holding space in-progress downloads notification suppression feature is
// enabled if the underlying download is dangerous.
TEST_P(DownloadItemNotificationParameterizedTest,
       ShowInProgressNotificationsIfDangerous) {
  // Creates a download in-progress notification.
  CreateDownloadItemNotification();

  // Confirms that the notification is suppressed if and only if holding space
  // in-progress downloads notification suppression is enabled.
  EXPECT_EQ(IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled()
                ? 0u
                : 1u,
            NotificationCount());

  // The download becoming dangerous should cause the notification to be
  // displayed even if it was previously suppressed.
  ON_CALL(*download_item_, GetDangerType)
      .WillByDefault(Return(
          download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  ON_CALL(*download_item_, IsDangerous).WillByDefault(Return(true));
  download_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(1u, NotificationCount());

  // The download becoming non-dangerous should cause the notification to be
  // suppressed if an only if holding space in-progress downloads notification
  // suppression is enabled.
  ON_CALL(*download_item_, GetDangerType)
      .WillByDefault(Return(
          download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  ON_CALL(*download_item_, IsDangerous).WillByDefault(Return(false));
  download_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled()
                ? 0u
                : 1u,
            NotificationCount());
}

// Verifies that download in-progress notifications are displayed even if the
// holding space in-progress downloads notification suppression feature is
// enabled if the underlying download is insecure.
TEST_P(DownloadItemNotificationParameterizedTest,
       ShowInProgressNotificationsIfInsecure) {
  // Creates a download in-progress notification.
  CreateDownloadItemNotification();

  // Confirms that the notification is suppressed if and only if holding space
  // in-progress downloads notification suppression is enabled.
  EXPECT_EQ(IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled()
                ? 0u
                : 1u,
            NotificationCount());

  // The download becoming insecure should cause the notification to be
  // displayed even if it was previously suppressed.
  ON_CALL(*download_item_, GetInsecureDownloadStatus)
      .WillByDefault(
          Return(download::DownloadItem::InsecureDownloadStatus::WARN));
  ON_CALL(*download_item_, IsInsecure).WillByDefault(Return(true));
  download_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(1u, NotificationCount());

  // The download becoming secure should cause the notification to be
  // suppressed if an only if holding space in-progress downloads notification
  // suppression is enabled.
  ON_CALL(*download_item_, GetInsecureDownloadStatus)
      .WillByDefault(
          Return(download::DownloadItem::InsecureDownloadStatus::SAFE));
  ON_CALL(*download_item_, IsInsecure).WillByDefault(Return(false));
  download_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled()
                ? 0u
                : 1u,
            NotificationCount());
}

// Verifies that download complete notifications are displayed even if the
// holding space in-progress downloads notification suppression feature is
// enabled.
TEST_P(DownloadItemNotificationParameterizedTest, ShowCompleteNotifications) {
  // Creates a download in-progress notification.
  CreateDownloadItemNotification();

  // Confirms that the notification is suppressed if and only if holding space
  // in-progress downloads notification suppression is enabled.
  EXPECT_EQ(IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled()
                ? 0u
                : 1u,
            NotificationCount());

  // Completing the download should cause the notification to be displayed even
  // if it was previously suppressed.
  ON_CALL(*download_item_, GetState)
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item_, IsDone).WillByDefault(Return(true));
  download_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(1u, NotificationCount());
}

// Test that PLATFORM_ACTION is added for pdf file if
// kGalleryAppPdfEditNotification flag is enabled on CHROMEOS_ASH. It should not
// be added for other build configs.
TEST_P(DownloadItemNotificationParameterizedTest,
       GalleryAppPdfEditNotification) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kGalleryAppPdfEditNotification,
      {{kGalleryAppPdfEditNotificationTextParamName,
        kGalleryAppPdfEditNotificationTextParamValue}});
#endif

  ON_CALL(*download_item_, GetState)
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item_, IsDone).WillByDefault(Return(true));
  ON_CALL(*download_item_, GetTargetFilePath)
      .WillByDefault(testing::ReturnRef(kTestPdfFilePath));

  CreateDownloadItemNotification();
  auto actions = GetExtraActions();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(base::Contains(*actions, DownloadCommands::PLATFORM_OPEN));
  EXPECT_EQ(u"testCommandLabel",
            GetCommandLabel(DownloadCommands::PLATFORM_OPEN));
#else
  EXPECT_FALSE(base::Contains(*actions, DownloadCommands::PLATFORM_OPEN));
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test that PLATFORM_OPEN is not added if a user's default app for pdf file is
// not the Gallery app.
TEST_P(DownloadItemNotificationParameterizedTest,
       GalleryAppPdfEditNotificationDefaultNonGallery) {
  constexpr char kNonGalleryAppTaskId[] = "non-gallery-app|app|open";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kGalleryAppPdfEditNotification,
      {{kGalleryAppPdfEditNotificationTextParamName,
        kGalleryAppPdfEditNotificationTextParamValue}});

  base::Value::Dict suffix_dict;
  suffix_dict.Set(".pdf", kNonGalleryAppTaskId);
  profile_->GetTestingPrefService()->SetDict(prefs::kDefaultTasksBySuffix,
                                             std::move(suffix_dict));
  base::Value::Dict mime_dict;
  mime_dict.Set("application/pdf", kNonGalleryAppTaskId);
  profile_->GetTestingPrefService()->SetDict(prefs::kDefaultTasksByMimeType,
                                             std::move(mime_dict));

  ON_CALL(*download_item_, GetState)
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item_, IsDone).WillByDefault(Return(true));
  ON_CALL(*download_item_, GetTargetFilePath)
      .WillByDefault(testing::ReturnRef(kTestPdfFilePath));

  CreateDownloadItemNotification();
  auto actions = GetExtraActions();
  EXPECT_FALSE(base::Contains(*actions, DownloadCommands::PLATFORM_OPEN));
}
#endif

struct FileReroutedTestCase {
  download::DownloadItem::DownloadState state;
  download::DownloadInterruptReason reason;
  RerouteInfo reroute_info;
};

RerouteInfo MakeTestRerouteInfo(std::string file_id = std::string()) {
  RerouteInfo info;
  info.set_service_provider(Provider::BOX);
  if (!file_id.empty())
    info.mutable_box()->set_file_id(file_id);
  return info;
}

RerouteInfo MakeTestRerouteInfoWithError(const std::string& error_message) {
  RerouteInfo info;
  info.set_service_provider(Provider::BOX);
  info.mutable_box()->set_error_message(error_message);
  return info;
}

class DownloadItemNotificationFileReroutedParametrizedTest
    : public DownloadItemNotificationTest,
      public ::testing::WithParamInterface<std::tuple<
          /*is_holding_space_in_progress_downloads_notification_suppression_enabled=*/
          bool,
          FileReroutedTestCase>> {
 public:
  DownloadItemNotificationFileReroutedParametrizedTest()
      : DownloadItemNotificationTest(
            /*is_holding_space_in_progress_downloads_notification_suppression_enabled=*/
            std::get<0>(GetParam())) {}

 protected:
  const FileReroutedTestCase& GetTestCase() const {
    return std::get<1>(GetParam());
  }
};

TEST_P(DownloadItemNotificationFileReroutedParametrizedTest,
       CreateDownloadItemNotification) {
  RerouteInfo reroute_info;
  reroute_info.set_service_provider(Provider::BOX);

  // Setup file rerouted to Box info.
  EXPECT_CALL(*download_item_, GetRerouteInfo())
      .WillRepeatedly(ReturnRefOfCopy(GetTestCase().reroute_info));
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(GetTestCase().state));

  switch (GetTestCase().state) {
    case (download::DownloadItem::INTERRUPTED):
      EXPECT_CALL(*download_item_, GetLastReason())
          .WillRepeatedly(Return(GetTestCase().reason));
      break;
    case (download::DownloadItem::COMPLETE):
      EXPECT_CALL(*download_item_, IsDone()).WillRepeatedly(Return(true));
      [[fallthrough]];
    default:
      EXPECT_CALL(*download_item_, GetLastReason()).Times(0);
  }

  // Show the download item notification.
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();
}

const FileReroutedTestCase kFileReroutedTestCases[] = {
    {download::DownloadItem::DownloadState::IN_PROGRESS,
     download::DOWNLOAD_INTERRUPT_REASON_NONE, MakeTestRerouteInfo()},
    {download::DownloadItem::DownloadState::INTERRUPTED,
     download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT,
     MakeTestRerouteInfo()},
    {download::DownloadItem::DownloadState::INTERRUPTED,
     download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
     MakeTestRerouteInfoWithError("400 - \"item_name_invalid\"")},
    {download::DownloadItem::DownloadState::COMPLETE,
     download::DOWNLOAD_INTERRUPT_REASON_NONE, MakeTestRerouteInfo("13579")}};

INSTANTIATE_TEST_SUITE_P(
    ReroutedByFileSystemConnectorTest,
    DownloadItemNotificationFileReroutedParametrizedTest,
    testing::Combine(
        /*is_holding_space_in_progress_downloads_notification_suppression_enabled=*/
        testing::Bool(),
        ValuesIn(kFileReroutedTestCases)));

}  // namespace test
