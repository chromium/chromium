// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/move_migrator.h"

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/fake_migration_progress_tracker.h"
#include "chrome/common/chrome_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace ash {

namespace {

constexpr char kBookmarksFilePath[] = "Bookmarks";             // lacros
constexpr char kCookiesFilePath[] = "Cookies";                 // lacros
constexpr char kDownloadsFilePath[] = "Downloads";             // remain in ash
constexpr char kExtensionStateFilePath[] = "Extension State";  // split
constexpr char kLoginDataFilePath[] = "Login Data";            // need copy
constexpr char kCacheFilePath[] = "Cache";                     // deletable

constexpr char kDataFilePath[] = "Data";
constexpr char kDataContent[] = "Hello, World!";
constexpr int kDataSize = sizeof(kDataContent);

// ID of an extension that will be moved from Ash to Lacros.
// NOTE: we use a sequence of characters that can't be an
// actual AppId here, so we can be sure that it won't be
// included in `kExtensionKeepList`.
constexpr char kMoveExtensionId[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

constexpr int64_t kRequiredDiskSpaceForBot =
    browser_data_migrator_util::kBuffer * 2;

// Setup the `Extensions` folder inside a profile.
// If `ash_only` is true, it will only generate data associated to extensions
// that have to be kept in Ash. Otherwise, it will generate data for both
// categories of extensions.
void SetUpExtensions(const base::FilePath& profile_path,
                     bool ash = true,
                     bool lacros = true) {
  base::FilePath path =
      profile_path.Append(browser_data_migrator_util::kExtensionsFilePath);

  // Generate data for an extension that has to be moved to Lacros.
  if (lacros) {
    ASSERT_TRUE(base::CreateDirectory(path.Append(kMoveExtensionId)));
    ASSERT_EQ(
        base::WriteFile(path.Append(kMoveExtensionId).Append(kDataFilePath),
                        kDataContent, kDataSize),
        kDataSize);
  }

  // Generate data for an extension that has to stay in Ash.
  if (ash) {
    std::string keep_extension_id =
        browser_data_migrator_util::kExtensionKeepList[0];
    ASSERT_TRUE(base::CreateDirectory(path.Append(keep_extension_id)));
    ASSERT_EQ(
        base::WriteFile(path.Append(keep_extension_id).Append(kDataFilePath),
                        kDataContent, kDataSize),
        kDataSize);
  }
}

// Setup the `Local Storage` folder inside a profile.
// If `ash_only` is true, it will only generate data associated to extensions
// that have to be kept in Ash. Otherwise, it will generate data for both
// categories of extensions.
void SetUpLocalStorage(const base::FilePath& profile_path,
                       bool ash_only = false) {
  using std::string_literals::operator""s;

  base::FilePath path =
      profile_path.Append(browser_data_migrator_util::kLocalStorageFilePath)
          .Append(browser_data_migrator_util::kLocalStorageLeveldbName);

  // Open a new LevelDB database.
  leveldb_env::Options options;
  options.create_if_missing = true;
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status = leveldb_env::OpenDB(options, path.value(), &db);
  ASSERT_TRUE(status.ok());
  // Part of the LocalStorage schema.
  leveldb::WriteBatch batch;
  batch.Put("VERSION", "1");

  // Generate data for an extension that has to be moved to Lacros.
  std::string key;
  if (!ash_only) {
    batch.Put("META:chrome-extension://" + std::string(kMoveExtensionId),
              "meta");
    batch.Put(
        "_chrome-extension://" + std::string(kMoveExtensionId) + "\x00key"s,
        "value");
  }

  // Generate data for an extension that has to stay in Ash.
  std::string keep_extension_id =
      browser_data_migrator_util::kExtensionKeepList[0];
  batch.Put("META:chrome-extension://" + keep_extension_id, "meta");
  batch.Put("_chrome-extension://" + keep_extension_id + "\x00key"s, "value");

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = db->Write(write_options, &batch);
  ASSERT_TRUE(status.ok());
}

// Setup the `Extension State` folder inside a profile directory.
void SetUpExtensionState(const base::FilePath& profile_path) {
  base::FilePath path = profile_path.Append(kExtensionStateFilePath);

  leveldb_env::Options options;
  options.create_if_missing = true;
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status = leveldb_env::OpenDB(options, path.value(), &db);
  ASSERT_TRUE(status.ok());

  std::string keep_extension_id =
      browser_data_migrator_util::kExtensionKeepList[0];
  leveldb::WriteBatch batch;
  batch.Put(std::string(kMoveExtensionId) + ".key", "value");
  batch.Put(keep_extension_id + ".key", "value");

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = db->Write(write_options, &batch);
  ASSERT_TRUE(status.ok());
}

void SetUpProfileDirectory(const base::FilePath& path) {
  // Setup `path` as below.
  // |- Bookmarks/
  // |- Cache/
  // |- Cookies
  // |- Downloads/
  // |- Extensions/
  // |- Extension State/
  // |- Login Data/
  // |- Local Storage/

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
  ASSERT_EQ(
      base::WriteFile(path.Append(kCookiesFilePath), kDataContent, kDataSize),
      kDataSize);

  ASSERT_TRUE(base::CreateDirectory(path.Append(kLoginDataFilePath)));
  ASSERT_EQ(
      base::WriteFile(path.Append(kLoginDataFilePath).Append(kDataFilePath),
                      kDataContent, kDataSize),
      kDataSize);

  SetUpExtensions(path);
  SetUpLocalStorage(path);
  SetUpExtensionState(path);
}

std::map<std::string, std::string> ReadLevelDB(const base::FilePath& path) {
  leveldb_env::Options options;
  options.create_if_missing = false;

  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status = leveldb_env::OpenDB(options, path.value(), &db);
  EXPECT_TRUE(status.ok());

  std::map<std::string, std::string> db_map;
  std::unique_ptr<leveldb::Iterator> it(
      db->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    db_map.emplace(it->key().ToString(), it->value().ToString());
  }

  return db_map;
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

  // `PreMigrationCleanUp()` deletes any temporary split directory and returns
  // true.
  const base::FilePath original_profile_dir_4 =
      scoped_temp_dir.GetPath().Append("user4");
  EXPECT_TRUE(base::CreateDirectory(original_profile_dir_4));
  EXPECT_TRUE(base::CreateDirectory(
      original_profile_dir_4.Append(browser_data_migrator_util::kSplitTmpDir)));
  EXPECT_EQ(
      base::WriteFile(original_profile_dir_4
                          .Append(browser_data_migrator_util::kSplitTmpDir)
                          .Append("TestFile"),
                      kDataContent, kDataSize),
      kDataSize);
  MoveMigrator::PreMigrationCleanUpResult result_4 =
      MoveMigrator::PreMigrationCleanUp(original_profile_dir_4);
  ASSERT_TRUE(result_4.success);
  EXPECT_EQ(result_4.extra_bytes_required_to_be_freed, 0u);
  EXPECT_FALSE(base::PathExists(
      original_profile_dir_4.Append(browser_data_migrator_util::kSplitTmpDir)));
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
}

TEST(MoveMigratorTest, MoveLacrosItemsToNewDir) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath original_profile_dir = scoped_temp_dir.GetPath();
  SetUpProfileDirectory(original_profile_dir);

  const base::FilePath tmp_profile_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir)
          .Append(browser_data_migrator_util::kLacrosProfilePath);

  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir));
  EXPECT_TRUE(MoveMigrator::MoveLacrosItemsToNewDir(original_profile_dir));

  EXPECT_FALSE(
      base::PathExists(original_profile_dir.Append(kBookmarksFilePath)));
  EXPECT_FALSE(base::PathExists(original_profile_dir.Append(kCookiesFilePath)));
  EXPECT_TRUE(base::PathExists(tmp_profile_dir.Append(kBookmarksFilePath)));
  EXPECT_TRUE(base::PathExists(tmp_profile_dir.Append(kCookiesFilePath)));
}

