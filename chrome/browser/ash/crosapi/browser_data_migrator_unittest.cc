// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
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
constexpr char kFirstRun[] = "First Run";
}  // namespace

class BrowserDataMigratorTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Setup `user_data_dir_` as below.
    // ./                         /* user_data_dir_ */
    // |- 'First Run'
    // |- user/                   /* from_dir_ */
    //     |- file
    //     |- directory/
    //         |- file
    //         |- Downloads/file
    //     |- Downloads/file

    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    from_dir_ = user_data_dir_.GetPath().Append("user");

    ASSERT_TRUE(base::WriteFile(user_data_dir_.GetPath().Append(
                                    kFirstRun) /* .../'First Run' */,
                                "", 0) == 0);
    ASSERT_TRUE(base::CreateDirectory(
        from_dir_.Append(kDirName) /* .../user/directory/ */));
    ASSERT_TRUE(base::CreateDirectory(from_dir_.Append(kDirName).Append(
        kDownloads) /* .../user/directory/Downloads/ */));
    ASSERT_TRUE(base::CreateDirectory(
        from_dir_.Append(kDownloads) /* .../user/Downloads/ */));
    ASSERT_TRUE(
        base::WriteFile(from_dir_.Append(kFileName) /* .../user/file/ */,
                        kFileData, kFileSize));
    ASSERT_TRUE(base::WriteFile(from_dir_.Append(kDirName).Append(
                                    kFileName) /* .../user/directory/file/ */,
                                kFileData, kFileSize));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.Append(kDirName)
            .Append(kDownloads)
            .Append(kFileName) /* .../user/directory/Downloads/file/ */,
        kFileData, kFileSize));
    ASSERT_TRUE(
        base::WriteFile(from_dir_.Append(kDownloads)
                            .Append(kFileName) /* .../user/Downloads/file/ */,
                        kFileData, kFileSize));
  }

  void TearDown() override { EXPECT_TRUE(user_data_dir_.Delete()); }

 protected:
  base::ScopedTempDir user_data_dir_;
  base::FilePath from_dir_;
};

