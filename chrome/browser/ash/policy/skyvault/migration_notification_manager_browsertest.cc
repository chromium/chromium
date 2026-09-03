// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/webui_url_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/check_deref.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy::local_user_files {

namespace {

class NotificationDisplayEventObserver
    : public message_center::MessageCenterObserver {
 public:
  explicit NotificationDisplayEventObserver(std::string notification_id)
      : notification_id_(std::move(notification_id)) {
    observation_.Observe(message_center::MessageCenter::Get());
  }
  ~NotificationDisplayEventObserver() override = default;

  int event_count() const { return event_count_; }
  void Reset() { event_count_ = 0; }

  void OnNotificationAdded(const std::string& notification_id) override {
    CountEvent(notification_id);
  }
  void OnNotificationUpdated(const std::string& notification_id) override {
    CountEvent(notification_id);
  }

 private:
  void CountEvent(const std::string& notification_id) {
    if (notification_id == notification_id_) {
      ++event_count_;
    }
  }

  const std::string notification_id_;
  int event_count_ = 0;
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      observation_{this};
};

}  // namespace

// Tests the MigrationNotificationManager class, which is in charge of most
// SkyVault migration notifications and dialogs.
class MigrationNotificationManagerTest : public InProcessBrowserTest {
 public:
  MigrationNotificationManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kSkyVault,
                              ash::features::kSkyVaultV2},
        /*disabled_features=*/{});
  }
  ~MigrationNotificationManagerTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(manager());
  }

 protected:
  Profile* profile() { return browser()->GetProfile(); }

  std::string notification_id() {
    const user_manager::User& user = CHECK_DEREF(
        ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile()));
    return ash::CreateUserScopedNotificationId(kSkyVaultMigrationNotificationId,
                                               user.username_hash());
  }

  const message_center::Notification* GetNotification() {
    return message_center::MessageCenter::Get()->FindNotificationById(
        notification_id());
  }

  MigrationNotificationManager* manager() {
    return MigrationNotificationManagerFactory::GetInstance()
        ->GetForBrowserContext(profile());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

class MigrationNotificationManagerParamTest
    : public MigrationNotificationManagerTest,
      public ::testing::WithParamInterface<MigrationDestination> {
 public:
  MigrationNotificationManagerParamTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    switch (info.param) {
      case MigrationDestination::kGoogleDrive:
        return "google_drive";
      case MigrationDestination::kOneDrive:
        return "one_drive";
      case MigrationDestination::kNotSpecified:
      case MigrationDestination::kDelete:
        NOTREACHED();
    }
  }

 protected:
  MigrationDestination CloudProvider() { return GetParam(); }

  base::ScopedTempDir temp_dir_;
};

// Tests that a progress notification is shown, and closed when
// CloseNotifications() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest,
                       ShowMigrationProgressNotification) {
  EXPECT_FALSE(GetNotification());

  manager()->ShowMigrationProgressNotification(CloudProvider());
  EXPECT_TRUE(GetNotification());

  manager()->CloseNotifications();
  EXPECT_FALSE(GetNotification());
}

// Tests that a completed notification is shown, and closed when
// CloseNotifications() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest,
                       ShowMigrationCompletedNotification) {
  EXPECT_FALSE(GetNotification());

  manager()->ShowMigrationCompletedNotification(
      CloudProvider(),
      /*destination_path=*/base::FilePath());
  EXPECT_TRUE(GetNotification());

  manager()->CloseNotifications();
  EXPECT_FALSE(GetNotification());
}

// Tests that a deletion completed notification is shown, and closed when
// CloseNotifications() is called.
IN_PROC_BROWSER_TEST_F(MigrationNotificationManagerTest,
                       ShowDeletionCompletedNotification) {
  EXPECT_FALSE(GetNotification());

  manager()->ShowDeletionCompletedNotification();
  EXPECT_TRUE(GetNotification());

  manager()->CloseNotifications();
  EXPECT_FALSE(GetNotification());
}

// Tests that an error notification is shown, and closed when
// CloseNotifications() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest,
                       ShowMigrationErrorNotification_CloseNotifications) {
  EXPECT_FALSE(GetNotification());

  manager()->ShowMigrationErrorNotification(
      CloudProvider(), kUploadRootPrefix,
      /*error_log_path=*/
      base::FilePath(kErrorLogFileBasePath).Append(kErrorLogFileName));
  EXPECT_TRUE(GetNotification());

  manager()->CloseNotifications();
  EXPECT_FALSE(GetNotification());
}

// Tests that clicking on the "Review error log" button on an error notification
// correctly openes the passed path in the browser and closes the notification.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest,
                       ShowMigrationErrorNotification_ReviewErrorLog) {
  const base::FilePath error_log_path =
      temp_dir_.GetPath().Append(kErrorLogFileName);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(WriteFile(error_log_path,
                    "/home/chronos/user/MyFiles/Downloads/test_file.txt - "
                    "Something went wrong. Try again."));
    CHECK(base::PathExists(error_log_path));
  }

  EXPECT_FALSE(GetNotification());

  manager()->ShowMigrationErrorNotification(CloudProvider(), kUploadRootPrefix,
                                            error_log_path);
  EXPECT_TRUE(GetNotification());

  const GURL error_log_url = net::FilePathToFileURL(error_log_path);
  EXPECT_NE(
      error_log_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec());

  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      notification_id(), /*button_index=*/0);

  EXPECT_FALSE(GetNotification());
  EXPECT_EQ(
      error_log_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec());
}