TEST(MoveMigratorTest, MoveLacrosItemsToNewDirFailIfNoWritePermForLacrosItem) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath original_profile_dir = scoped_temp_dir.GetPath();
  SetUpProfileDirectory(original_profile_dir);

  // Remove write permission from a lacros item.
  base::SetPosixFilePermissions(original_profile_dir.Append(kBookmarksFilePath),
                                0500);

  EXPECT_FALSE(MoveMigrator::MoveLacrosItemsToNewDir(original_profile_dir));
}

TEST(MoveMigratorTest, SetupAshSplitDir) {
  using std::string_literals::operator""s;

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath original_profile_dir = scoped_temp_dir.GetPath();
  SetUpProfileDirectory(original_profile_dir);

  EXPECT_TRUE(MoveMigrator::SetupAshSplitDir(original_profile_dir));

  const base::FilePath tmp_split_dir =
      original_profile_dir.Append(browser_data_migrator_util::kSplitTmpDir);

  // Check `Local Storage` is present in the split directory.
  base::FilePath path =
      tmp_split_dir.Append(browser_data_migrator_util::kLocalStorageFilePath)
          .Append(browser_data_migrator_util::kLocalStorageLeveldbName);
  EXPECT_TRUE(base::PathExists(path));
  // Check the content of the leveldb database. It should contain only
  // extensions in the keep list.
  auto db_map = ReadLevelDB(path);
  EXPECT_EQ(3, db_map.size());
  EXPECT_EQ("1", db_map["VERSION"]);
  std::string keep_extension_id =
      browser_data_migrator_util::kExtensionKeepList[0];
  std::string key = "_chrome-extension://" + keep_extension_id + "\x00key"s;
  EXPECT_EQ("meta", db_map["META:chrome-extension://" + keep_extension_id]);
  EXPECT_EQ("value", db_map[key]);

  // Check `Extension State` is present in the split directory.
  path = tmp_split_dir.Append(kExtensionStateFilePath);
  EXPECT_TRUE(base::PathExists(path));
  // Check the content of the leveldb database. It should contain only
  // extensions in the keep list.
  db_map = ReadLevelDB(path);
  EXPECT_EQ(1, db_map.size());
  EXPECT_EQ("value", db_map[keep_extension_id + ".key"]);
}

