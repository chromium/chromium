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

  base::ScopedTempDir user_data_dir_;
  base::FilePath from_dir_;
};

TEST_F(BrowserDataMigratorTest, IsMigrationRequiredOnWorker) {
  const std::string user_id_hash = "user";
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();

  // Lacros UDD does not exist.
  EXPECT_TRUE(BrowserDataMigrator::IsMigrationRequiredOnWorker(
      user_data_dir_path, user_id_hash));

  // Create lacros user data dir.
  ASSERT_TRUE(base::CreateDirectory(
      user_data_dir_path.Append("user").Append(kLacrosDir)));

  // Lacros UDD exists.
  EXPECT_FALSE(BrowserDataMigrator::IsMigrationRequiredOnWorker(
      user_data_dir_path, user_id_hash));
}

TEST_F(BrowserDataMigratorTest, CopyDirectory) {
  BrowserDataMigrator browser_data_migrator(from_dir_);
  const base::FilePath from_dir = user_data_dir_.GetPath().Append("user");
  const base::FilePath to_dir = user_data_dir_.GetPath().Append("to_dir");

  base::CreateSymbolicLink(user_data_dir_.GetPath().Append(kFirstRun),
                           from_dir.Append(kFirstRun));
  ASSERT_TRUE(browser_data_migrator.CopyDirectory(from_dir, to_dir));

  // Setup `from_dir` as below.
  // |- user/                   /* from_dir_ */
  //     |- file
  //     |- directory/
  //         |- file
  //         |- Downloads/file
  //     |- Downloads/file
  //     |- 'First Run'         /* symlink */
  //
  // Expected `to_dir` structure after `CopyDirectory()`.
  // |- to_dir/
  //     |- file
  //     |- directory
  //         |- file
  //         |- Downloads/file
  //     |- Downloads/file
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::PathExists(to_dir.Append(kFileName)));
  EXPECT_TRUE(base::PathExists(to_dir.Append(kDirName).Append(kFileName)));
  EXPECT_TRUE(base::PathExists(
      to_dir.Append(kDirName).Append(kDownloads).Append(kFileName)));
  EXPECT_TRUE(base::PathExists(to_dir.Append(kDownloads).Append(kFileName)));
  // Make sure that symlink does not get copied.
  EXPECT_FALSE(base::PathExists(to_dir.Append(kFirstRun)));
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

    browser_data_migrator.MigrateInternal();

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
