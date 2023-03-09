// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/copy_migrator.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/fake_migration_progress_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kDataFile[] = "data";
constexpr char kDataContent[] = "{test:'data'}";
constexpr int kFileSize = sizeof(kDataContent);
constexpr char kFirstRun[] = "First Run";
constexpr char kCache[] = "Cache";
constexpr char kFullRestoreData[] = "FullRestoreData";
constexpr char kDownloads[] = "Downloads";
constexpr char kCookies[] = "Cookies";
constexpr char kBookmarks[] = "Bookmarks";
constexpr char kAffiliationDatabase[] = "Affiliation Database";

}  // namespace

class CopyMigratorTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Setup `user_data_dir_` as below.
    // ./                             /* user_data_dir_ */
    // |- 'First Run'
    // |- user/                       /* from_dir_ */
    //     |- Cache                   /* deletable */
    //     |- Downloads/data          /* ash */
    //     |- FullRestoreData         /* ash */
    //     |- Bookmarks               /* lacros */
    //     |- Cookies                 /* lacros */
    //     |- Affiliation Database/  /* common */
    //         |- data
    //         |- Downloads/data

    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    from_dir_ = user_data_dir_.GetPath().Append("user");

    ASSERT_TRUE(base::WriteFile(
        user_data_dir_.GetPath().Append(kFirstRun) /* .../'First Run' */,
        base::StringPiece()));
    ASSERT_TRUE(base::CreateDirectory(
        from_dir_.Append(kDownloads) /* .../user/Downloads/ */));
    ASSERT_TRUE(base::CreateDirectory(from_dir_.Append(
        kAffiliationDatabase) /* .../user/Affiliation Database/ */));
    ASSERT_TRUE(base::CreateDirectory(
        from_dir_.Append(kAffiliationDatabase)
            .Append(
                kDownloads) /* .../user/Affiliation Database/Downloads/ */));
    ASSERT_TRUE(base::WriteFile(from_dir_.Append(kCache) /* .../user/Cache/ */,
                                kDataContent));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.Append(kFullRestoreData) /* .../user/FullRestoreData/ */,
        kDataContent));
    ASSERT_TRUE(
        base::WriteFile(from_dir_.Append(kDownloads)
                            .Append(kDataFile) /* .../user/Downloads/data */,
                        kDataContent));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.Append(kCookies) /* .../user/Cookies */, kDataContent));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.Append(kBookmarks) /* .../user/Bookmarks */, kDataContent));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.Append(kAffiliationDatabase)
            .Append(kDataFile) /* .../user/Affiliation Database/data */,
        kDataContent));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.Append(kAffiliationDatabase)
            .Append(kDownloads)
            .Append(
                kDataFile) /* .../user/Affiliation Database/Downloads/data */,
        kDataContent));
  }

  void TearDown() override { EXPECT_TRUE(user_data_dir_.Delete()); }

  base::ScopedTempDir user_data_dir_;
  base::FilePath from_dir_;
};

TEST_F(CopyMigratorTest, SetupTmpDir) {
  base::FilePath tmp_dir =
      from_dir_.Append(browser_data_migrator_util::kTmpDir);
  scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag =
      base::MakeRefCounted<browser_data_migrator_util::CancelFlag>();
  browser_data_migrator_util::TargetItems lacros_items =
      browser_data_migrator_util::GetTargetItems(
          from_dir_, browser_data_migrator_util::ItemType::kLacros);
  browser_data_migrator_util::TargetItems need_copy_items =
      browser_data_migrator_util::GetTargetItems(
          from_dir_, browser_data_migrator_util::ItemType::kNeedCopyForCopy);
  FakeMigrationProgressTracker progress_tracker;
  EXPECT_TRUE(CopyMigrator::SetupTmpDir(lacros_items, need_copy_items, tmp_dir,
                                        cancel_flag.get(), &progress_tracker));

  EXPECT_TRUE(base::PathExists(tmp_dir));
  EXPECT_TRUE(base::PathExists(tmp_dir.Append(kFirstRun)));
  EXPECT_TRUE(base::PathExists(
      tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath)));
  EXPECT_TRUE(base::PathExists(
      tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath)
          .Append(kBookmarks)));
  EXPECT_TRUE(base::PathExists(
      tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath)
          .Append(kCookies)));
  EXPECT_TRUE(base::PathExists(
      tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath)
          .Append(kAffiliationDatabase)));
  EXPECT_TRUE(base::PathExists(
      tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath)
          .Append(kAffiliationDatabase)
          .Append(kDataFile)));
  EXPECT_TRUE(base::PathExists(
      tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath)
          .Append(kAffiliationDatabase)
          .Append(kDownloads)
          .Append(kDataFile)));
}