TEST(MoveMigratorTest, ResumeRequired) {
  const std::string user_id_hash = "abcd";
  TestingPrefServiceSimple pref_service;
  MoveMigrator::RegisterLocalStatePrefs(pref_service.registry());

  EXPECT_FALSE(MoveMigrator::ResumeRequired(&pref_service, user_id_hash));

  MoveMigrator::SetResumeStep(&pref_service, user_id_hash,
                              MoveMigrator::ResumeStep::kMoveLacrosItems);
  EXPECT_TRUE(MoveMigrator::ResumeRequired(&pref_service, user_id_hash));

  MoveMigrator::SetResumeStep(&pref_service, user_id_hash,
                              MoveMigrator::ResumeStep::kMoveSplitItems);
  EXPECT_TRUE(MoveMigrator::ResumeRequired(&pref_service, user_id_hash));

  MoveMigrator::SetResumeStep(&pref_service, user_id_hash,
                              MoveMigrator::ResumeStep::kMoveTmpDir);
  EXPECT_TRUE(MoveMigrator::ResumeRequired(&pref_service, user_id_hash));

  MoveMigrator::SetResumeStep(&pref_service, user_id_hash,
                              MoveMigrator::ResumeStep::kCompleted);
  EXPECT_FALSE(MoveMigrator::ResumeRequired(&pref_service, user_id_hash));
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
    // |- Extensions
    // |- Local Storage
    // |- Login Data
    // |- lacros/First Run
    // |- lacros/Default/
    //     |- Bookmarks
    //     |- Cookies
    //     |- Extensions
    //     |- Local Storage
    //     |- Login Data

    const base::FilePath new_user_dir =
        original_profile_dir_.Append(browser_data_migrator_util::kLacrosDir);
    const base::FilePath new_profile_dir =
        new_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);

    EXPECT_TRUE(
        base::PathExists(new_user_dir.Append(chrome::kFirstRunSentinel)));

    EXPECT_FALSE(
        base::PathExists(original_profile_dir_.Append(kBookmarksFilePath)));
    EXPECT_TRUE(base::PathExists(new_profile_dir.Append(kBookmarksFilePath)));

    EXPECT_FALSE(
        base::PathExists(original_profile_dir_.Append(kCookiesFilePath)));
    EXPECT_TRUE(base::PathExists(new_profile_dir.Append(kCookiesFilePath)));

    EXPECT_TRUE(
        base::PathExists(original_profile_dir_.Append(kLoginDataFilePath)));
    EXPECT_TRUE(base::PathExists(new_profile_dir.Append(kLoginDataFilePath)));

    EXPECT_TRUE(
        base::PathExists(original_profile_dir_.Append(kDownloadsFilePath)));
    EXPECT_FALSE(base::PathExists(new_profile_dir.Append(kDownloadsFilePath)));

    EXPECT_FALSE(
        base::PathExists(original_profile_dir_.Append(kCacheFilePath)));
    EXPECT_FALSE(base::PathExists(new_profile_dir.Append(kCacheFilePath)));

    // Extensions.
    std::string keep_extension_id =
        browser_data_migrator_util::kExtensionKeepList[0];
    EXPECT_TRUE(base::PathExists(
        original_profile_dir_
            .Append(browser_data_migrator_util::kExtensionsFilePath)
            .Append(keep_extension_id)));
    EXPECT_FALSE(base::PathExists(
        original_profile_dir_
            .Append(browser_data_migrator_util::kExtensionsFilePath)
            .Append(kMoveExtensionId)));
    EXPECT_TRUE(base::PathExists(
        new_profile_dir.Append(browser_data_migrator_util::kExtensionsFilePath)
            .Append(kMoveExtensionId)));

    // Local Storage.
    const base::FilePath ash_local_storage_path =
        original_profile_dir_
            .Append(browser_data_migrator_util::kLocalStorageFilePath)
            .Append(browser_data_migrator_util::kLocalStorageLeveldbName);
    const base::FilePath lacros_local_storage_path =
        new_profile_dir
            .Append(browser_data_migrator_util::kLocalStorageFilePath)
            .Append(browser_data_migrator_util::kLocalStorageLeveldbName);
    EXPECT_TRUE(base::PathExists(ash_local_storage_path));
    EXPECT_TRUE(base::PathExists(lacros_local_storage_path));
    // Ash contains only keys relevant to the extension keep list.
    auto ash_local_storage = ReadLevelDB(ash_local_storage_path);
    EXPECT_EQ(3, ash_local_storage.size());
    // Lacros contains all the keys.
    auto lacros_local_storage = ReadLevelDB(lacros_local_storage_path);
    EXPECT_EQ(5, lacros_local_storage.size());
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

