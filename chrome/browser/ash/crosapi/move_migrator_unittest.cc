// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/move_migrator.h"

#include <sys/stat.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/fake_migration_progress_tracker.h"
#include "chrome/common/chrome_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kBookmarksFilePath[] = "Bookmarks";   // lacros
constexpr char kDownloadsFilePath[] = "Downloads";   // remain in ash
constexpr char kLoginDataFilePath[] = "Login Data";  // need copy
constexpr char kCacheFilePath[] = "Cache";           // deletable

constexpr char kDataFilePath[] = "Data";
constexpr char kDataContent[] = "Hello, World!";
constexpr int kDataSize = sizeof(kDataContent);

constexpr int64_t kRequiredDiskSpaceForBot =
    browser_data_migrator_util::kBuffer * 2;

void SetUpProfileDirectory(const base::FilePath& path) {
  // Setup `path` as below.
  // |- Bookmarks/
  // |- Downloads/
  // |- Login Data/
  // |- Cache/

  ASSERT_TRUE(base::CreateDirectory(path.Append(kCacheFilePath)));
  ASSERT_EQ(base::WriteFile(path.Append(kCacheFilePath).Append(kDataFilePath),
                            kDataContent, kDataSize),
            kDataSize);

  ASSERT_TRUE(base::CreateDirectory(path.Append(kDownloadsFilePath)));
  ASSERT_EQ(
      base::WriteFile(path.Append(kDownloadsFilePath).Append(kDataFilePath),
                      kDataContent, kDataSize),
      kDataSize);

  ASSERT_TRUE(base::CreateDirectory(path.Append(kBookmarksFilePath)));
  ASSERT_EQ(
      base::WriteFile(path.Append(kBookmarksFilePath).Append(kDataFilePath),
                      kDataContent, kDataSize),
      kDataSize);

  ASSERT_TRUE(base::CreateDirectory(path.Append(kLoginDataFilePath)));
  ASSERT_EQ(
      base::WriteFile(path.Append(kLoginDataFilePath).Append(kDataFilePath),
                      kDataContent, kDataSize),
      kDataSize);
}

}  // namespace

TEST(MoveMigratorTest, PreMigrationCleanUp) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  ASSERT_GE(base::SysInfo::AmountOfFreeDiskSpace(scoped_temp_dir.GetPath()),
            kRequiredDiskSpaceForBot);

  // `PreMigrationCleanUp()` returns true if there is nothing to delete.
  const base::FilePath original_profile_dir_1 =
      scoped_temp_dir.GetPath().Append("user1");
  EXPECT_TRUE(base::CreateDirectory(original_profile_dir_1));
  MoveMigrator::PreMigrationCleanUpResult result_1 =
      MoveMigrator::PreMigrationCleanUp(original_profile_dir_1);
  ASSERT_TRUE(result_1.success);
  EXPECT_EQ(result_1.extra_bytes_required_to_be_freed, 0u);

  // `PreMigrationCleanUp()` deletes any `.../lacros/` directory and returns
  // true.
  const base::FilePath original_profile_dir_2 =
      scoped_temp_dir.GetPath().Append("user2");
  EXPECT_TRUE(base::CreateDirectory(original_profile_dir_2));
  EXPECT_TRUE(base::CreateDirectory(
      original_profile_dir_2.Append(browser_data_migrator_util::kLacrosDir)));
  EXPECT_EQ(base::WriteFile(original_profile_dir_2
                                .Append(browser_data_migrator_util::kLacrosDir)
                                .Append(chrome::kFirstRunSentinel),
                            "", 0),
            0);
  MoveMigrator::PreMigrationCleanUpResult result_2 =
      MoveMigrator::PreMigrationCleanUp(original_profile_dir_2);
  ASSERT_TRUE(result_2.success);
  EXPECT_EQ(result_2.extra_bytes_required_to_be_freed, 0u);
  EXPECT_FALSE(base::PathExists(
      original_profile_dir_2.Append(browser_data_migrator_util::kLacrosDir)));

  // `PreMigrationCleanUp()` deletes any deletable item and returns true.
  const base::FilePath original_profile_dir_3 =
      scoped_temp_dir.GetPath().Append("user3");
  EXPECT_TRUE(base::CreateDirectory(original_profile_dir_3));
  ASSERT_EQ(base::WriteFile(original_profile_dir_3.Append(kCacheFilePath),
                            kDataContent, kDataSize),
            kDataSize);
  MoveMigrator::PreMigrationCleanUpResult result_3 =
      MoveMigrator::PreMigrationCleanUp(original_profile_dir_3);
  ASSERT_TRUE(result_3.success);
  EXPECT_EQ(result_3.extra_bytes_required_to_be_freed, 0u);
  EXPECT_FALSE(base::PathExists(
      original_profile_dir_3.Append(browser_data_migrator_util::kLacrosDir)));
  EXPECT_FALSE(base::PathExists(original_profile_dir_3.Append(kCacheFilePath)));
}