// Tests that a policy configuration error notification is shown, and closed
// when CloseNotifications() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest,
                       ShowConfigurationErrorNotification) {
  EXPECT_FALSE(GetNotification());

  manager()->ShowConfigurationErrorNotification(CloudProvider());
  EXPECT_TRUE(GetNotification());

  manager()->CloseNotifications();
  EXPECT_FALSE(GetNotification());
}

// Tests that a sign in notification is shown once, even if multiple requests
// are called, and that closing it notifies all the requesters.
IN_PROC_BROWSER_TEST_F(MigrationNotificationManagerTest,
                       ShowSignInNotification_CloseByUser) {
  EXPECT_FALSE(GetNotification());

  NotificationDisplayEventObserver display_observer(notification_id());
  base::test::TestFuture<base::File::Error> sign_in_future_1;
  base::test::TestFuture<base::File::Error> sign_in_future_2;

  base::CallbackListSubscription subscription_1 =
      manager()->ShowOneDriveSignInNotification(sign_in_future_1.GetCallback());
  EXPECT_TRUE(GetNotification());
  EXPECT_EQ(display_observer.event_count(), 1);
  display_observer.Reset();

  base::CallbackListSubscription subscription_2 =
      manager()->ShowOneDriveSignInNotification(sign_in_future_2.GetCallback());
  EXPECT_TRUE(GetNotification());

  EXPECT_EQ(display_observer.event_count(), 0);

  // Cancel the sign in.
  message_center::MessageCenter::Get()->RemoveNotification(notification_id(),
                                                           /*by_user=*/true);
  EXPECT_FALSE(GetNotification());

  // Both callbacks should run.
  EXPECT_EQ(sign_in_future_1.Get(), base::File::Error::FILE_ERROR_FAILED);
  EXPECT_EQ(sign_in_future_2.Get(), base::File::Error::FILE_ERROR_FAILED);
}

// Tests that when a sign in notification is closed by CloseNotifications(), all
// requesters to sign in are notified.
IN_PROC_BROWSER_TEST_F(MigrationNotificationManagerTest,
                       ShowSignInNotification_CloseNotifications) {
  EXPECT_FALSE(GetNotification());

  NotificationDisplayEventObserver display_observer(notification_id());
  base::test::TestFuture<base::File::Error> sign_in_future_1;
  base::test::TestFuture<base::File::Error> sign_in_future_2;

  base::CallbackListSubscription subscription_1 =
      manager()->ShowOneDriveSignInNotification(sign_in_future_1.GetCallback());
  EXPECT_TRUE(GetNotification());
  EXPECT_EQ(display_observer.event_count(), 1);
  display_observer.Reset();

  base::CallbackListSubscription subscription_2 =
      manager()->ShowOneDriveSignInNotification(sign_in_future_2.GetCallback());
  EXPECT_TRUE(GetNotification());
  EXPECT_EQ(display_observer.event_count(), 0);

  manager()->CloseNotifications();
  EXPECT_FALSE(GetNotification());

  // Both callbacks should run.
  EXPECT_EQ(sign_in_future_1.Get(), base::File::Error::FILE_ERROR_FAILED);
  EXPECT_EQ(sign_in_future_2.Get(), base::File::Error::FILE_ERROR_FAILED);
}

// Tests that a migration dialog is shown, and closed when CloseDialog() is
// called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest, ShowDialog) {
  EXPECT_FALSE(LocalFilesMigrationDialog::GetDialog());

  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(ash::kChromeUILocalFilesMigrationURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  base::MockCallback<StartMigrationCallback> mock_cb;
  manager()->ShowMigrationInfoDialog(
      CloudProvider(), base::Time::Now() + base::Minutes(5), mock_cb.Get());

  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  ash::SystemWebDialogDelegate* dialog = LocalFilesMigrationDialog::GetDialog();
  EXPECT_TRUE(dialog);

  content::WebUI* web_ui = dialog->GetWebUIForTest();
  EXPECT_TRUE(web_ui);
  content::WebContents* web_contents = web_ui->GetWebContents();
  content::WebContentsDestroyedWatcher watcher(web_contents);

  manager()->CloseDialog();
  watcher.Wait();

  EXPECT_FALSE(LocalFilesMigrationDialog::GetDialog());
}

INSTANTIATE_TEST_SUITE_P(LocalUserFiles,
                         MigrationNotificationManagerParamTest,
                         ::testing::Values(MigrationDestination::kGoogleDrive,
                                           MigrationDestination::kOneDrive),
                         MigrationNotificationManagerParamTest::ParamToName);

}  // namespace policy::local_user_files