TEST_F(MoveMigratorMigrateTest, MigrateResumeFromMoveLacrosItems) {
  MoveMigrator::SetResumeStep(&pref_service_, user_id_hash_,
                              MoveMigrator::ResumeStep::kMoveLacrosItems);

  // Setup `original_profile_dir_` as below.
  // |- Cookies
  // |- Downloads
  // |- Login Data
  // |- move_migrator/First Run
  // |- move_migrator/Default/
  //     |- Bookmarks
  //     |- Extensions
  //     |- Local Storage
  //     |- Login Data
  // |- move_migrator_split/
  //     |- Local Storage

  const base::FilePath tmp_user_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);
  const base::FilePath tmp_split_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kSplitTmpDir);
  ASSERT_TRUE(base::DeletePathRecursively(
      original_profile_dir_.Append(kCacheFilePath)));

  ASSERT_TRUE(base::CreateDirectory(tmp_user_dir));
  ASSERT_EQ(
      base::WriteFile(tmp_user_dir.Append(chrome::kFirstRunSentinel), "", 0),
      0);
  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir));
  ASSERT_TRUE(base::CreateDirectory(tmp_split_dir));
  ASSERT_TRUE(base::CopyDirectory(
      original_profile_dir_.Append(kLoginDataFilePath),
      tmp_profile_dir.Append(kLoginDataFilePath), true /* recursive */));
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kBookmarksFilePath),
                         tmp_profile_dir.Append(kBookmarksFilePath)));

  // Extensions have been moved to Lacros's tmp dir.
  ASSERT_TRUE(base::Move(
      original_profile_dir_.Append(
          browser_data_migrator_util::kExtensionsFilePath),
      tmp_profile_dir.Append(browser_data_migrator_util::kExtensionsFilePath)));

  // Local Storage has been split.
  ASSERT_TRUE(
      base::Move(original_profile_dir_.Append(
                     browser_data_migrator_util::kLocalStorageFilePath),
                 tmp_profile_dir.Append(
                     browser_data_migrator_util::kLocalStorageFilePath)));
  SetUpLocalStorage(tmp_split_dir, true /* ash_only */);

  migrator_->Migrate();
  run_loop_->Run();

  EXPECT_EQ(data_wipe_result_,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(data_migration_result_.kind,
            BrowserDataMigrator::ResultKind::kSucceeded);

  CheckProfileDirFinalState();
}

