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
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/uuid.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/notification/download_notification_manager.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/common/pref_names.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#endif

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRefOfCopy;
using testing::ValuesIn;

namespace {

const char kPdfMimeType[] = "application/pdf";
const char kMp3MimeType[] = "audio/mpeg";

const base::FilePath::CharType kDownloadItemTargetPathString[] =
    FILE_PATH_LITERAL("/tmp/TITLE.bin");

}  // anonymous namespace

namespace test {

class DownloadItemNotificationTest : public testing::Test {
 public:
  DownloadItemNotificationTest() : profile_(nullptr) {}

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
        .WillByDefault(ReturnRefOfCopy(
            base::Uuid::GenerateRandomV4().AsLowercaseString()));
    ON_CALL(*download_item_, GetState())
        .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
    ON_CALL(*download_item_, IsDangerous()).WillByDefault(Return(false));
    ON_CALL(*download_item_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath("TITLE.bin")));
    ON_CALL(*download_item_, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(download_item_target_path));
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InstallChromeApp(const std::string& app_id) {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile_);
    WaitForAppServiceProxyReady(proxy);

    std::vector<apps::AppPtr> apps;
    apps::AppPtr app =
        std::make_unique<apps::App>(apps::AppType::kChromeApp, app_id);
    app->readiness = apps::Readiness::kReady;
    app->policy_ids = {app_id};
    apps.push_back(std::move(app));

    proxy->OnApps(std::move(apps), apps::AppType::kChromeApp,
                  /*should_notify_initialized=*/false);
  }
#endif

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;

  std::unique_ptr<NiceMock<download::MockDownloadItem>> download_item_;
  std::unique_ptr<DownloadNotificationManager> download_notification_manager_;
  raw_ptr<DownloadItemNotification> download_item_notification_;
  std::unique_ptr<NotificationDisplayServiceTester> service_tester_;
};

TEST_F(DownloadItemNotificationTest, ShowAndCloseNotification) {
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

TEST_F(DownloadItemNotificationTest, ShowAndCloseDangerousNotification) {
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

TEST_F(DownloadItemNotificationTest, PauseAndResumeNotification) {
  // Shows a notification
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();

  // Confirms that the notification is shown as a popup.
  EXPECT_EQ(1u, NotificationCount());

  // Pauses and makes sure the DownloadItem::Pause() is called.
  EXPECT_CALL(*download_item_, Pause()).Times(1);
  EXPECT_CALL(*download_item_, IsPaused()).WillRepeatedly(Return(true));
  download_item_notification_->Click(0, std::nullopt);
  download_item_->NotifyObserversDownloadUpdated();

  // Resumes and makes sure the DownloadItem::Resume() is called.
  EXPECT_CALL(*download_item_, Resume(true)).Times(1);
  EXPECT_CALL(*download_item_, IsPaused()).WillRepeatedly(Return(false));
  download_item_notification_->Click(0, std::nullopt);
  download_item_->NotifyObserversDownloadUpdated();
}

TEST_F(DownloadItemNotificationTest, OpenDownload) {
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
  download_item_notification_->Click(std::nullopt, std::nullopt);
}

TEST_F(DownloadItemNotificationTest, OpenWhenComplete) {
  // Shows a notification
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();

  EXPECT_CALL(*download_item_, OpenDownload()).Times(0);

  // Toggles open-when-complete (new value: true).
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true))
      .Times(1)
      .WillOnce(Return());
  download_item_notification_->Click(std::nullopt, std::nullopt);
  EXPECT_CALL(*download_item_, GetOpenWhenComplete())
      .WillRepeatedly(Return(true));

  // Toggles open-when-complete (new value: false).
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(false))
      .Times(1)
      .WillOnce(Return());
  download_item_notification_->Click(std::nullopt, std::nullopt);
  EXPECT_CALL(*download_item_, GetOpenWhenComplete())
      .WillRepeatedly(Return(false));

  // Sets open-when-complete.
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true))
      .Times(1)
      .WillOnce(Return());
  download_item_notification_->Click(std::nullopt, std::nullopt);
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