TEST(MoveMigratorTest, SetupLacrosDir) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath original_profile_dir = scoped_temp_dir.GetPath();
  SetUpProfileDirectory(original_profile_dir);

  std::unique_ptr<MigrationProgressTracker> progress_tracker =
      std::make_unique<FakeMigrationProgressTracker>();
  scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag =
      base::MakeRefCounted<browser_data_migrator_util::CancelFlag>();

  EXPECT_TRUE(MoveMigrator::SetupLacrosDir(
      original_profile_dir, std::move(progress_tracker), cancel_flag));

  const base::FilePath tmp_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);

  // Check chrome::kFirstRunSentinel, need copy item and lacros item exist in
  // lacros dir.
  EXPECT_TRUE(base::PathExists(tmp_user_dir.Append(chrome::kFirstRunSentinel)));
  EXPECT_TRUE(base::PathExists(tmp_profile_dir.Append(kLoginDataFilePath)));
  EXPECT_TRUE(base::PathExists(tmp_profile_dir.Append(kBookmarksFilePath)));

  // Check that the lacros files exists as a hard link to the original file i.e.
  // they point to the same inode. Note that directories are created.
  const base::FilePath original_bookmarks_data_path =
      original_profile_dir.Append(kBookmarksFilePath).Append(kDataFilePath);
  const base::FilePath new_bookmarks_data_path =
      tmp_profile_dir.Append(kBookmarksFilePath).Append(kDataFilePath);

  struct stat st_1;
  ASSERT_EQ(stat(original_bookmarks_data_path.value().c_str(), &st_1), 0);

  struct stat st_2;
  ASSERT_EQ(stat(new_bookmarks_data_path.value().c_str(), &st_2), 0);

  EXPECT_EQ(st_1.st_ino, st_2.st_ino);
}

TEST(MoveMigratorTest, SetupLacrosDirFailIfNoWritePermForLacrosItem) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath original_profile_dir = scoped_temp_dir.GetPath();
  SetUpProfileDirectory(original_profile_dir);

  std::unique_ptr<MigrationProgressTracker> progress_tracker =
      std::make_unique<FakeMigrationProgressTracker>();
  scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag =
      base::MakeRefCounted<browser_data_migrator_util::CancelFlag>();

  // Remove write permission from a lacros item.
  base::SetPosixFilePermissions(original_profile_dir.Append(kBookmarksFilePath),
                                0500);

  EXPECT_FALSE(MoveMigrator::SetupLacrosDir(
      original_profile_dir, std::move(progress_tracker), cancel_flag));
}

TEST(MoveMigratorTest, RemoveHardLinksFromOriginalDir) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath original_profile_dir = scoped_temp_dir.GetPath();
  SetUpProfileDirectory(original_profile_dir);

  EXPECT_TRUE(
      MoveMigrator::RemoveHardLinksFromOriginalDir(original_profile_dir));

  // Check that lacros items are deleted.
  EXPECT_FALSE(
      base::PathExists(original_profile_dir.Append(kBookmarksFilePath)));
}