TEST_F(BrowserDataMigratorTest, IsMigrationRequiredOnUI) {
  FakeChromeUserManager fake_user_manager;

  {
    // If lacros is disabled, `IsMigrationRequiredOnUI()` should return false
    // even for regular users.
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

TEST_F(BrowserDataMigratorTest, IsDataWipeRequiredInvalid) {
  const base::Version data_version;
  const base::Version current{"3"};
  const base::Version required{"2"};

  ASSERT_FALSE(data_version.IsValid());
  EXPECT_TRUE(
      BrowserDataMigrator::IsDataWipeRequired(data_version, current, required));
}

TEST_F(BrowserDataMigratorTest, IsDataWipeRequiredFutureVersion) {
  const base::Version data_version{"1"};
  const base::Version current{"2"};
  const base::Version required{"3"};

  EXPECT_FALSE(
      BrowserDataMigrator::IsDataWipeRequired(data_version, current, required));
}

TEST_F(BrowserDataMigratorTest, IsDataWipeRequiredSameVersion) {
  const base::Version data_version{"3"};
  const base::Version current{"4"};
  const base::Version required{"3"};

  EXPECT_FALSE(
      BrowserDataMigrator::IsDataWipeRequired(data_version, current, required));
}

TEST_F(BrowserDataMigratorTest, IsDataWipeRequired) {
  const base::Version data_version{"1"};
  const base::Version current{"3"};
  const base::Version required{"2"};

  EXPECT_TRUE(
      BrowserDataMigrator::IsDataWipeRequired(data_version, current, required));
}

TEST_F(BrowserDataMigratorTest, IsDataWipeRequired2) {
  const base::Version data_version{"1"};
  const base::Version current{"3"};
  const base::Version required{"3"};

  EXPECT_TRUE(
      BrowserDataMigrator::IsDataWipeRequired(data_version, current, required));
}

TEST_F(BrowserDataMigratorTest, IsMigrationRequiredOnWorker) {
  BrowserDataMigrator browser_data_migrator(from_dir_);

  // If `BrowserDataMigrator::to_dir_` does not exist, run migration.
  EXPECT_TRUE(browser_data_migrator.IsMigrationRequiredOnWorker());

  // Create `BrowserDataMigrator::to_dir_`.
  ASSERT_TRUE(base::CreateDirectory(from_dir_.Append(kLacrosDir)));

  // If `BrowserDataMigrator::to_dir_` already exists, do not run migration.
  EXPECT_FALSE(browser_data_migrator.IsMigrationRequiredOnWorker());

  ASSERT_TRUE(base::DeletePathRecursively(from_dir_));

  // If `BrowserDataMigrator::from_dir_` does not exist, do not run migration.
  EXPECT_FALSE(browser_data_migrator.IsMigrationRequiredOnWorker());
}

TEST_F(BrowserDataMigratorTest, GetTargetInfo) {
  BrowserDataMigrator browser_data_migrator(from_dir_);

  BrowserDataMigrator::TargetInfo target_info =
      browser_data_migrator.GetTargetInfo();

  EXPECT_EQ(target_info.total_byte_count,
            kFileSize * 3 /* expect three files */);

  ASSERT_EQ(target_info.user_data_items.size(), 1);

  std::vector<BrowserDataMigrator::TargetItem> expected_user_data_items = {
      BrowserDataMigrator::TargetItem{
          from_dir_.DirName().Append(kFirstRun),
          BrowserDataMigrator::TargetItem::ItemType::kFile}};

  EXPECT_EQ(target_info.user_data_items[0], expected_user_data_items[0]);

  ASSERT_EQ(target_info.profile_data_items.size(), 2);

  std::vector<BrowserDataMigrator::TargetItem> expected_profile_data_items = {
      BrowserDataMigrator::TargetItem{
          from_dir_.Append(kDirName),
          BrowserDataMigrator::TargetItem::ItemType::kDirectory},
      BrowserDataMigrator::TargetItem{
          from_dir_.Append(kFileName),
          BrowserDataMigrator::TargetItem::ItemType::kFile}};

  std::sort(
      target_info.profile_data_items.begin(),
      target_info.profile_data_items.end(),
      [](BrowserDataMigrator::TargetItem i1,
         BrowserDataMigrator::TargetItem i2) { return i1.path < i2.path; });

  for (int i = 0; i < target_info.profile_data_items.size(); i++) {
    SCOPED_TRACE(target_info.profile_data_items[i].path);
    EXPECT_EQ(target_info.profile_data_items[i],
              expected_profile_data_items[i]);
  }
}

TEST_F(BrowserDataMigratorTest, RecordStatus) {
  {
    // If `FinalStatus::kSkipped`, only record the status and do not record
    // copied data size or total time.
    base::HistogramTester histogram_tester;

    BrowserDataMigrator::RecordStatus(
        BrowserDataMigrator::FinalStatus::kSkipped);

    histogram_tester.ExpectTotalCount(kFinalStatus, 1);
    histogram_tester.ExpectTotalCount(kCopiedDataSize, 0);
    histogram_tester.ExpectTotalCount(kTotalTime, 0);

    histogram_tester.ExpectBucketCount(
        kFinalStatus, BrowserDataMigrator::FinalStatus::kSkipped, 1);
  }

  {
    // If `FInalStatus::kSuccess`, the three UMA `kFinalStatus`,
    // `kCopiedDataSize`, `kTotalTime` should be recorded.
    base::HistogramTester histogram_tester;
    BrowserDataMigrator browser_data_migrator(from_dir_);

    BrowserDataMigrator::TargetInfo target_info;
    target_info.total_byte_count = /* 200 MBs */ 200 * 1024 * 1024;

    base::ElapsedTimer timer;

    BrowserDataMigrator::RecordStatus(
        BrowserDataMigrator::FinalStatus::kSuccess, &target_info, &timer);

    histogram_tester.ExpectTotalCount(kFinalStatus, 1);
    histogram_tester.ExpectTotalCount(kCopiedDataSize, 1);
    histogram_tester.ExpectTotalCount(kTotalTime, 1);

    histogram_tester.ExpectBucketCount(
        kFinalStatus, BrowserDataMigrator::FinalStatus::kSuccess, 1);
    histogram_tester.ExpectBucketCount(
        kCopiedDataSize, target_info.total_byte_count / (1024 * 1024), 1);
  }
}

TEST_F(BrowserDataMigratorTest, Migrate) {
  base::HistogramTester histogram_tester;

  {
    BrowserDataMigrator browser_data_migrator(from_dir_);

    browser_data_migrator.MigrateInternal(false /* is_data_wipe_required */);

    // Expected dir structure after migration.
    // ./                         /* user_data_dir_ */
    // |- 'First Run'
    // |- user/                   /* from_dir_ */
    //     |- Downloads/file
    //     |- lacros
    //         |- 'First Run'
    //         |- Default/
    //             |- file
    //             |- directory
    //                 |- file
    //                 |- Downloads/file

    EXPECT_FALSE(base::PathExists(from_dir_.Append(kLacrosDir)
                                      .Append("Default")
                                      .Append(kDownloads)
                                      .Append(kFileName)));
    const base::FilePath new_user_data_dir = from_dir_.Append(kLacrosDir);
    EXPECT_TRUE(base::PathExists(new_user_data_dir.Append(kFirstRun)));
    EXPECT_TRUE(
        base::PathExists(from_dir_.Append(kDownloads).Append(kFileName)));
    EXPECT_TRUE(base::PathExists(
        new_user_data_dir.Append("Default").Append(kFileName)));
    EXPECT_TRUE(base::PathExists(
        new_user_data_dir.Append("Default").Append(kDirName).Append(
            kFileName)));
    EXPECT_TRUE(base::PathExists(new_user_data_dir.Append("Default")
                                     .Append(kDirName)
                                     .Append(kDownloads)
                                     .Append(kFileName)));
  }  // `browser_data_migrator` is destructed and `RecordStatus()` is called.

  histogram_tester.ExpectTotalCount(kFinalStatus, 1);
  histogram_tester.ExpectTotalCount(kCopiedDataSize, 1);
  histogram_tester.ExpectTotalCount(kTotalTime, 1);
  histogram_tester.ExpectTotalCount(kCreateDirectoryFail, 0);

  histogram_tester.ExpectBucketCount(
      kFinalStatus, BrowserDataMigrator::FinalStatus::kSuccess, 1);
  histogram_tester.ExpectBucketCount(kCopiedDataSize,
                                     kFileSize * 3 / (1024 * 1024), 1);
}
}  // namespace ash