TEST_F(DownloadItemNotificationTest, DisablePopup) {
  CreateDownloadItemNotification();
  download_item_->NotifyObserversDownloadOpened();

  EXPECT_EQ(message_center::DEFAULT_PRIORITY, LookUpNotification()->priority());

  download_item_notification_->DisablePopup();

  // Priority is low.
  EXPECT_EQ(message_center::LOW_PRIORITY, LookUpNotification()->priority());

  // Downloading is completed.
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(*download_item_, IsDone()).WillRepeatedly(Return(true));
  download_item_->NotifyObserversDownloadUpdated();

  // Priority is increased by the download's completion.
  EXPECT_GT(LookUpNotification()->priority(), message_center::LOW_PRIORITY);
}

TEST_F(DownloadItemNotificationTest, DeepScanning) {
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
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
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
  download_item_notification_->Click(std::nullopt, std::nullopt);

  // Can be opened while scanning.
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
      R"(
        {
          "service_provider": "google",
          "enable": [{"url_list": ["*"], "tags": ["malware"]}],
          "block_until_verdict": 0
        }
      )");
  EXPECT_CALL(*download_item_, OpenDownload()).Times(1);
  EXPECT_EQ(u"TITLE.bin is being scanned.", GetStatusString());
  download_item_notification_->Click(std::nullopt, std::nullopt);

  // Scanning finished, warning.
  EXPECT_CALL(*download_item_, IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(*download_item_, GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING));
  EXPECT_CALL(*download_item_, OpenDownload()).Times(0);
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true)).Times(0);
  download_item_notification_->Click(std::nullopt, std::nullopt);

  // Scanning finished, blocked.
  EXPECT_CALL(*download_item_, IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(*download_item_, GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK));
  EXPECT_CALL(*download_item_, OpenDownload()).Times(0);
  EXPECT_CALL(*download_item_, SetOpenWhenComplete(true)).Times(0);
  download_item_notification_->Click(std::nullopt, std::nullopt);

  // Scanning finished, safe.
  EXPECT_CALL(*download_item_, IsDangerous()).WillRepeatedly(Return(false));
  EXPECT_CALL(*download_item_, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE));
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(*download_item_, OpenDownload()).Times(1);
  download_item_notification_->Click(std::nullopt, std::nullopt);
}

// Test that EDIT_WITH_MEDIA_APP is added for pdf file on CHROMEOS_ASH.
// It should not be added for other build configs.
TEST_F(DownloadItemNotificationTest, NotificationActionsForPdf) {
  ON_CALL(*download_item_, GetState)
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item_, IsDone).WillByDefault(Return(true));
  ON_CALL(*download_item_, GetMimeType).WillByDefault(Return(kPdfMimeType));

  CreateDownloadItemNotification();
  auto actions = GetExtraActions();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(base::Contains(*actions, DownloadCommands::EDIT_WITH_MEDIA_APP));
  EXPECT_EQ(u"Open and edit",
            GetCommandLabel(DownloadCommands::EDIT_WITH_MEDIA_APP));
#else
  EXPECT_FALSE(base::Contains(*actions, DownloadCommands::EDIT_WITH_MEDIA_APP));
#endif
}

// Test that OPEN_WITH_MEDIA_APP is added for audio file on CHROMEOS_ASH.
// It should not be added for other build configs.
TEST_F(DownloadItemNotificationTest, NotificationActionsForAudio) {
  ON_CALL(*download_item_, GetState)
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item_, IsDone).WillByDefault(Return(true));
  ON_CALL(*download_item_, GetMimeType).WillByDefault(Return(kMp3MimeType));

  CreateDownloadItemNotification();
  auto actions = GetExtraActions();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(base::Contains(*actions, DownloadCommands::OPEN_WITH_MEDIA_APP));
  EXPECT_EQ(u"Open", GetCommandLabel(DownloadCommands::OPEN_WITH_MEDIA_APP));
#else
  EXPECT_FALSE(base::Contains(*actions, DownloadCommands::OPEN_WITH_MEDIA_APP));
#endif
}

}  // namespace test
