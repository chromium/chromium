// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/fake_migration_progress_tracker.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;

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

struct TargetItemComparator {
  bool operator()(const BrowserDataMigratorImpl::TargetItem& t1,
                  const BrowserDataMigratorImpl::TargetItem& t2) const {
    return t1.path < t2.path;
  }
};

}  // namespace

class BrowserDataMigratorTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Setup `user_data_dir_` as below.
    // ./                             /* user_data_dir_ */
    // |- 'First Run'
    // |- user/                       /* from_dir_ */
    //     |- Cache                   /* no copy */
    //     |- Downloads/data          /* ash */
    //     |- FullRestoreData         /* ash */
    //     |- Bookmarks               /* lacros */
    //     |- Cookies                 /* lacros */
    //     |- Affiliation Database/  /* common */
    //         |- data
    //         |- Downloads/data

    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    from_dir_ = user_data_dir_.GetPath().Append("user");

    ASSERT_TRUE(base::WriteFile(user_data_dir_.GetPath().Append(
                                    kFirstRun) /* .../'First Run' */,
                                "", 0) == 0);
    ASSERT_TRUE(base::CreateDirectory(
        from_dir_.Append(kDownloads) /* .../user/Downloads/ */));
    ASSERT_TRUE(base::CreateDirectory(from_dir_.Append(
        kAffiliationDatabase) /* .../user/Affiliation Database/ */));
    ASSERT_TRUE(base::CreateDirectory(
        from_dir_.Append(kAffiliationDatabase)
            .Append(
                kDownloads) /* .../user/Affiliation Database/Downloads/ */));
    ASSERT_TRUE(base::WriteFile(from_dir_.Append(kCache) /* .../user/Cache/ */,
                                kDataContent, kFileSize));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.Append(kFullRestoreData) /* .../user/FullRestoreData/ */,
        kDataContent, kFileSize));
    ASSERT_TRUE(
        base::WriteFile(from_dir_.Append(kDownloads)
                            .Append(kDataFile) /* .../user/Downloads/data */,
                        kDataContent, kFileSize));
    ASSERT_TRUE(
        base::WriteFile(from_dir_.Append(kCookies) /* .../user/Cookies */,
                        kDataContent, kFileSize));
    ASSERT_TRUE(
        base::WriteFile(from_dir_.Append(kBookmarks) /* .../user/Bookmarks */,
                        kDataContent, kFileSize));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.Append(kAffiliationDatabase)
            .Append(kDataFile) /* .../user/Affiliation Database/data */,
        kDataContent, kFileSize));
    ASSERT_TRUE(base::WriteFile(
        from_dir_.Append(kAffiliationDatabase)
            .Append(kDownloads)
            .Append(
                kDataFile) /* .../user/Affiliation Database/Downloads/data */,
        kDataContent, kFileSize));

    BrowserDataMigratorImpl::RegisterLocalStatePrefs(pref_service_.registry());
    crosapi::browser_util::RegisterLocalStatePrefs(pref_service_.registry());
  }

  void TearDown() override { EXPECT_TRUE(user_data_dir_.Delete()); }

  base::ScopedTempDir user_data_dir_;
  base::FilePath from_dir_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(BrowserDataMigratorTest, ManipulateMigrationAttemptCount) {
  const std::string user_id_hash = "user";

  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            0);
  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            1);

  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            2);

  BrowserDataMigratorImpl::ClearMigrationAttemptCountForUser(&pref_service_,
                                                             user_id_hash);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            0);
}