TEST_F(MoveMigratorMigrateTest, MigrateResumeFromMoveSplitItems) {
  MoveMigrator::SetResumeStep(&pref_service_, user_id_hash_,
                              MoveMigrator::ResumeStep::kMoveSplitItems);

  // Setup `original_profile_dir_` as below.
  // |- Downloads
  // |- Login Data
  // |- move_migrator/First Run
  // |- move_migrator/Default/
  //     |- Bookmarks
  //     |- Cookies
  //     |- Extensions
  //     |- Local Storage
  //     |- Login Data
  // |- move_migrator_split/
  //     |- Local Storage

  const base::FilePath tmp_user_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);
  const base::FilePath tmp_split_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kSplitTmpDir);
  ASSERT_TRUE(base::DeletePathRecursively(
      original_profile_dir_.Append(kCacheFilePath)));

  ASSERT_TRUE(base::CreateDirectory(tmp_user_dir));
  ASSERT_EQ(
      base::WriteFile(tmp_user_dir.Append(chrome::kFirstRunSentinel), "", 0),
      0);
  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir));
  ASSERT_TRUE(base::CreateDirectory(tmp_split_dir));
  ASSERT_TRUE(base::CopyDirectory(
      original_profile_dir_.Append(kLoginDataFilePath),
      tmp_profile_dir.Append(kLoginDataFilePath), true /* recursive */));
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kBookmarksFilePath),
                         tmp_profile_dir.Append(kBookmarksFilePath)));
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kCookiesFilePath),
                         tmp_profile_dir.Append(kCookiesFilePath)));

  // Extensions have been moved to Lacros's tmp dir, but not yet split and moved
  // to Ash profile dir.
  ASSERT_TRUE(base::Move(
      original_profile_dir_.Append(
          browser_data_migrator_util::kExtensionsFilePath),
      tmp_profile_dir.Append(browser_data_migrator_util::kExtensionsFilePath)));

  // Local Storage has been split, but not yet moved to Ash profile dir.
  ASSERT_TRUE(
      base::Move(original_profile_dir_.Append(
                     browser_data_migrator_util::kLocalStorageFilePath),
                 tmp_profile_dir.Append(
                     browser_data_migrator_util::kLocalStorageFilePath)));
  SetUpLocalStorage(tmp_split_dir, true /* ash_only */);

  migrator_->Migrate();
  run_loop_->Run();

  EXPECT_EQ(data_wipe_result_,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(data_migration_result_.kind,
            BrowserDataMigrator::ResultKind::kSucceeded);

  CheckProfileDirFinalState();
}

TEST_F(MoveMigratorMigrateTest, MigrateResumeFromMoveTmpDir) {
  MoveMigrator::SetResumeStep(&pref_service_, user_id_hash_,
                              MoveMigrator::ResumeStep::kMoveTmpDir);

  // Setup `original_profile_dir_` as below.
  // |- Downloads
  // |- Extensions
  // |- Local Storage
  // |- Login Data
  // |- move_migrator/First Run
  // |- move_migrator/Default/
  //     |- Bookmarks
  //     |- Cookies
  //     |- Extensions
  //     |- Local Storage
  //     |- Login Data

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
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kBookmarksFilePath),
                         tmp_profile_dir.Append(kBookmarksFilePath)));
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kCookiesFilePath),
                         tmp_profile_dir.Append(kCookiesFilePath)));

  // Extensions have been split, and Ash's version is in its final place.
  ASSERT_TRUE(base::DeletePathRecursively(original_profile_dir_.Append(
      browser_data_migrator_util::kExtensionsFilePath)));
  SetUpExtensions(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpExtensions(original_profile_dir_, /*ash=*/true, /*lacros=*/false);

  // Local Storage has been split, and Ash's version is in its final place.
  ASSERT_TRUE(
      base::Move(original_profile_dir_.Append(
                     browser_data_migrator_util::kLocalStorageFilePath),
                 tmp_profile_dir.Append(
                     browser_data_migrator_util::kLocalStorageFilePath)));
  SetUpLocalStorage(original_profile_dir_, true /* ash_only */);

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