class MoveMigratorMigrateTest : public ::testing::Test {
 public:
  MoveMigratorMigrateTest()
      : run_loop_(std::make_unique<base::RunLoop>()), user_id_hash_("abcd") {}

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    ASSERT_GE(base::SysInfo::AmountOfFreeDiskSpace(scoped_temp_dir_.GetPath()),
              kRequiredDiskSpaceForBot);
    original_profile_dir_ = scoped_temp_dir_.GetPath();

    SetUpProfileDirectory(original_profile_dir_);

    std::unique_ptr<MigrationProgressTracker> progress_tracker =
        std::make_unique<FakeMigrationProgressTracker>();
    scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag =
        base::MakeRefCounted<browser_data_migrator_util::CancelFlag>();

    base::OnceCallback<void(BrowserDataMigratorImpl::MigrationResult)>
        finished_callback = base::BindOnce(
            [](BrowserDataMigratorImpl::DataWipeResult* data_wipe_result,
               BrowserDataMigrator::Result* data_migration_result,
               base::OnceClosure cb,
               BrowserDataMigratorImpl::MigrationResult result) {
              *data_wipe_result = result.data_wipe_result;
              *data_migration_result = result.data_migration_result;
              std::move(cb).Run();
            },
            &data_wipe_result_, &data_migration_result_,
            run_loop_->QuitClosure());

    migrator_ = std::make_unique<MoveMigrator>(
        original_profile_dir_, user_id_hash_, std::move(progress_tracker),
        cancel_flag, &pref_service_, std::move(finished_callback));

    MoveMigrator::RegisterLocalStatePrefs(pref_service_.registry());
  }

  void CheckProfileDirFinalState() {
    // Check that `original_profile_dir_` is as below as a result of the
    // migration.
    // |- Downloads
    // |- Login Data
    // |- lacros/First Run
    // |- lacros/Default/
    //     |- Login Data
    //     |- Bookmarks

    const base::FilePath new_user_dir =
        original_profile_dir_.Append(browser_data_migrator_util::kLacrosDir);
    const base::FilePath new_profile_dir =
        new_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);

    EXPECT_TRUE(
        base::PathExists(new_user_dir.Append(chrome::kFirstRunSentinel)));

    EXPECT_FALSE(
        base::PathExists(original_profile_dir_.Append(kBookmarksFilePath)));
    EXPECT_TRUE(base::PathExists(new_profile_dir.Append(kBookmarksFilePath)));

    EXPECT_TRUE(
        base::PathExists(original_profile_dir_.Append(kLoginDataFilePath)));
    EXPECT_TRUE(base::PathExists(new_profile_dir.Append(kLoginDataFilePath)));

    EXPECT_TRUE(
        base::PathExists(original_profile_dir_.Append(kDownloadsFilePath)));
    EXPECT_FALSE(base::PathExists(new_profile_dir.Append(kDownloadsFilePath)));

    EXPECT_FALSE(
        base::PathExists(original_profile_dir_.Append(kCacheFilePath)));
    EXPECT_FALSE(base::PathExists(new_profile_dir.Append(kCacheFilePath)));
  }

  void TearDown() override { EXPECT_TRUE(scoped_temp_dir_.Delete()); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath original_profile_dir_;
  TestingPrefServiceSimple pref_service_;
  std::string user_id_hash_;
  std::unique_ptr<MoveMigrator> migrator_;

  // Updated from `finished_callback` with the corresponding value on
  // `BrowserDataMigratorImpl::MigrationResult`.
  BrowserDataMigratorImpl::DataWipeResult data_wipe_result_;
  BrowserDataMigrator::Result data_migration_result_;
};