TEST_F(BrowserDataMigratorTest, GetTargetInfo) {
  BrowserDataMigratorImpl::TargetInfo target_info =
      BrowserDataMigratorImpl::GetTargetInfo(from_dir_);

  EXPECT_EQ(target_info.ash_data_size, kFileSize * 2 /* expect two files */);
  EXPECT_EQ(target_info.no_copy_data_size, kFileSize /* expect one file */);
  EXPECT_EQ(target_info.lacros_data_size, kFileSize * 2 /* expect two files */);
  EXPECT_EQ(target_info.common_data_size, kFileSize * 2 /* expect two file */);

  // Check for ash data.
  std::vector<BrowserDataMigratorImpl::TargetItem> expected_ash_data_items = {
      {from_dir_.Append(kDownloads), kFileSize,
       BrowserDataMigratorImpl::TargetItem::ItemType::kDirectory},
      {from_dir_.Append(kFullRestoreData), kFileSize,
       BrowserDataMigratorImpl::TargetItem::ItemType::kFile}};
  std::sort(target_info.ash_data_items.begin(),
            target_info.ash_data_items.end(), TargetItemComparator());
  ASSERT_EQ(target_info.ash_data_items.size(), expected_ash_data_items.size());
  for (int i = 0; i < target_info.ash_data_items.size(); i++) {
    SCOPED_TRACE(target_info.ash_data_items[i].path.value());
    EXPECT_EQ(target_info.ash_data_items[i], expected_ash_data_items[i]);
  }

  // Check for lacros data.
  std::vector<BrowserDataMigratorImpl::TargetItem> expected_lacros_data_items =
      {
          {from_dir_.Append(kBookmarks), kFileSize,
           BrowserDataMigratorImpl::TargetItem::ItemType::kFile},
          {from_dir_.Append(kCookies), kFileSize,
           BrowserDataMigratorImpl::TargetItem::ItemType::kFile},
      };
  ASSERT_EQ(target_info.lacros_data_items.size(),
            expected_lacros_data_items.size());
  std::sort(target_info.lacros_data_items.begin(),
            target_info.lacros_data_items.end(), TargetItemComparator());
  for (int i = 0; i < target_info.common_data_items.size(); i++) {
    SCOPED_TRACE(target_info.lacros_data_items[i].path.value());
    EXPECT_EQ(target_info.lacros_data_items[i], expected_lacros_data_items[i]);
  }

  // Check for common data.
  std::vector<BrowserDataMigratorImpl::TargetItem> expected_common_data_items =
      {{from_dir_.Append(kAffiliationDatabase), kFileSize * 2,
        BrowserDataMigratorImpl::TargetItem::ItemType::kDirectory}};
  ASSERT_EQ(target_info.common_data_items.size(),
            expected_common_data_items.size());
  std::sort(target_info.common_data_items.begin(),
            target_info.common_data_items.end(), TargetItemComparator());
  for (int i = 0; i < target_info.common_data_items.size(); i++) {
    SCOPED_TRACE(target_info.common_data_items[i].path.value());
    EXPECT_EQ(target_info.common_data_items[i], expected_common_data_items[i]);
  }
}

TEST_F(BrowserDataMigratorTest, CopyDirectory) {
  const base::FilePath copy_from = user_data_dir_.GetPath().Append("copy_from");
  const base::FilePath copy_to = user_data_dir_.GetPath().Append("copy_to");

  const char subdirectory[] = "Subdirectory";
  ASSERT_TRUE(base::CreateDirectory(copy_from));
  ASSERT_TRUE(base::CreateDirectory(copy_from.Append(subdirectory)));
  ASSERT_TRUE(base::CreateDirectory(
      copy_from.Append(subdirectory).Append(subdirectory)));
  ASSERT_TRUE(
      base::WriteFile(copy_from.Append(kDataFile), kDataContent, kFileSize));
  ASSERT_TRUE(base::WriteFile(copy_from.Append(subdirectory).Append(kDataFile),
                              kDataContent, kFileSize));
  ASSERT_TRUE(base::WriteFile(
      copy_from.Append(subdirectory).Append(subdirectory).Append(kDataFile),
      kDataContent, kFileSize));
  base::CreateSymbolicLink(user_data_dir_.GetPath().Append(kFirstRun),
                           copy_from.Append(kFirstRun));

  scoped_refptr<CancelFlag> cancelled = base::MakeRefCounted<CancelFlag>();
  FakeMigrationProgressTracker progress_tracker;
  ASSERT_TRUE(BrowserDataMigratorImpl::CopyDirectory(
      copy_from, copy_to, cancelled.get(), &progress_tracker));

  // Setup `copy_from` as below.
  // |- copy_from/
  //     |- data
  //     |- Subdirectory/
  //         |- data
  //         |- Subdirectory/data
  //     |- First Run  /* symlink */
  //
  // Expected `copy_to` structure after `CopyDirectory()`.
  // |- copy_to/
  //     |- data
  //     |- Subdirectory/
  //         |- data
  //         |- Subdirectory/data
  EXPECT_TRUE(base::PathExists(copy_to));
  EXPECT_TRUE(base::PathExists(copy_to.Append(kDataFile)));
  EXPECT_TRUE(base::PathExists(copy_to.Append(subdirectory).Append(kDataFile)));
  EXPECT_TRUE(base::PathExists(
      copy_to.Append(subdirectory).Append(subdirectory).Append(kDataFile)));
  // Make sure that symlink does not get copied.
  EXPECT_FALSE(base::PathExists(copy_to.Append(kFirstRun)));
}

