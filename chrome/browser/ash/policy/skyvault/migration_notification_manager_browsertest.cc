// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"

#include "base/notreached.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
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
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_user_files {

// Tests the MigrationNotificationManager class, which is in charge of most
// SkyVault migration notifications and dialogs.
class MigrationNotificationManagerTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<CloudProvider> {
 public:
  MigrationNotificationManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSkyVault, features::kSkyVaultV2},
        /*disabled_features=*/{});
  }
  ~MigrationNotificationManagerTest() override = default;

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
  Profile* profile() { return browser()->profile(); }

  CloudProvider CloudProvider() { return GetParam(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a progress notification is shown, and closed when CloseAll() is
// called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerTest,
                       ShowMigrationProgressNotification) {
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  MigrationNotificationManager manager(profile());
  manager.ShowMigrationProgressNotification(CloudProvider());
  EXPECT_TRUE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  manager.CloseAll();
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));
}

// Tests that a completed notification is shown, and closed when CloseAll() is
// called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerTest,
                       ShowMigrationCompletedNotification) {
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  MigrationNotificationManager manager(profile());
  manager.ShowMigrationCompletedNotification(
      CloudProvider(),
      /*destination_path=*/base::FilePath());
  EXPECT_TRUE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  manager.CloseAll();
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));
}

// Tests that an error notification is shown, and closed when CloseAll() is
// called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerTest,
                       ShowMigrationErrorNotification) {
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  MigrationNotificationManager manager(profile());
  manager.ShowMigrationErrorNotification(CloudProvider(), base::FilePath(),
                                         /*errors=*/{});
  EXPECT_TRUE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  manager.CloseAll();
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));
}

// Tests that a policy configuration error notification is shown, and closed
// when CloseAll() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerTest,
                       ShowConfigurationErrorNotification) {
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  MigrationNotificationManager manager(profile());
  manager.ShowConfigurationErrorNotification(CloudProvider());
  EXPECT_TRUE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  manager.CloseAll();
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));
}

// Tests that a migration dialog is shown, and closed when CloseAll() is called.
IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerTest, ShowDialog) {
  EXPECT_FALSE(LocalFilesMigrationDialog::GetDialog());

  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUILocalFilesMigrationURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  MigrationNotificationManager manager(profile());
  base::MockCallback<StartMigrationCallback> mock_cb;
  manager.ShowMigrationInfoDialog(
      CloudProvider(), base::TimeDelta(base::Minutes(5)), mock_cb.Get());

  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  ash::SystemWebDialogDelegate* dialog = LocalFilesMigrationDialog::GetDialog();
  EXPECT_TRUE(dialog);

  content::WebUI* web_ui = dialog->GetWebUIForTest();
  EXPECT_TRUE(web_ui);
  content::WebContents* web_contents = web_ui->GetWebContents();
  content::WebContentsDestroyedWatcher watcher(web_contents);

  manager.CloseAll();
  watcher.Wait();

  EXPECT_FALSE(LocalFilesMigrationDialog::GetDialog());
}

INSTANTIATE_TEST_SUITE_P(LocalUserFiles,
                         MigrationNotificationManagerTest,
                         ::testing::Values(CloudProvider::kGoogleDrive,
                                           CloudProvider::kOneDrive),
                         MigrationNotificationManagerTest::ParamToName);

}  // namespace policy::local_user_files
