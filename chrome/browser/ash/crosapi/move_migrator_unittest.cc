// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/move_migrator.h"

#include <errno.h>

#include <map>
#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/fake_migration_progress_tracker.h"
#include "chrome/common/chrome_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/blocking_model_type_store_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace ash {

namespace {

constexpr char kBookmarksFilePath[] = "Bookmarks";             // lacros
constexpr char kCookiesFilePath[] = "Cookies";                 // lacros
constexpr char kDownloadsFilePath[] = "Downloads";             // remain in ash
constexpr char kExtensionStateFilePath[] = "Extension State";  // split
constexpr char kSharedProtoDBPath[] = "shared_proto_db";       // need copy
constexpr char kCacheFilePath[] = "Cache";                     // deletable

constexpr char kDataFilePath[] = "Data";
constexpr char kDataContent[] = "Hello, World!";

// ID of an extension that will be moved from Ash to Lacros.
// NOTE: we use a sequence of characters that can't be an
// actual AppId here, so we can be sure that it won't be
// included in `kExtensionsAshOnly`.
constexpr char kMoveExtensionId[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

constexpr syncer::ModelType kAshSyncDataType =
    browser_data_migrator_util::kAshOnlySyncDataTypes[0];
constexpr syncer::ModelType kLacrosSyncDataType = syncer::ModelType::WEB_APPS;
static_assert(!base::Contains(browser_data_migrator_util::kAshOnlySyncDataTypes,
                              kLacrosSyncDataType));

constexpr int64_t kRequiredDiskSpaceForBot =
    browser_data_migrator_util::kBuffer * 2;

base::FilePath GetNigoriPath(const base::FilePath& profile_path) {
  return profile_path.Append(browser_data_migrator_util::kSyncDataFilePath)
      .Append(browser_data_migrator_util::kSyncDataNigoriFileName);
}

// Setup the `Extensions` folder inside a profile.
// If `ash_only` is true, it will only generate data associated to extensions
// that have to be kept in Ash. Otherwise, it will generate data for both
// categories of extensions.
void SetUpExtensions(const base::FilePath& profile_path,
                     bool ash = true,
                     bool lacros = true,
                     bool both = true) {
  base::FilePath path =
      profile_path.Append(browser_data_migrator_util::kExtensionsFilePath);

  // Generate data for an extension that has to be moved to Lacros.
  if (lacros) {
    ASSERT_TRUE(base::CreateDirectory(path.Append(kMoveExtensionId)));
    ASSERT_TRUE(base::WriteFile(
        path.Append(kMoveExtensionId).Append(kDataFilePath), kDataContent));
  }

  // Generate data for an extension that has to stay in Ash.
  if (ash) {
    std::string keep_extension_id =
        browser_data_migrator_util::kExtensionsAshOnly[0];
    ASSERT_TRUE(base::CreateDirectory(path.Append(keep_extension_id)));
    ASSERT_TRUE(base::WriteFile(
        path.Append(keep_extension_id).Append(kDataFilePath), kDataContent));
  }

  // Generate data for an extension that has to be in both Ash and Lacros.
  if (both) {
    std::string both_extension_id =
        browser_data_migrator_util::kExtensionsBothChromes[0];
    ASSERT_TRUE(base::CreateDirectory(path.Append(both_extension_id)));
    ASSERT_TRUE(base::WriteFile(
        path.Append(both_extension_id).Append(kDataFilePath), kDataContent));
  }
}

// Setup the `Storage` folder inside a profile.
// If `ash_only` is true, it will only generate data associated to extensions
// that have to be kept in Ash. Otherwise, it will generate data for both
// categories of extensions.
void SetUpStorage(const base::FilePath& profile_path,
                  bool ash = true,
                  bool lacros = true,
                  bool both = true) {
  base::FilePath path =
      profile_path.Append(browser_data_migrator_util::kStorageFilePath)
          .Append(browser_data_migrator_util::kStorageExtFilePath);

  // Generate data for an extension that has to be moved to Lacros.
  if (lacros) {
    ASSERT_TRUE(base::CreateDirectory(path.Append(kMoveExtensionId)));
    ASSERT_TRUE(base::WriteFile(
        path.Append(kMoveExtensionId).Append(kDataFilePath), kDataContent));
  }

  // Generate data for an extension that has to stay in Ash.
  if (ash) {
    std::string keep_extension_id =
        browser_data_migrator_util::kExtensionsAshOnly[0];
    ASSERT_TRUE(base::CreateDirectory(path.Append(keep_extension_id)));
    ASSERT_TRUE(base::WriteFile(
        path.Append(keep_extension_id).Append(kDataFilePath), kDataContent));
  }

  // Generate data for an extension that has to be in both Ash and Lacros.
  if (both) {
    std::string both_extension_id =
        browser_data_migrator_util::kExtensionsBothChromes[0];
    ASSERT_TRUE(base::CreateDirectory(path.Append(both_extension_id)));
    ASSERT_TRUE(base::WriteFile(
        path.Append(both_extension_id).Append(kDataFilePath), kDataContent));
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
      browser_data_migrator_util::kExtensionsAshOnly[0];
  batch.Put("META:chrome-extension://" + keep_extension_id, "meta");
  batch.Put("_chrome-extension://" + keep_extension_id + "\x00key"s, "value");

  // Generate data for an extension that has to be in both Ash and Lacros.
  std::string both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];
  batch.Put("META:chrome-extension://" + both_extension_id, "meta");
  batch.Put("_chrome-extension://" + both_extension_id + "\x00key"s, "value");

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
      browser_data_migrator_util::kExtensionsAshOnly[0];
  std::string both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];
  leveldb::WriteBatch batch;
  batch.Put(std::string(kMoveExtensionId) + ".key", "value");
  batch.Put(keep_extension_id + ".key", "value");
  batch.Put(both_extension_id + ".key", "value");

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = db->Write(write_options, &batch);
  ASSERT_TRUE(status.ok());
}

void SetUpIndexedDB(const base::FilePath& profile_path,
                    bool ash = true,
                    bool lacros = true,
                    bool both = true) {
  if (lacros) {
    const auto [move_extension_blob_path, move_extension_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(profile_path,
                                                      kMoveExtensionId);
    ASSERT_TRUE(base::CreateDirectory(move_extension_blob_path));
    ASSERT_TRUE(base::CreateDirectory(move_extension_leveldb_path));
    ASSERT_TRUE(base::WriteFile(move_extension_blob_path.Append(kDataFilePath),
                                kDataContent));
    ASSERT_TRUE(base::WriteFile(
        move_extension_leveldb_path.Append(kDataFilePath), kDataContent));
  }

  if (ash) {
    const char* keep_extension_id =
        browser_data_migrator_util::kExtensionsAshOnly[0];
    const auto [keep_extension_blob_path, keep_extension_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(profile_path,
                                                      keep_extension_id);
    ASSERT_TRUE(base::CreateDirectory(keep_extension_blob_path));
    ASSERT_TRUE(base::CreateDirectory(keep_extension_leveldb_path));
    ASSERT_TRUE(base::WriteFile(keep_extension_blob_path.Append(kDataFilePath),
                                kDataContent));
    ASSERT_TRUE(base::WriteFile(
        keep_extension_leveldb_path.Append(kDataFilePath), kDataContent));
  }

  if (both) {
    const char* both_extension_id =
        browser_data_migrator_util::kExtensionsBothChromes[0];
    const auto [both_extension_blob_path, both_extension_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(profile_path,
                                                      both_extension_id);
    ASSERT_TRUE(base::CreateDirectory(both_extension_blob_path));
    ASSERT_TRUE(base::CreateDirectory(both_extension_leveldb_path));
    ASSERT_TRUE(base::WriteFile(both_extension_blob_path.Append(kDataFilePath),
                                kDataContent));
    ASSERT_TRUE(base::WriteFile(
        both_extension_leveldb_path.Append(kDataFilePath), kDataContent));
  }
}

void SetUpPreferences(const base::FilePath& profile_path,
                      bool ash = true,
                      bool lacros = true) {
  base::FilePath path = profile_path.Append(chrome::kPreferencesFilename);

  std::string contents;
  base::Value::Dict dict;

  if (ash) {
    dict.SetByDottedPath(browser_data_migrator_util::kAshOnlyPreferencesKeys[0],
                         "test1");
  }
  if (lacros) {
    dict.SetByDottedPath(
        browser_data_migrator_util::kLacrosOnlyPreferencesKeys[0], "test2");
  }
  dict.SetByDottedPath("unrelated.key", "test3");

  ASSERT_TRUE(base::JSONWriter::Write(dict, &contents));
  ASSERT_TRUE(base::WriteFile(path, contents));
}

void SetUpSyncDataLevelDB(const base::FilePath& profile_path,
                          bool ash = true,
                          bool lacros = true) {
  base::FilePath leveldb_path =
      profile_path.Append(browser_data_migrator_util::kSyncDataFilePath)
          .Append(browser_data_migrator_util::kSyncDataLeveldbName);

  leveldb_env::Options options;
  options.create_if_missing = true;
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status =
      leveldb_env::OpenDB(options, leveldb_path.value(), &db);
  ASSERT_TRUE(status.ok());

  leveldb::WriteBatch batch;
  if (ash) {
    batch.Put(syncer::FormatDataPrefix(kAshSyncDataType,
                                       syncer::StorageType::kUnspecified) +
                  kMoveExtensionId,
              "ash_data");
    batch.Put(syncer::FormatMetaPrefix(kAshSyncDataType,
                                       syncer::StorageType::kUnspecified) +
                  kMoveExtensionId,
              "ash_metadata");
    batch.Put(syncer::FormatGlobalMetadataKey(
                  kAshSyncDataType, syncer::StorageType::kUnspecified),
              "ash_globalmetadata");
  }
  if (lacros) {
    batch.Put(syncer::FormatDataPrefix(kLacrosSyncDataType,
                                       syncer::StorageType::kUnspecified) +
                  kMoveExtensionId,
              "lacros_data");
    batch.Put(syncer::FormatMetaPrefix(kLacrosSyncDataType,
                                       syncer::StorageType::kUnspecified) +
                  kMoveExtensionId,
              "lacros_metadata");
    batch.Put(syncer::FormatGlobalMetadataKey(
                  kLacrosSyncDataType, syncer::StorageType::kUnspecified),
              "lacros_globalmetadata");
  }

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
  // |- Extension State/
  // |- Extensions/
  // |- IndexedDB/
  // |- Local Storage/
  // |- Login Data/
  // |- shared_proto_db/
  // |- Preferences
  // |- Storage/
  // |- Sync Data/
  //     |- LevelDB
  //     |- Nigori.bin
  ASSERT_TRUE(base::CreateDirectory(path.Append(kCacheFilePath)));
  ASSERT_TRUE(base::WriteFile(path.Append(kCacheFilePath).Append(kDataFilePath),
                              kDataContent));

  ASSERT_TRUE(base::CreateDirectory(path.Append(kDownloadsFilePath)));
  ASSERT_TRUE(base::WriteFile(
      path.Append(kDownloadsFilePath).Append(kDataFilePath), kDataContent));

  ASSERT_TRUE(base::CreateDirectory(path.Append(kBookmarksFilePath)));
  ASSERT_TRUE(base::WriteFile(
      path.Append(kBookmarksFilePath).Append(kDataFilePath), kDataContent));
  ASSERT_TRUE(base::WriteFile(path.Append(kCookiesFilePath), kDataContent));

  ASSERT_TRUE(base::CreateDirectory(path.Append(kSharedProtoDBPath)));
  ASSERT_TRUE(base::WriteFile(
      path.Append(kSharedProtoDBPath).Append(kDataFilePath), kDataContent));

  ASSERT_TRUE(base::CreateDirectory(
      path.Append(browser_data_migrator_util::kSyncDataFilePath)));
  ASSERT_TRUE(base::WriteFile(GetNigoriPath(path), kDataContent));

  SetUpExtensions(path);
  SetUpStorage(path);
  SetUpLocalStorage(path);
  SetUpExtensionState(path);
  SetUpIndexedDB(path);
  SetUpPreferences(path);
  SetUpSyncDataLevelDB(path);
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
  MoveMigrator::TaskResult result_1 =
      MoveMigrator::PreMigrationCleanUp(original_profile_dir_1);
  ASSERT_EQ(result_1.status, MoveMigrator::TaskStatus::kSucceeded);
  EXPECT_FALSE(result_1.extra_bytes_required_to_be_freed.has_value());

  // `PreMigrationCleanUp()` deletes any `.../lacros/` directory and returns
  // true.
  const base::FilePath original_profile_dir_2 =
      scoped_temp_dir.GetPath().Append("user2");
  EXPECT_TRUE(base::CreateDirectory(original_profile_dir_2));
  EXPECT_TRUE(base::CreateDirectory(
      original_profile_dir_2.Append(browser_data_migrator_util::kLacrosDir)));
  ASSERT_TRUE(base::WriteFile(
      original_profile_dir_2.Append(browser_data_migrator_util::kLacrosDir)
          .Append(chrome::kFirstRunSentinel),
      base::StringPiece()));
  MoveMigrator::TaskResult result_2 =
      MoveMigrator::PreMigrationCleanUp(original_profile_dir_2);
  ASSERT_EQ(result_2.status, MoveMigrator::TaskStatus::kSucceeded);
  EXPECT_FALSE(result_2.extra_bytes_required_to_be_freed.has_value());
  EXPECT_FALSE(base::PathExists(
      original_profile_dir_2.Append(browser_data_migrator_util::kLacrosDir)));

  // `PreMigrationCleanUp()` deletes any deletable item and returns true.
  const base::FilePath original_profile_dir_3 =
      scoped_temp_dir.GetPath().Append("user3");
  EXPECT_TRUE(base::CreateDirectory(original_profile_dir_3));
  ASSERT_TRUE(base::WriteFile(original_profile_dir_3.Append(kCacheFilePath),
                              kDataContent));
  MoveMigrator::TaskResult result_3 =
      MoveMigrator::PreMigrationCleanUp(original_profile_dir_3);
  ASSERT_EQ(result_3.status, MoveMigrator::TaskStatus::kSucceeded);
  EXPECT_FALSE(result_3.extra_bytes_required_to_be_freed.has_value());
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
  ASSERT_TRUE(base::WriteFile(
      original_profile_dir_4.Append(browser_data_migrator_util::kSplitTmpDir)
          .Append("TestFile"),
      kDataContent));
  MoveMigrator::TaskResult result_4 =
      MoveMigrator::PreMigrationCleanUp(original_profile_dir_4);
  ASSERT_EQ(result_4.status, MoveMigrator::TaskStatus::kSucceeded);
  EXPECT_FALSE(result_4.extra_bytes_required_to_be_freed.has_value());
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

  MoveMigrator::TaskResult result = MoveMigrator::SetupLacrosDir(
      original_profile_dir, std::move(progress_tracker), cancel_flag);
  ASSERT_EQ(result.status, MoveMigrator::TaskStatus::kSucceeded);

  const base::FilePath tmp_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);

  // Check chrome::kFirstRunSentinel, need copy item and lacros item exist in
  // lacros dir.
  EXPECT_TRUE(base::PathExists(tmp_user_dir.Append(chrome::kFirstRunSentinel)));
  EXPECT_TRUE(base::PathExists(tmp_profile_dir.Append(kSharedProtoDBPath)));
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
  ASSERT_EQ(MoveMigrator::MoveLacrosItemsToNewDir(original_profile_dir).status,
            MoveMigrator::TaskStatus::kSucceeded);

  EXPECT_FALSE(
      base::PathExists(original_profile_dir.Append(kBookmarksFilePath)));
  EXPECT_FALSE(base::PathExists(original_profile_dir.Append(kCookiesFilePath)));
  EXPECT_FALSE(base::PathExists(GetNigoriPath(original_profile_dir)));
  EXPECT_TRUE(base::PathExists(tmp_profile_dir.Append(kBookmarksFilePath)));
  EXPECT_TRUE(base::PathExists(tmp_profile_dir.Append(kCookiesFilePath)));
  EXPECT_TRUE(base::PathExists(
      tmp_profile_dir.Append(browser_data_migrator_util::kExtensionsFilePath)));
  EXPECT_TRUE(base::PathExists(
      tmp_profile_dir.Append(browser_data_migrator_util::kIndexedDBFilePath)));
  EXPECT_TRUE(base::PathExists(GetNigoriPath(tmp_profile_dir)));
}

TEST(MoveMigratorTest, MoveLacrosItemsToNewDirFailIfNoWritePermForLacrosItem) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath original_profile_dir = scoped_temp_dir.GetPath();
  SetUpProfileDirectory(original_profile_dir);

  // Remove write permission from a lacros item.
  base::SetPosixFilePermissions(original_profile_dir.Append(kBookmarksFilePath),
                                0500);

  MoveMigrator::TaskResult result =
      MoveMigrator::MoveLacrosItemsToNewDir(original_profile_dir);
  ASSERT_EQ(result.status,
            MoveMigrator::TaskStatus::kMoveLacrosItemsToNewDirNoWritePerm);
  ASSERT_TRUE(result.posix_errno.has_value());
  ASSERT_EQ(result.posix_errno.value(), EACCES);
}

TEST(MoveMigratorTest, SetupAshSplitDir) {
  using std::string_literals::operator""s;

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath original_profile_dir = scoped_temp_dir.GetPath();
  SetUpProfileDirectory(original_profile_dir);

  const base::FilePath tmp_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);
  ASSERT_TRUE(base::CreateDirectory(tmp_user_dir));
  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir));

  EXPECT_EQ(MoveMigrator::SetupAshSplitDir(original_profile_dir, 0).status,
            MoveMigrator::TaskStatus::kSucceeded);

  const base::FilePath tmp_split_dir =
      original_profile_dir.Append(browser_data_migrator_util::kSplitTmpDir);

  // Check `Extensions` is present in the split directory.
  base::FilePath path =
      tmp_split_dir.Append(browser_data_migrator_util::kExtensionsFilePath);
  EXPECT_TRUE(base::PathExists(path));
  // Check `Extensions` contains only extensions that have to stay in both Ash
  // and Lacros at this stage.
  std::string keep_extension_id =
      browser_data_migrator_util::kExtensionsAshOnly[0];
  std::string both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];
  EXPECT_FALSE(base::PathExists(path.Append(keep_extension_id)));
  EXPECT_TRUE(base::PathExists(path.Append(both_extension_id)));
  EXPECT_FALSE(base::PathExists(path.Append(kMoveExtensionId)));

  // Check `IndexedDB` contains only extensions that have to stay in both Ash
  // and Lacros at this stage.
  const auto [keep_extension_blob_path, keep_extension_leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(tmp_split_dir,
                                                    keep_extension_id.c_str());
  const auto [both_extension_blob_path, both_extension_leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(tmp_split_dir,
                                                    both_extension_id.c_str());
  const auto [move_extension_blob_path, move_extension_leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(tmp_split_dir,
                                                    kMoveExtensionId);
  EXPECT_FALSE(
      base::PathExists(keep_extension_blob_path.Append(kDataFilePath)));
  EXPECT_FALSE(
      base::PathExists(keep_extension_leveldb_path.Append(kDataFilePath)));
  EXPECT_TRUE(base::PathExists(both_extension_blob_path.Append(kDataFilePath)));
  EXPECT_TRUE(
      base::PathExists(both_extension_leveldb_path.Append(kDataFilePath)));
  EXPECT_FALSE(
      base::PathExists(move_extension_blob_path.Append(kDataFilePath)));
  EXPECT_FALSE(
      base::PathExists(move_extension_leveldb_path.Append(kDataFilePath)));

  // Check `Local Storage` is present in the split directory.
  path = tmp_split_dir.Append(browser_data_migrator_util::kLocalStorageFilePath)
             .Append(browser_data_migrator_util::kLocalStorageLeveldbName);
  EXPECT_TRUE(base::PathExists(path));
  // Check the content of the leveldb database. It should contain only
  // extensions in the keep list.
  auto db_map = ReadLevelDB(path);
  EXPECT_EQ(5u, db_map.size());
  EXPECT_EQ("1", db_map["VERSION"]);
  std::string key = "_chrome-extension://" + keep_extension_id + "\x00key"s;
  EXPECT_EQ("meta", db_map["META:chrome-extension://" + keep_extension_id]);
  EXPECT_EQ("value", db_map[key]);
  key = "_chrome-extension://" + both_extension_id + "\x00key"s;
  EXPECT_EQ("meta", db_map["META:chrome-extension://" + both_extension_id]);
  EXPECT_EQ("value", db_map[key]);

  // Check `Extension State` is present in the split directory.
  path = tmp_split_dir.Append(kExtensionStateFilePath);
  EXPECT_TRUE(base::PathExists(path));
  // Check the content of the leveldb database. It should contain only
  // extensions in the keep list.
  db_map = ReadLevelDB(path);
  EXPECT_EQ(2u, db_map.size());
  EXPECT_EQ("value", db_map[keep_extension_id + ".key"]);

  // Check Preferences is present in both tmp_profile_dir and tmp_split_dir.
  path = tmp_split_dir.Append(chrome::kPreferencesFilename);
  EXPECT_TRUE(base::PathExists(path));
  base::FilePath lacros_path =
      tmp_profile_dir.Append(chrome::kPreferencesFilename);
  EXPECT_TRUE(base::PathExists(lacros_path));

  // Check `Sync Data`/LevelDB is present in both tmp_profile_dir and
  // tmp_split_dir.
  path = tmp_split_dir.Append(browser_data_migrator_util::kSyncDataFilePath)
             .Append(browser_data_migrator_util::kSyncDataLeveldbName);
  EXPECT_TRUE(base::PathExists(path));
  lacros_path =
      tmp_profile_dir.Append(browser_data_migrator_util::kSyncDataFilePath)
          .Append(browser_data_migrator_util::kSyncDataLeveldbName);
  EXPECT_TRUE(base::PathExists(lacros_path));
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

TEST(MoveMigratorTest, RecordPosixErrnoUMA) {
  base::HistogramTester histogram_tester;

  MoveMigrator::RecordPosixErrnoUMA(
      MoveMigrator::TaskStatus::kMoveLacrosItemsToNewDirNoWritePerm, EPERM);

  std::string uma_name =
      "Ash.BrowserDataMigrator.MoveMigrator.PosixErrno."
      "MoveLacrosItemsToNewDirNoWritePerm";
  histogram_tester.ExpectBucketCount(uma_name, EPERM, 1);
}

class MoveMigratorMigrateTest : public ::testing::Test {
 public:
  MoveMigratorMigrateTest() : user_id_hash_("abcd") {}

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

    migrator_ = std::make_unique<MoveMigrator>(
        original_profile_dir_, user_id_hash_, std::move(progress_tracker),
        cancel_flag, &pref_service_, migrate_result_.GetCallback());

    MoveMigrator::RegisterLocalStatePrefs(pref_service_.registry());
  }

  void CheckProfileDirFinalState() {
    // Check that `original_profile_dir_` is as below as a result of the
    // migration.
    // |- Downloads
    // |- Extensions
    // |- IndexedDB
    // |- Local Storage
    // |- Login Data
    // |- shared_proto_db
    // |- Preferences
    // |- Storage/
    // |- Sync Data/LevelDB
    // |- lacros/First Run
    // |- lacros/Default/
    //     |- Bookmarks
    //     |- Cookies
    //     |- Extensions
    //     |- IndexedDB
    //     |- Local Storage
    //     |- shared_proto_db
    //     |- Preferences
    //     |- Storage/
    //     |- Sync Data/
    //         |- LevelDB
    //         |- Nigori.bin

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
        base::PathExists(original_profile_dir_.Append(kSharedProtoDBPath)));
    EXPECT_TRUE(base::PathExists(new_profile_dir.Append(kSharedProtoDBPath)));

    EXPECT_TRUE(
        base::PathExists(original_profile_dir_.Append(kDownloadsFilePath)));
    EXPECT_FALSE(base::PathExists(new_profile_dir.Append(kDownloadsFilePath)));

    EXPECT_FALSE(
        base::PathExists(original_profile_dir_.Append(kCacheFilePath)));
    EXPECT_FALSE(base::PathExists(new_profile_dir.Append(kCacheFilePath)));

    // Extensions.
    std::string keep_extension_id =
        browser_data_migrator_util::kExtensionsAshOnly[0];
    std::string both_extension_id =
        browser_data_migrator_util::kExtensionsBothChromes[0];
    EXPECT_TRUE(base::PathExists(
        original_profile_dir_
            .Append(browser_data_migrator_util::kExtensionsFilePath)
            .Append(keep_extension_id)));
    EXPECT_TRUE(base::PathExists(
        original_profile_dir_
            .Append(browser_data_migrator_util::kExtensionsFilePath)
            .Append(both_extension_id)));
    EXPECT_FALSE(base::PathExists(
        original_profile_dir_
            .Append(browser_data_migrator_util::kExtensionsFilePath)
            .Append(kMoveExtensionId)));
    EXPECT_TRUE(base::PathExists(
        new_profile_dir.Append(browser_data_migrator_util::kExtensionsFilePath)
            .Append(kMoveExtensionId)));

    // Storage.
    EXPECT_TRUE(base::PathExists(
        original_profile_dir_
            .Append(browser_data_migrator_util::kStorageFilePath)
            .Append(browser_data_migrator_util::kStorageExtFilePath)
            .Append(keep_extension_id)));
    EXPECT_TRUE(base::PathExists(
        original_profile_dir_
            .Append(browser_data_migrator_util::kStorageFilePath)
            .Append(browser_data_migrator_util::kStorageExtFilePath)
            .Append(both_extension_id)));
    EXPECT_FALSE(base::PathExists(
        original_profile_dir_
            .Append(browser_data_migrator_util::kStorageFilePath)
            .Append(browser_data_migrator_util::kStorageExtFilePath)
            .Append(kMoveExtensionId)));
    EXPECT_TRUE(base::PathExists(
        new_profile_dir.Append(browser_data_migrator_util::kStorageFilePath)
            .Append(browser_data_migrator_util::kStorageExtFilePath)
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
    EXPECT_EQ(5u, ash_local_storage.size());
    // Lacros contains all the keys.
    auto lacros_local_storage = ReadLevelDB(lacros_local_storage_path);
    EXPECT_EQ(7u, lacros_local_storage.size());

    // Ash contains only IndexedDB folders of extensions in keeplist.
    {
      const auto [keep_extension_blob_path, keep_extension_leveldb_path] =
          browser_data_migrator_util::GetIndexedDBPaths(
              original_profile_dir_, keep_extension_id.c_str());
      const auto [both_extension_blob_path, both_extension_leveldb_path] =
          browser_data_migrator_util::GetIndexedDBPaths(
              original_profile_dir_, both_extension_id.c_str());
      const auto [move_extension_blob_path, move_extension_leveldb_path] =
          browser_data_migrator_util::GetIndexedDBPaths(original_profile_dir_,
                                                        kMoveExtensionId);
      EXPECT_TRUE(
          base::PathExists(keep_extension_blob_path.Append(kDataFilePath)));
      EXPECT_TRUE(
          base::PathExists(keep_extension_leveldb_path.Append(kDataFilePath)));
      EXPECT_TRUE(
          base::PathExists(both_extension_blob_path.Append(kDataFilePath)));
      EXPECT_TRUE(
          base::PathExists(both_extension_leveldb_path.Append(kDataFilePath)));
      EXPECT_FALSE(base::PathExists(move_extension_blob_path));
      EXPECT_FALSE(base::PathExists(move_extension_leveldb_path));
    }

    // Lacros contains only IndexedDB folders of extensions not in keeplist.
    {
      const auto [keep_extension_blob_path, keep_extension_leveldb_path] =
          browser_data_migrator_util::GetIndexedDBPaths(
              new_profile_dir, keep_extension_id.c_str());
      const auto [both_extension_blob_path, both_extension_leveldb_path] =
          browser_data_migrator_util::GetIndexedDBPaths(
              original_profile_dir_, both_extension_id.c_str());
      const auto [move_extension_blob_path, move_extension_leveldb_path] =
          browser_data_migrator_util::GetIndexedDBPaths(new_profile_dir,
                                                        kMoveExtensionId);
      EXPECT_FALSE(base::PathExists(keep_extension_blob_path));
      EXPECT_FALSE(base::PathExists(keep_extension_leveldb_path));
      EXPECT_TRUE(
          base::PathExists(both_extension_blob_path.Append(kDataFilePath)));
      EXPECT_TRUE(
          base::PathExists(both_extension_leveldb_path.Append(kDataFilePath)));
      EXPECT_TRUE(
          base::PathExists(move_extension_blob_path.Append(kDataFilePath)));
      EXPECT_TRUE(
          base::PathExists(move_extension_leveldb_path.Append(kDataFilePath)));
    }

    // Preferences.
    const base::FilePath ash_preferences_path =
        original_profile_dir_.Append(chrome::kPreferencesFilename);
    const base::FilePath lacros_preferences_path =
        new_profile_dir.Append(chrome::kPreferencesFilename);
    EXPECT_TRUE(base::PathExists(ash_preferences_path));
    EXPECT_TRUE(base::PathExists(lacros_preferences_path));

    // Sync Data/LevelDB.
    const base::FilePath ash_syncdata_leveldb_path =
        original_profile_dir_
            .Append(browser_data_migrator_util::kSyncDataFilePath)
            .Append(browser_data_migrator_util::kSyncDataLeveldbName);
    const base::FilePath lacros_syncdata_leveldb_path =
        new_profile_dir.Append(browser_data_migrator_util::kSyncDataFilePath)
            .Append(browser_data_migrator_util::kSyncDataLeveldbName);
    EXPECT_TRUE(base::PathExists(ash_syncdata_leveldb_path));
    EXPECT_TRUE(base::PathExists(lacros_syncdata_leveldb_path));

    // Sync Data/Nigori.bin
    EXPECT_FALSE(base::PathExists(GetNigoriPath(original_profile_dir_)));
    EXPECT_TRUE(base::PathExists(GetNigoriPath(new_profile_dir)));
  }

  void TearDown() override { EXPECT_TRUE(scoped_temp_dir_.Delete()); }

  base::test::TaskEnvironment task_environment_;
  base::test::TestFuture<BrowserDataMigratorImpl::MigrationResult>
      migrate_result_;
  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath original_profile_dir_;
  TestingPrefServiceSimple pref_service_;
  std::string user_id_hash_;
  std::unique_ptr<MoveMigrator> migrator_;
};

TEST_F(MoveMigratorMigrateTest, Migrate) {
  migrator_->Migrate();

  EXPECT_EQ(migrate_result_.Get().data_wipe_result,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(migrate_result_.Get().data_migration_result.kind,
            BrowserDataMigrator::ResultKind::kSucceeded);

  CheckProfileDirFinalState();
}

TEST_F(MoveMigratorMigrateTest, MigrateResumeFromMoveLacrosItems) {
  MoveMigrator::SetResumeStep(&pref_service_, user_id_hash_,
                              MoveMigrator::ResumeStep::kMoveLacrosItems);

  // Setup `original_profile_dir_` as below.
  // |- Cookies
  // |- Downloads
  // |- shared_proto_db
  // |- move_migrator/First Run
  // |- move_migrator/Default/
  //     |- Bookmarks
  //     |- Extensions
  //     |- IndexedDB
  //     |- Local Storage
  //     |- shared_proto_db
  //     |- Preferences
  //     |- Storage/
  //     |- Sync Data/
  //         |- LevelDB
  //         |- Nigori.bin
  // |- move_migrator_split/
  //     |- Extensions
  //     |- IndexedDB
  //     |- Local Storage
  //     |- Preferences
  //     |- Storage/
  //     |- Sync Data/LevelDB

  const base::FilePath tmp_user_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);
  const base::FilePath tmp_split_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kSplitTmpDir);
  ASSERT_TRUE(base::DeletePathRecursively(
      original_profile_dir_.Append(kCacheFilePath)));

  ASSERT_TRUE(base::CreateDirectory(tmp_user_dir));
  ASSERT_TRUE(base::WriteFile(tmp_user_dir.Append(chrome::kFirstRunSentinel),
                              base::StringPiece()));
  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir));
  ASSERT_TRUE(base::CreateDirectory(tmp_split_dir));
  ASSERT_TRUE(
      base::CopyDirectory(original_profile_dir_.Append(kSharedProtoDBPath),
                          tmp_profile_dir.Append(kSharedProtoDBPath),
                          /*recursive=*/true));
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kBookmarksFilePath),
                         tmp_profile_dir.Append(kBookmarksFilePath)));

  ASSERT_TRUE(base::CreateDirectory(
      tmp_profile_dir.Append(browser_data_migrator_util::kSyncDataFilePath)));
  ASSERT_TRUE(base::Move(GetNigoriPath(original_profile_dir_),
                         GetNigoriPath(tmp_profile_dir)));

  // Extensions that have to stay in both Ash and Lacros were copied to the
  // split dir.
  SetUpExtensions(tmp_split_dir, /*ash=*/false, /*lacros=*/false,
                  /*both=*/true);
  // Extensions have been moved to Lacros's tmp dir.
  ASSERT_TRUE(base::Move(
      original_profile_dir_.Append(
          browser_data_migrator_util::kExtensionsFilePath),
      tmp_profile_dir.Append(browser_data_migrator_util::kExtensionsFilePath)));

  // Storage objects that have to stay in both Ash and Lacros were copied to the
  // split dir.
  SetUpStorage(tmp_split_dir, /*ash=*/false, /*lacros=*/false,
               /*both=*/true);
  // Storage objects have been moved to Lacros's tmp dir.
  ASSERT_TRUE(base::Move(
      original_profile_dir_.Append(
          browser_data_migrator_util::kStorageFilePath),
      tmp_profile_dir.Append(browser_data_migrator_util::kStorageFilePath)));

  // IndexedDB objects that have to stay in both Ash and Lacros were copied to
  // the split dir.
  SetUpIndexedDB(tmp_split_dir, /*ash=*/false, /*lacros=*/false, /*both=*/true);
  // IndexedDB has been moved to Lacros's tmp dir.
  ASSERT_TRUE(base::Move(
      original_profile_dir_.Append(
          browser_data_migrator_util::kIndexedDBFilePath),
      tmp_profile_dir.Append(browser_data_migrator_util::kIndexedDBFilePath)));

  // Local Storage has been split.
  ASSERT_TRUE(
      base::Move(original_profile_dir_.Append(
                     browser_data_migrator_util::kLocalStorageFilePath),
                 tmp_profile_dir.Append(
                     browser_data_migrator_util::kLocalStorageFilePath)));
  SetUpLocalStorage(tmp_split_dir, /*ash_only=*/true);

  // Preferences has been split.
  SetUpPreferences(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpPreferences(tmp_split_dir, /*ash=*/true, /*lacros=*/false);

  // `Sync Data`/LevelDB has been split.
  SetUpSyncDataLevelDB(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpSyncDataLevelDB(tmp_split_dir, /*ash=*/true, /*lacros=*/false);

  migrator_->Migrate();

  EXPECT_EQ(migrate_result_.Get().data_wipe_result,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(migrate_result_.Get().data_migration_result.kind,
            BrowserDataMigrator::ResultKind::kSucceeded);

  CheckProfileDirFinalState();
}

TEST_F(MoveMigratorMigrateTest, MigrateResumeFromMoveSplitItems) {
  MoveMigrator::SetResumeStep(&pref_service_, user_id_hash_,
                              MoveMigrator::ResumeStep::kMoveSplitItems);

  // Setup `original_profile_dir_` as below.
  // |- Downloads
  // |- shared_proto_db
  // |- move_migrator/First Run
  // |- move_migrator/Default/
  //     |- Bookmarks
  //     |- Cookies
  //     |- Extensions
  //     |- IndexedDB
  //     |- Local Storage
  //     |- shared_proto_db
  //     |- Preferences
  //     |- Storage/
  //     |- Sync Data/
  //         |- LevelDB
  //         |- Nigori.bin
  // |- move_migrator_split/
  //     |- Extensions
  //     |- IndexedDB
  //     |- Local Storage
  //     |- Preferences
  //     |- Storage/
  //     |- Sync Data/LevelDB

  const base::FilePath tmp_user_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);
  const base::FilePath tmp_split_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kSplitTmpDir);
  ASSERT_TRUE(base::DeletePathRecursively(
      original_profile_dir_.Append(kCacheFilePath)));

  ASSERT_TRUE(base::CreateDirectory(tmp_user_dir));
  ASSERT_TRUE(base::WriteFile(tmp_user_dir.Append(chrome::kFirstRunSentinel),
                              base::StringPiece()));
  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir));
  ASSERT_TRUE(base::CreateDirectory(tmp_split_dir));
  ASSERT_TRUE(
      base::CopyDirectory(original_profile_dir_.Append(kSharedProtoDBPath),
                          tmp_profile_dir.Append(kSharedProtoDBPath),
                          /*recursive=*/true));
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kBookmarksFilePath),
                         tmp_profile_dir.Append(kBookmarksFilePath)));
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kCookiesFilePath),
                         tmp_profile_dir.Append(kCookiesFilePath)));

  ASSERT_TRUE(base::CreateDirectory(
      tmp_profile_dir.Append(browser_data_migrator_util::kSyncDataFilePath)));
  ASSERT_TRUE(base::Move(GetNigoriPath(original_profile_dir_),
                         GetNigoriPath(tmp_profile_dir)));

  // Extensions that have to stay in both Ash and Lacros were copied to the
  // split dir.
  SetUpExtensions(tmp_split_dir, /*ash=*/false, /*lacros=*/false,
                  /*both=*/true);
  // Extensions have been moved to Lacros's tmp dir, but not yet split and moved
  // to Ash profile dir.
  ASSERT_TRUE(base::Move(
      original_profile_dir_.Append(
          browser_data_migrator_util::kExtensionsFilePath),
      tmp_profile_dir.Append(browser_data_migrator_util::kExtensionsFilePath)));

  // Storage objects that have to stay in both Ash and Lacros were copied to the
  // split dir.
  SetUpStorage(tmp_split_dir, /*ash=*/false, /*lacros=*/false,
               /*both=*/true);
  // Storage objects have been moved to Lacros's tmp dir, but not yet split and
  // moved to Ash profile dir.
  ASSERT_TRUE(base::Move(
      original_profile_dir_.Append(
          browser_data_migrator_util::kStorageFilePath),
      tmp_profile_dir.Append(browser_data_migrator_util::kStorageFilePath)));

  // IndexedDB objects that have to stay in both Ash and Lacros were copied to
  // the split dir.
  SetUpIndexedDB(tmp_split_dir, /*ash=*/false, /*lacros=*/false, /*both=*/true);
  // IndexedDB objects have been moved to Lacros's tmp dir, but not yet split
  // and moved to Ash profile dir.
  ASSERT_TRUE(base::Move(
      original_profile_dir_.Append(
          browser_data_migrator_util::kIndexedDBFilePath),
      tmp_profile_dir.Append(browser_data_migrator_util::kIndexedDBFilePath)));

  // Local Storage has been split, but not yet moved to Ash profile dir.
  ASSERT_TRUE(
      base::Move(original_profile_dir_.Append(
                     browser_data_migrator_util::kLocalStorageFilePath),
                 tmp_profile_dir.Append(
                     browser_data_migrator_util::kLocalStorageFilePath)));
  SetUpLocalStorage(tmp_split_dir, /*ash_only=*/true);

  // Preferences has been split, but not yet moved to Ash profile dir.
  SetUpPreferences(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpPreferences(tmp_split_dir, /*ash=*/true, /*lacros=*/false);

  // `Sync Data`/LevelDB has been split, but not yet moved to Ash profile dir.
  SetUpSyncDataLevelDB(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpSyncDataLevelDB(tmp_split_dir, /*ash=*/true, /*lacros=*/false);

  migrator_->Migrate();

  EXPECT_EQ(migrate_result_.Get().data_wipe_result,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(migrate_result_.Get().data_migration_result.kind,
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
  // |- shared_proto_db
  // |- Preferences
  // |- Storage/
  // |- Sync Data/LevelDB
  // |- move_migrator/First Run
  // |- move_migrator/Default/
  //     |- Bookmarks
  //     |- Cookies
  //     |- Extensions
  //     |- Local Storage
  //     |- shared_proto_db
  //     |- Preferences
  //     |- Storage/
  //     |- Sync Data/
  //         |- LevelDB
  //         |- Nigori.bin

  const base::FilePath tmp_user_dir =
      original_profile_dir_.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);
  ASSERT_TRUE(base::DeletePathRecursively(
      original_profile_dir_.Append(kCacheFilePath)));

  ASSERT_TRUE(base::CreateDirectory(tmp_user_dir));
  ASSERT_TRUE(base::WriteFile(tmp_user_dir.Append(chrome::kFirstRunSentinel),
                              base::StringPiece()));
  ASSERT_TRUE(base::CreateDirectory(tmp_profile_dir));
  ASSERT_TRUE(
      base::CopyDirectory(original_profile_dir_.Append(kSharedProtoDBPath),
                          tmp_profile_dir.Append(kSharedProtoDBPath),
                          /*recursive=*/true));
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kBookmarksFilePath),
                         tmp_profile_dir.Append(kBookmarksFilePath)));
  ASSERT_TRUE(base::Move(original_profile_dir_.Append(kCookiesFilePath),
                         tmp_profile_dir.Append(kCookiesFilePath)));

  ASSERT_TRUE(base::CreateDirectory(
      tmp_profile_dir.Append(browser_data_migrator_util::kSyncDataFilePath)));
  ASSERT_TRUE(base::Move(GetNigoriPath(original_profile_dir_),
                         GetNigoriPath(tmp_profile_dir)));

  // Extensions have been split, and Ash's version is in its final place.
  ASSERT_TRUE(base::DeletePathRecursively(original_profile_dir_.Append(
      browser_data_migrator_util::kExtensionsFilePath)));
  SetUpExtensions(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpExtensions(original_profile_dir_, /*ash=*/true, /*lacros=*/false);

  // Storage objects have been split, and Ash's version is in its final place.
  ASSERT_TRUE(base::DeletePathRecursively(original_profile_dir_.Append(
      browser_data_migrator_util::kStorageFilePath)));
  SetUpStorage(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpStorage(original_profile_dir_, /*ash=*/true, /*lacros=*/false);

  // IndexedDB has been split, and Ash's version is in its final place.
  ASSERT_TRUE(base::DeletePathRecursively(original_profile_dir_.Append(
      browser_data_migrator_util::kIndexedDBFilePath)));
  SetUpIndexedDB(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpIndexedDB(original_profile_dir_, /*ash=*/true, /*lacros=*/false);

  // Local Storage has been split, and Ash's version is in its final place.
  ASSERT_TRUE(
      base::Move(original_profile_dir_.Append(
                     browser_data_migrator_util::kLocalStorageFilePath),
                 tmp_profile_dir.Append(
                     browser_data_migrator_util::kLocalStorageFilePath)));
  SetUpLocalStorage(original_profile_dir_, /*ash_only=*/true);

  // Preferences has been split.
  SetUpPreferences(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpPreferences(original_profile_dir_, /*ash=*/true, /*lacros=*/false);

  // `Sync Data`/LevelDB has been split.
  SetUpSyncDataLevelDB(tmp_profile_dir, /*ash=*/false, /*lacros=*/true);
  SetUpSyncDataLevelDB(original_profile_dir_, /*ash=*/true, /*lacros=*/false);

  migrator_->Migrate();

  EXPECT_EQ(migrate_result_.Get().data_wipe_result,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(migrate_result_.Get().data_migration_result.kind,
            BrowserDataMigrator::ResultKind::kSucceeded);

  CheckProfileDirFinalState();
}

TEST_F(MoveMigratorMigrateTest, MigrateOutOfDisk) {
  // Emulate the situation of out-of-disk.
  browser_data_migrator_util::ScopedExtraBytesRequiredToBeFreedForTesting
      scoped_extra_bytes(100);

  migrator_->Migrate();

  EXPECT_EQ(migrate_result_.Get().data_wipe_result,
            BrowserDataMigratorImpl::DataWipeResult::kSucceeded);
  EXPECT_EQ(migrate_result_.Get().data_migration_result.kind,
            BrowserDataMigrator::ResultKind::kFailed);
  EXPECT_EQ(100u, migrate_result_.Get().data_migration_result.required_size);
}

}  // namespace ash