TEST_F(CopyMigratorTest, CancelSetupTmpDir) {
  base::FilePath tmp_dir =
      from_dir_.Append(browser_data_migrator_util::kTmpDir);
  scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag =
      base::MakeRefCounted<browser_data_migrator_util::CancelFlag>();
  FakeMigrationProgressTracker progress_tracker;
  browser_data_migrator_util::TargetItems lacros_items =
      browser_data_migrator_util::GetTargetItems(
          from_dir_, browser_data_migrator_util::ItemType::kLacros);
  browser_data_migrator_util::TargetItems need_copy_items =
      browser_data_migrator_util::GetTargetItems(
          from_dir_, browser_data_migrator_util::ItemType::kNeedCopyForCopy);

  // Set cancel_flag to cancel migrationl.
  cancel_flag->Set();
  EXPECT_FALSE(CopyMigrator::SetupTmpDir(lacros_items, need_copy_items, tmp_dir,
                                         cancel_flag.get(), &progress_tracker));

  // These files should not exist.
  EXPECT_FALSE(base::PathExists(tmp_dir.Append(kFirstRun)));
  EXPECT_FALSE(base::PathExists(
      tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath)
          .Append(kBookmarks)));
  EXPECT_FALSE(base::PathExists(
      tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath)
          .Append(kCookies)));
}

TEST_F(CopyMigratorTest, MigrateInternal) {
  base::HistogramTester histogram_tester;

  {
    scoped_refptr<browser_data_migrator_util::CancelFlag> cancelled =
        base::MakeRefCounted<browser_data_migrator_util::CancelFlag>();
    std::unique_ptr<MigrationProgressTracker> progress_tracker =
        std::make_unique<FakeMigrationProgressTracker>();
    CopyMigrator::MigrateInternal(from_dir_, std::move(progress_tracker),
                                  cancelled);

    // Expected dir structure after migration.
    // ./                             /* user_data_dir_ */
    // |- 'First Run'
    // |- user/                       /* from_dir_ */
    //     |- Cache                   /* no copy */
    //     |- Downloads/data          /* ash */
    //     |- FullRestoreData         /* ash */
    //     |- Bookmarks               /* lacros */
    //     |- Cookies                 /* common */
    //     |- Affiliation Database/   /* common */
    //         |- data
    //         |- Downloads/data
    //     |- lacros/
    //         |- `First Run`
    //         |- Default
    //             |- Bookmarks
    //             |- Cookies
    //             |- Affiliation Database/
    //                 |- data
    //                 |- Downloads/data

    const base::FilePath new_user_data_dir =
        from_dir_.Append(browser_data_migrator_util::kLacrosDir);
    const base::FilePath new_profile_data_dir =
        new_user_data_dir.Append("Default");
    EXPECT_TRUE(base::PathExists(new_user_data_dir.Append(kFirstRun)));
    EXPECT_TRUE(base::PathExists(new_profile_data_dir.Append(kBookmarks)));
    EXPECT_TRUE(base::PathExists(new_profile_data_dir.Append(kCookies)));
    EXPECT_TRUE(base::PathExists(
        new_profile_data_dir.Append(kAffiliationDatabase).Append(kDataFile)));
    // Check that cache is not migrated.
    EXPECT_FALSE(base::PathExists(new_profile_data_dir.Append(kCache)));
    // Check that data that belongs to ash is not migrated.
    EXPECT_FALSE(base::PathExists(
        new_profile_data_dir.Append(kDownloads).Append(kDataFile)));
  }

  histogram_tester.ExpectTotalCount(kFinalStatus, 1);
  histogram_tester.ExpectTotalCount(kCopiedDataSize, 1);
  histogram_tester.ExpectTotalCount(kLacrosDataSize, 1);
  histogram_tester.ExpectTotalCount(kCommonDataSize, 1);
  histogram_tester.ExpectTotalCount(kTotalTime, 1);
  histogram_tester.ExpectTotalCount(kLacrosDataTime, 1);
  histogram_tester.ExpectTotalCount(kCommonDataTime, 1);
  histogram_tester.ExpectTotalCount(kCreateDirectoryFail, 0);

  histogram_tester.ExpectBucketCount(kFinalStatus,
                                     CopyMigrator::FinalStatus::kSuccess, 1);
  histogram_tester.ExpectBucketCount(kCopiedDataSize,
                                     kFileSize * 4 / (1024 * 1024), 1);
}

TEST_F(CopyMigratorTest, MigrateInternalOutOfDisk) {
  // Emulate the situation of out-of-disk.
  browser_data_migrator_util::ScopedExtraBytesRequiredToBeFreedForTesting
      scoped_extra_bytes(100);

  // Run the migration.
  auto result = CopyMigrator::MigrateInternal(
      from_dir_, std::make_unique<FakeMigrationProgressTracker>(),
      base::MakeRefCounted<browser_data_migrator_util::CancelFlag>());

  EXPECT_EQ(BrowserDataMigrator::ResultKind::kFailed,
            result.data_migration_result.kind);
  EXPECT_EQ(100u, result.data_migration_result.required_size);
}

}  // namespace ash
