// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"

#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_user_files {

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
        NOTREACHED_NORETURN();
    }
  }

 protected:
  Profile* profile() { return browser()->profile(); }

  CloudProvider CloudProvider() { return GetParam(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

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

IN_PROC_BROWSER_TEST_P(MigrationNotificationManagerTest,
                       ShowMigrationErrorNotification) {
  NotificationDisplayServiceTester display_service_tester(browser()->profile());
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  MigrationNotificationManager manager(profile());
  manager.ShowMigrationErrorNotification(CloudProvider(), /*message=*/"");
  // TODO(aidazolic): Uncomment when finished.
  // EXPECT_TRUE(
  //   display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));

  manager.CloseAll();
  EXPECT_FALSE(
      display_service_tester.GetNotification(kSkyVaultMigrationNotificationId));
}

INSTANTIATE_TEST_SUITE_P(LocalUserFiles,
                         MigrationNotificationManagerTest,
                         ::testing::Values(CloudProvider::kGoogleDrive,
                                           CloudProvider::kOneDrive),
                         MigrationNotificationManagerTest::ParamToName);

}  // namespace policy::local_user_files