TEST_F(BrowserDataMigratorTest, DryRunToCollectUMA) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(base::WriteFile(from_dir_.Append(FILE_PATH_LITERAL("abcd")),
                              kDataContent, kFileSize));

  BrowserDataMigratorImpl::DryRunToCollectUMA(from_dir_);

  std::string uma_name_cache =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "Cache";
  std::string uma_name_downloads =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "Downloads";
  std::string uma_name_full_restore_data =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "FullRestoreData";
  std::string uma_name_bookmarks =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "Bookmarks";
  std::string uma_name_cookies =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "Cookies";
  std::string uma_name_afiiliation_database =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "AffiliationDatabase";
  std::string uma_name_unknown =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      browser_data_migrator_util::kUnknownUMAName;

  histogram_tester.ExpectTotalCount(uma_name_cache, 1);
  histogram_tester.ExpectTotalCount(uma_name_downloads, 1);
  histogram_tester.ExpectTotalCount(uma_name_full_restore_data, 1);
  histogram_tester.ExpectTotalCount(uma_name_bookmarks, 1);
  histogram_tester.ExpectTotalCount(uma_name_cookies, 1);
  histogram_tester.ExpectTotalCount(uma_name_afiiliation_database, 1);
  histogram_tester.ExpectTotalCount(uma_name_unknown, 1);

  histogram_tester.ExpectBucketCount(uma_name_cache, kFileSize / 1024 / 1024,
                                     1);
  histogram_tester.ExpectBucketCount(uma_name_downloads,
                                     kFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_full_restore_data,
                                     kFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_bookmarks,
                                     kFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_cookies, kFileSize / 1024 / 1024,
                                     1);
  histogram_tester.ExpectBucketCount(uma_name_afiiliation_database,
                                     kFileSize * 2 / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_unknown, kFileSize / 1024 / 1024,
                                     1);

  histogram_tester.ExpectBucketCount(kDryRunNoCopyDataSize,
                                     kFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(kDryRunAshDataSize,
                                     kFileSize * 2 / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(kDryRunLacrosDataSize,
                                     kFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(kDryRunCommonDataSize,
                                     kFileSize * 3 / 1024 / 1024, 1);

  histogram_tester.ExpectTotalCount(kDryRunCopyMigrationHasEnoughDiskSpace, 1);
  histogram_tester.ExpectTotalCount(kDryRunMoveMigrationHasEnoughDiskSpace, 1);
  histogram_tester.ExpectTotalCount(
      kDryRunDeleteAndCopyMigrationHasEnoughDiskSpace, 1);
  histogram_tester.ExpectTotalCount(
      kDryRunDeleteAndMoveMigrationHasEnoughDiskSpace, 1);
}

TEST_F(BrowserDataMigratorTest, RecordStatus) {
  {
    // If `FinalStatus::kSkipped`, only record the status and do not record
    // copied data size or total time.
    base::HistogramTester histogram_tester;

    BrowserDataMigratorImpl::RecordStatus(
        BrowserDataMigratorImpl::FinalStatus::kSkipped);

    histogram_tester.ExpectTotalCount(kFinalStatus, 1);
    histogram_tester.ExpectTotalCount(kCopiedDataSize, 0);
    histogram_tester.ExpectTotalCount(kTotalTime, 0);

    histogram_tester.ExpectBucketCount(
        kFinalStatus, BrowserDataMigratorImpl::FinalStatus::kSkipped, 1);
  }

  {
    // If `FInalStatus::kSuccess`, the three UMA `kFinalStatus`,
    // `kCopiedDataSize`, `kTotalTime` should be recorded.
    base::HistogramTester histogram_tester;

    BrowserDataMigratorImpl::TargetInfo target_info;
    target_info.ash_data_size = /* 300 MBs */ 300 * 1024 * 1024;
    target_info.lacros_data_size = /* 400 MBs */ 400 * 1024 * 1024;
    target_info.common_data_size = /* 500 MBs */ 500 * 1024 * 1024;
    target_info.no_copy_data_size = /* 600 MBs */ 600 * 1024 * 1024;

    base::ElapsedTimer timer;

    BrowserDataMigratorImpl::RecordStatus(
        BrowserDataMigratorImpl::FinalStatus::kSuccess, &target_info, &timer);

    histogram_tester.ExpectTotalCount(kFinalStatus, 1);
    histogram_tester.ExpectTotalCount(kCopiedDataSize, 1);
    histogram_tester.ExpectTotalCount(kAshDataSize, 1);
    histogram_tester.ExpectTotalCount(kLacrosDataSize, 1);
    histogram_tester.ExpectTotalCount(kCommonDataSize, 1);
    histogram_tester.ExpectTotalCount(kTotalTime, 1);

    histogram_tester.ExpectBucketCount(
        kFinalStatus, BrowserDataMigratorImpl::FinalStatus::kSuccess, 1);
    histogram_tester.ExpectBucketCount(
        kCopiedDataSize, target_info.TotalCopySize() / (1024 * 1024), 1);
    histogram_tester.ExpectBucketCount(
        kAshDataSize, target_info.ash_data_size / (1024 * 1024), 1);
    histogram_tester.ExpectBucketCount(
        kLacrosDataSize, target_info.lacros_data_size / (1024 * 1024), 1);
    histogram_tester.ExpectBucketCount(
        kCommonDataSize, target_info.common_data_size / (1024 * 1024), 1);
    histogram_tester.ExpectBucketCount(
        kNoCopyDataSize, target_info.no_copy_data_size / (1024 * 1024), 1);
  }
}

TEST_F(BrowserDataMigratorTest, SetupTmpDir) {
  base::FilePath tmp_dir = from_dir_.Append(kTmpDir);
  scoped_refptr<CancelFlag> cancel_flag = base::MakeRefCounted<CancelFlag>();
  BrowserDataMigratorImpl::TargetInfo target_info =
      BrowserDataMigratorImpl::GetTargetInfo(from_dir_);
  FakeMigrationProgressTracker progress_tracker;
  EXPECT_TRUE(BrowserDataMigratorImpl::SetupTmpDir(
      target_info, from_dir_, tmp_dir, cancel_flag.get(), &progress_tracker));

  EXPECT_TRUE(base::PathExists(tmp_dir));
  EXPECT_TRUE(base::PathExists(tmp_dir.Append(kFirstRun)));
  EXPECT_TRUE(base::PathExists(tmp_dir.Append(kLacrosProfilePath)));
  EXPECT_TRUE(
      base::PathExists(tmp_dir.Append(kLacrosProfilePath).Append(kBookmarks)));
  EXPECT_TRUE(
      base::PathExists(tmp_dir.Append(kLacrosProfilePath).Append(kCookies)));
  EXPECT_TRUE(base::PathExists(
      tmp_dir.Append(kLacrosProfilePath).Append(kAffiliationDatabase)));
  EXPECT_TRUE(base::PathExists(tmp_dir.Append(kLacrosProfilePath)
                                   .Append(kAffiliationDatabase)
                                   .Append(kDataFile)));
  EXPECT_TRUE(base::PathExists(tmp_dir.Append(kLacrosProfilePath)
                                   .Append(kAffiliationDatabase)
                                   .Append(kDownloads)
                                   .Append(kDataFile)));
}

TEST_F(BrowserDataMigratorTest, CancelSetupTmpDir) {
  base::FilePath tmp_dir = from_dir_.Append(kTmpDir);
  scoped_refptr<CancelFlag> cancel_flag = base::MakeRefCounted<CancelFlag>();
  FakeMigrationProgressTracker progress_tracker;
  BrowserDataMigratorImpl::TargetInfo target_info =
      BrowserDataMigratorImpl::GetTargetInfo(from_dir_);

  // Set cancel_flag to cancel migrationl.
  cancel_flag->Set();
  EXPECT_FALSE(BrowserDataMigratorImpl::SetupTmpDir(
      target_info, user_data_dir_.GetPath(), tmp_dir, cancel_flag.get(),
      &progress_tracker));

  // These files should not exist.
  EXPECT_FALSE(base::PathExists(tmp_dir.Append(kFirstRun)));
  EXPECT_FALSE(
      base::PathExists(tmp_dir.Append(kLacrosProfilePath).Append(kBookmarks)));
  EXPECT_FALSE(
      base::PathExists(tmp_dir.Append(kLacrosProfilePath).Append(kCookies)));
}

TEST_F(BrowserDataMigratorTest, MigrateInternal) {
  base::HistogramTester histogram_tester;

  {
    scoped_refptr<CancelFlag> cancelled = base::MakeRefCounted<CancelFlag>();
    std::unique_ptr<MigrationProgressTracker> progress_tracker =
        std::make_unique<FakeMigrationProgressTracker>();
    BrowserDataMigratorImpl::MigrateInternal(
        from_dir_, std::move(progress_tracker), cancelled);

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

    const base::FilePath new_user_data_dir = from_dir_.Append(kLacrosDir);
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
  histogram_tester.ExpectTotalCount(kAshDataSize, 1);
  histogram_tester.ExpectTotalCount(kLacrosDataSize, 1);
  histogram_tester.ExpectTotalCount(kCommonDataSize, 1);
  histogram_tester.ExpectTotalCount(kNoCopyDataSize, 1);
  histogram_tester.ExpectTotalCount(kTotalTime, 1);
  histogram_tester.ExpectTotalCount(kLacrosDataTime, 1);
  histogram_tester.ExpectTotalCount(kCommonDataTime, 1);
  histogram_tester.ExpectTotalCount(kCreateDirectoryFail, 0);

  histogram_tester.ExpectBucketCount(
      kFinalStatus, BrowserDataMigratorImpl::FinalStatus::kSuccess, 1);
  histogram_tester.ExpectBucketCount(kCopiedDataSize,
                                     kFileSize * 4 / (1024 * 1024), 1);
}

TEST_F(BrowserDataMigratorTest, Migrate) {
  base::test::TaskEnvironment task_environment;
  scoped_refptr<CancelFlag> cancelled = base::MakeRefCounted<CancelFlag>();
  std::unique_ptr<MigrationProgressTracker> progress_tracker =
      std::make_unique<FakeMigrationProgressTracker>();
  const std::string user_id_hash = "abcd";
  BrowserDataMigratorImpl::SetMigrationStep(
      &pref_service_, BrowserDataMigratorImpl::MigrationStep::kRestartCalled);
  // Set migration attempt count to 1.
  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);

  base::RunLoop run_loop;
  std::unique_ptr<BrowserDataMigratorImpl> migrator =
      std::make_unique<BrowserDataMigratorImpl>(
          from_dir_, user_id_hash, base::DoNothing(), run_loop.QuitClosure(),
          &pref_service_);
  migrator->Migrate();
  run_loop.Run();

  const base::FilePath new_user_data_dir = from_dir_.Append(kLacrosDir);
  const base::FilePath new_profile_data_dir =
      new_user_data_dir.Append("Default");
  // Check that `First Run` file is created inside the new data directory.
  EXPECT_TRUE(base::PathExists(new_user_data_dir.Append(kFirstRun)));
  // Check that migration is marked as completed for the user.
  EXPECT_TRUE(crosapi::browser_util::IsProfileMigrationCompletedForUser(
      &pref_service_, user_id_hash));
  EXPECT_EQ(migrator->GetFinalStatus(),
            BrowserDataMigratorImpl::ResultValue::kSucceeded);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationStep(&pref_service_),
            BrowserDataMigratorImpl::MigrationStep::kEnded);
  // Successful migration should clear the migration attempt count.
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            0);
  // Data version should be updated to the current version after a migration.
  EXPECT_EQ(crosapi::browser_util::GetDataVer(&pref_service_, user_id_hash),
            version_info::GetVersion());
}

