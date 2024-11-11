// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_user_files {

// Tests the MigrationNotificationManager class, which is in charge of most
// SkyVault migration notifications and dialogs.
class MigrationNotificationManagerTest : public InProcessBrowserTest {
 public:
  MigrationNotificationManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSkyVault, features::kSkyVaultV2},
        /*disabled_features=*/{});
  }
  ~MigrationNotificationManagerTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    tester_ = std::make_unique<NotificationDisplayServiceTester>(profile());
    ASSERT_TRUE(manager());
  }

 protected:
  Profile* profile() { return browser()->profile(); }

  MigrationNotificationManager* manager() {
    return MigrationNotificationManagerFactory::GetInstance()
        ->GetForBrowserContext(profile());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
};

class MigrationNotificationManagerParamTest
    : public MigrationNotificationManagerTest,
      public ::testing::WithParamInterface<CloudProvider> {
 public:
  MigrationNotificationManagerParamTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    switch (info.param) {
      case CloudProvider::kGoogleDrive:
        return "google_drive";
      case CloudProvider::kOneDrive:
        return "one_drive";
      case CloudProvider::kNotSpecified:
        NOTREACHED();
    }
  }

 protected:
  CloudProvider CloudProvider() { return GetParam(); }

  base::ScopedTempDir temp_dir_;
};

// Tests that a progress notification is shown, and closed when
// CloseNotifications() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest,
                       ShowMigrationProgressNotification) {
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->ShowMigrationProgressNotification(CloudProvider());
  EXPECT_TRUE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->CloseNotifications();
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));
}

// Tests that a completed notification is shown, and closed when
// CloseNotifications() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest,
                       ShowMigrationCompletedNotification) {
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->ShowMigrationCompletedNotification(
      CloudProvider(),
      /*destination_path=*/base::FilePath());
  EXPECT_TRUE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->CloseNotifications();
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));
}

// Tests that an error notification is shown, and closed when
// CloseNotifications() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest,
                       ShowMigrationErrorNotification_CloseNotifications) {
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->ShowMigrationErrorNotification(
      CloudProvider(), kUploadRootPrefix,
      /*error_log_path=*/
      base::FilePath(kErrorLogFileBasePath).Append(kErrorLogFileName));
  EXPECT_TRUE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->CloseNotifications();
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));
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

  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->ShowMigrationErrorNotification(CloudProvider(), kUploadRootPrefix,
                                            error_log_path);
  EXPECT_TRUE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  const GURL error_log_url = net::FilePathToFileURL(error_log_path);
  EXPECT_NE(
      error_log_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec());

  tester_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                         kSkyVaultMigrationNotificationId, 0, std::nullopt);

  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));
  EXPECT_EQ(
      error_log_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec());
}

// Tests that a policy configuration error notification is shown, and closed
// when CloseNotifications() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest,
                       ShowConfigurationErrorNotification) {
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->ShowConfigurationErrorNotification(CloudProvider());
  EXPECT_TRUE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->CloseNotifications();
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));
}

// Tests that a sign in notification is shown once, even if multiple requests
// are called, and that closing it notifies all the requesters.
IN_PROC_BROWSER_TEST_F(MigrationNotificationManagerTest,
                       ShowSignInNotification_CloseByUser) {
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  // Check that only one notification is added.
  base::MockCallback<base::RepeatingClosure> cb;
  EXPECT_CALL(cb, Run).Times(1);
  tester_->SetNotificationAddedClosure(cb.Get());

  base::test::TestFuture<base::File::Error> sign_in_future_1;
  base::test::TestFuture<base::File::Error> sign_in_future_2;

  base::CallbackListSubscription subscription_1 =
      manager()->ShowOneDriveSignInNotification(sign_in_future_1.GetCallback());
  EXPECT_TRUE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  base::CallbackListSubscription subscription_2 =
      manager()->ShowOneDriveSignInNotification(sign_in_future_2.GetCallback());
  EXPECT_TRUE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  // Cancel the sign in.
  tester_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                              kSkyVaultMigrationNotificationId,
                              /*by_user=*/true,
                              /*silent=*/false);
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  // Both callbacks should run.
  EXPECT_EQ(sign_in_future_1.Get(), base::File::Error::FILE_ERROR_FAILED);
  EXPECT_EQ(sign_in_future_2.Get(), base::File::Error::FILE_ERROR_FAILED);
}

// Tests that when a sign in notification is closed by CloseNotifications(), all
// requesters to sign in are notified.
IN_PROC_BROWSER_TEST_F(MigrationNotificationManagerTest,
                       ShowSignInNotification_CloseNotifications) {
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  // Check that only one notification is added.
  base::MockCallback<base::RepeatingClosure> cb;
  EXPECT_CALL(cb, Run).Times(1);
  tester_->SetNotificationAddedClosure(cb.Get());

  base::test::TestFuture<base::File::Error> sign_in_future_1;
  base::test::TestFuture<base::File::Error> sign_in_future_2;

  base::CallbackListSubscription subscription_1 =
      manager()->ShowOneDriveSignInNotification(sign_in_future_1.GetCallback());
  EXPECT_TRUE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  base::CallbackListSubscription subscription_2 =
      manager()->ShowOneDriveSignInNotification(sign_in_future_2.GetCallback());
  EXPECT_TRUE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  manager()->CloseNotifications();
  EXPECT_FALSE(tester_->GetNotification(kSkyVaultMigrationNotificationId));

  // Both callbacks should run.
  EXPECT_EQ(sign_in_future_1.Get(), base::File::Error::FILE_ERROR_FAILED);
  EXPECT_EQ(sign_in_future_2.Get(), base::File::Error::FILE_ERROR_FAILED);
}

// Tests that a migration dialog is shown, and closed when CloseDialog() is
// called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerParamTest, ShowDialog) {
  EXPECT_FALSE(LocalFilesMigrationDialog::GetDialog());

  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUILocalFilesMigrationURL)));
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
                         ::testing::Values(CloudProvider::kGoogleDrive,
                                           CloudProvider::kOneDrive),
                         MigrationNotificationManagerParamTest::ParamToName);

}  // namespace policy::local_user_files
