// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;

namespace ash {

namespace {
constexpr char kFileName[] = "file";
constexpr char kDirName[] = "directory";
constexpr char kFileData[] = "Hello";
constexpr int kFileSize = sizeof(kFileData);
constexpr char kDownloads[] = "Downloads";
}  // namespace

class BrowserDataMigratorTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Setup from_dir_ as below.
    // |- file
    // |- directory
    //    |- file
    //    |- Downloads/file
    // |- Downloads/file
    ASSERT_TRUE(from_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateDirectory(from_dir_.GetPath().Append(kDirName)));
    ASSERT_TRUE(base::CreateDirectory(
        from_dir_.GetPath().Append(kDirName).Append(kDownloads)));
    ASSERT_TRUE(base::CreateDirectory(from_dir_.GetPath().Append(kDownloads)));
    ASSERT_TRUE(base::WriteFile(from_dir_.GetPath().Append(kFileName),
                                kFileData, kFileSize));
    ASSERT_TRUE(
        base::WriteFile(from_dir_.GetPath().Append(kDirName).Append(kFileName),
                        kFileData, kFileSize));
    ASSERT_TRUE(base::WriteFile(from_dir_.GetPath()
                                    .Append(kDirName)
                                    .Append(kDownloads)
                                    .Append(kFileName),
                                kFileData, kFileSize));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.GetPath().Append(kDownloads).Append(kFileName), kFileData,
        kFileSize));
  }

  void TearDown() override { EXPECT_TRUE(from_dir_.Delete()); }

 protected:
  base::ScopedTempDir from_dir_;
};

TEST_F(BrowserDataMigratorTest, IsMigrationRequiredOnUI) {
  FakeChromeUserManager fake_user_manager;

  {
    // If lacros is disabled, IsMigrationRequiredOnUI should return false even
    // for regular users.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(chromeos::features::kLacrosSupport);

    const User* regular_user =
        fake_user_manager.AddUser(AccountId::FromUserEmail("user1@test.com"));

    EXPECT_FALSE(BrowserDataMigrator::IsMigrationRequiredOnUI(regular_user));
  }

  {
    // Lacros migration should happen only for regular users since only regular
    // user is guaranteed to have profile data directory at `u-<hash>`.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);

    const User* regular_user =
        fake_user_manager.AddUser(AccountId::FromUserEmail("user1@test.com"));
    const User* guest_user = fake_user_manager.AddGuestUser();
    const User* kiosk_user = fake_user_manager.AddKioskAppUser(
        AccountId::FromUserEmail("user2@test.com"));
    const User* arc_kiosk_user = fake_user_manager.AddArcKioskAppUser(
        AccountId::FromUserEmail("user3@test.com"));
    const User* web_kiosk_user = fake_user_manager.AddWebKioskAppUser(
        AccountId::FromUserEmail("user4@test.com"));
    const User* public_account_user = fake_user_manager.AddPublicAccountUser(
        AccountId::FromUserEmail("user5@test.com"));
    const User* active_directory_user =
        fake_user_manager.AddActiveDirectoryUser(
            AccountId::AdFromObjGuid("f04557de-5da2-40ce-ae9d-b8874d8da96e"));

    EXPECT_TRUE(BrowserDataMigrator::IsMigrationRequiredOnUI(regular_user));
    EXPECT_FALSE(BrowserDataMigrator::IsMigrationRequiredOnUI(guest_user));
    EXPECT_FALSE(BrowserDataMigrator::IsMigrationRequiredOnUI(kiosk_user));
    EXPECT_FALSE(BrowserDataMigrator::IsMigrationRequiredOnUI(arc_kiosk_user));
    EXPECT_FALSE(BrowserDataMigrator::IsMigrationRequiredOnUI(web_kiosk_user));
    EXPECT_FALSE(
        BrowserDataMigrator::IsMigrationRequiredOnUI(public_account_user));
    EXPECT_FALSE(
        BrowserDataMigrator::IsMigrationRequiredOnUI(active_directory_user));
  }
}

TEST_F(BrowserDataMigratorTest, IsMigrationRequiredOnWorker) {
  BrowserDataMigrator browser_data_migrator(from_dir_.GetPath());

  // If |BrowserDataMigrator::to_dir_| does not exist, run migration.
  EXPECT_TRUE(browser_data_migrator.IsMigrationRequiredOnWorker());

  // Create |BrowserDataMigrator::to_dir_|.
  ASSERT_TRUE(
      base::CreateDirectory(from_dir_.GetPath().Append(kLacrosProfileDir)));

  // If |BrowserDataMigrator::to_dir_| already exists, do not run migration.
  EXPECT_FALSE(browser_data_migrator.IsMigrationRequiredOnWorker());

  ASSERT_TRUE(base::DeletePathRecursively(from_dir_.GetPath()));

  // If |BrowserDataMigrator::from_dir_| does not exist, do not run migration.
  EXPECT_FALSE(browser_data_migrator.IsMigrationRequiredOnWorker());
}

TEST_F(BrowserDataMigratorTest, GetTargetInfo) {
  BrowserDataMigrator browser_data_migrator(from_dir_.GetPath());

  BrowserDataMigrator::TargetInfo target_info =
      browser_data_migrator.GetTargetInfo();

  EXPECT_EQ(target_info.total_byte_count,
            kFileSize * 3 /* expect three files */);

  const base::FilePath expected_file_path =
      from_dir_.GetPath().Append(kFileName);
  const base::FilePath expected_dir_path = from_dir_.GetPath().Append(kDirName);

  ASSERT_EQ(target_info.file_paths.size(), 1);
  EXPECT_EQ(target_info.file_paths[0], expected_file_path);

  ASSERT_EQ(target_info.dir_paths.size(), 1);
  EXPECT_EQ(target_info.dir_paths[0], expected_dir_path);
}

TEST_F(BrowserDataMigratorTest, Migrate) {
  BrowserDataMigrator browser_data_migrator(from_dir_.GetPath());

  BrowserDataMigrator::TargetInfo target_info =
      browser_data_migrator.GetTargetInfo();

  ASSERT_TRUE(browser_data_migrator.CopyToTmpDir(target_info));
  ASSERT_TRUE(browser_data_migrator.MoveTmpToTargetDir());

  // Expected dir structure.
  //  |- Downloads/file
  //  |- lacros/Default
  //      |- file
  //      |- directory
  //         |- file
  //         |- Downloads/file

  EXPECT_TRUE(base::PathExists(
      from_dir_.GetPath().Append(kDownloads).Append(kFileName)));
  EXPECT_TRUE(base::PathExists(
      from_dir_.GetPath().Append(kLacrosProfileDir).Append(kFileName)));
  EXPECT_TRUE(base::PathExists(from_dir_.GetPath()
                                   .Append(kLacrosProfileDir)
                                   .Append(kDirName)
                                   .Append(kFileName)));
  EXPECT_TRUE(base::PathExists(from_dir_.GetPath()
                                   .Append(kLacrosProfileDir)
                                   .Append(kDirName)
                                   .Append(kDownloads)
                                   .Append(kFileName)));
}
}  // namespace ash