TEST_F(MoveMigratorMigrateTest, Migrate) {
  migrator_->Migrate();
  run_loop_->Run();

  EXPECT_EQ(data_wipe_result_,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(data_migration_result_.kind,
            BrowserDataMigrator::ResultKind::kSucceeded);

  CheckProfileDirFinalState();
}

TEST_F(MoveMigratorMigrateTest, MigrateResumeFromRemoveHardLinks) {
  MoveMigrator::SetResumeStep(&pref_service_, user_id_hash_,
                              MoveMigrator::ResumeStep::kRemoveHardLinks);

  // Setup `original_profile_dir_` as below.
  // |- Bookmarks
  // |- Downloads
  // |- Login Data
  // |- move_migrator/First Run
  // |- move_migrator/Default/
  //     |- Login Data
  //     |- Bookmarks

  const base::FilePath tmp_user_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);
  ASSERT_TRUE(base::DeletePathRecursively(
      original_profile_dir_.Append(kCacheFilePath)));

  ASSERT_TRUE(base::CreateDirectory(tmp_user_dir));
  ASSERT_EQ(
      base::WriteFile(tmp_user_dir.Append(chrome::kFirstRunSentinel), "", 0),
      0);
  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir));
  ASSERT_TRUE(base::CopyDirectory(
      original_profile_dir_.Append(kLoginDataFilePath),
      tmp_profile_dir.Append(kLoginDataFilePath), true /* recursive */));
  ASSERT_TRUE(browser_data_migrator_util::CopyDirectoryByHardLinks(
      original_profile_dir_.Append(kBookmarksFilePath),
      tmp_profile_dir.Append(kBookmarksFilePath)));

  migrator_->Migrate();
  run_loop_->Run();

  EXPECT_EQ(data_wipe_result_,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(data_migration_result_.kind,
            BrowserDataMigrator::ResultKind::kSucceeded);

  CheckProfileDirFinalState();
}

TEST_F(MoveMigratorMigrateTest, MigrateResumeFromMove) {
  MoveMigrator::SetResumeStep(&pref_service_, user_id_hash_,
                              MoveMigrator::ResumeStep::kMoveTmpDir);

  // Setup `original_profile_dir_` as below.
  // |- Downloads
  // |- Login Data
  // |- move_migrator/First Run
  // |- move_migrator/Default/
  //     |- Login Data
  //     |- Bookmarks

  const base::FilePath tmp_user_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);
  ASSERT_TRUE(base::DeletePathRecursively(
      original_profile_dir_.Append(kCacheFilePath)));

  ASSERT_TRUE(base::CreateDirectory(tmp_user_dir));
  ASSERT_EQ(
      base::WriteFile(tmp_user_dir.Append(chrome::kFirstRunSentinel), "", 0),
      0);
  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir));
  ASSERT_TRUE(base::CopyDirectory(
      original_profile_dir_.Append(kLoginDataFilePath),
      tmp_profile_dir.Append(kLoginDataFilePath), true /* recursive */));
  ASSERT_TRUE(browser_data_migrator_util::CopyDirectoryByHardLinks(
      original_profile_dir_.Append(kBookmarksFilePath),
      tmp_profile_dir.Append(kBookmarksFilePath)));
  ASSERT_TRUE(base::DeletePathRecursively(
      original_profile_dir_.Append(kBookmarksFilePath)));

  migrator_->Migrate();
  run_loop_->Run();

  EXPECT_EQ(data_wipe_result_,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(data_migration_result_.kind,
            BrowserDataMigrator::ResultKind::kSucceeded);

  CheckProfileDirFinalState();
}

TEST_F(MoveMigratorMigrateTest, MigrateOutOfDisk) {
  // Emulate the situation of out-of-disk.
  browser_data_migrator_util::ScopedExtraBytesRequiredToBeFreedForTesting
      scoped_extra_bytes(100);

  migrator_->Migrate();
  run_loop_->Run();

  EXPECT_EQ(data_wipe_result_,
            BrowserDataMigratorImpl::DataWipeResult::kFailed);
  EXPECT_EQ(data_migration_result_.kind,
            BrowserDataMigrator::ResultKind::kFailed);
  EXPECT_EQ(100u, data_migration_result_.required_size);
}

}  // namespace ash