TEST_F(BrowserDataMigratorTest, MigrateCancelled) {
  base::test::TaskEnvironment task_environment;
  scoped_refptr<CancelFlag> cancelled = base::MakeRefCounted<CancelFlag>();
  std::unique_ptr<MigrationProgressTracker> progress_tracker =
      std::make_unique<FakeMigrationProgressTracker>();
  const std::string user_id_hash = "abcd";
  BrowserDataMigratorImpl::SetMigrationStep(
      &pref_service_, BrowserDataMigratorImpl::MigrationStep::kRestartCalled);
  // Set migration attempt count to 1.
  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);

  base::RunLoop run_loop;
  std::unique_ptr<BrowserDataMigratorImpl> migrator =
      std::make_unique<BrowserDataMigratorImpl>(
          from_dir_, user_id_hash, base::DoNothing(), run_loop.QuitClosure(),
          &pref_service_);
  migrator->Migrate();
  migrator->Cancel();
  run_loop.Run();

  const base::FilePath new_user_data_dir = from_dir_.Append(kLacrosDir);
  const base::FilePath new_profile_data_dir =
      new_user_data_dir.Append("Default");
  EXPECT_FALSE(base::PathExists(new_user_data_dir.Append(kFirstRun)));
  EXPECT_FALSE(crosapi::browser_util::IsProfileMigrationCompletedForUser(
      &pref_service_, user_id_hash));
  EXPECT_EQ(migrator->GetFinalStatus(),
            BrowserDataMigratorImpl::ResultValue::kCancelled);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationStep(&pref_service_),
            BrowserDataMigratorImpl::MigrationStep::kEnded);
  // If migration fails, migration attempt count should not be cleared thus
  // should remain as 1.
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            1);
  // Even if migration is cancelled, lacros data dir is cleared and thus data
  // version should be updated.
  EXPECT_EQ(crosapi::browser_util::GetDataVer(&pref_service_, user_id_hash),
            version_info::GetVersion());
}
}  // namespace ash
