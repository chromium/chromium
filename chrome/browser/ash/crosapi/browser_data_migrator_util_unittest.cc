// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"

#include <sys/stat.h>

#include <map>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/crosapi/fake_migration_progress_tracker.h"
#include "chrome/common/chrome_constants.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/blocking_model_type_store_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace ash::browser_data_migrator_util {

namespace {

constexpr char kDownloadsPath[] = "Downloads";
constexpr char kSharedProtoDBPath[] = "shared_proto_db";
constexpr char kBookmarksPath[] = "Bookmarks";
constexpr char kCookiesPath[] = "Cookies";
constexpr char kCachePath[] = "Cache";
constexpr char kCodeCachePath[] = "Code Cache";
constexpr char kCodeCacheUMAName[] = "CodeCache";
constexpr base::StringPiece kTextFileContent = "Hello, World!";
constexpr char kMoveExtensionId[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

constexpr syncer::ModelType kAshSyncDataType =
    browser_data_migrator_util::kAshOnlySyncDataTypes[0];
constexpr syncer::ModelType kLacrosSyncDataType = syncer::ModelType::WEB_APPS;
static_assert(!base::Contains(browser_data_migrator_util::kAshOnlySyncDataTypes,
                              kLacrosSyncDataType));

struct TargetItemComparator {
  bool operator()(const TargetItem& t1, const TargetItem& t2) const {
    return t1.path < t2.path;
  }
};

// Checks if the file paths point to the same inode.
bool IsSameFile(const base::FilePath& file1, const base::FilePath& file2) {
  struct stat st_1;
  if (stat(file1.value().c_str(), &st_1) == -1) {
    PLOG(ERROR) << "stat failed";
    return false;
  }

  struct stat st_2;
  if (stat(file2.value().c_str(), &st_2) == -1) {
    PLOG(ERROR) << "stat failed";
    return false;
  }

  // Make sure that they are indeed the same file.
  return (st_1.st_ino == st_2.st_ino);
}

std::set<std::string> CollectDictKeys(const base::Value::Dict* dict) {
  std::set<std::string> result;
  if (dict) {
    for (const auto entry : *dict) {
      result.insert(entry.first);
    }
  }
  return result;
}

// Prepare LocalStorage-like LevelDB.
void SetUpLocalStorage(const base::FilePath& path,
                       std::unique_ptr<leveldb::DB>& db) {
  using std::string_literals::operator""s;

  leveldb_env::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb_env::OpenDB(options, path.value(), &db);
  ASSERT_TRUE(status.ok());

  leveldb::WriteBatch batch;
  batch.Put("VERSION", "1");

  std::string keep_extension_id =
      browser_data_migrator_util::kExtensionsAshOnly[0];
  batch.Put("META:chrome-extension://" + keep_extension_id, "meta");
  batch.Put("_chrome-extension://" + keep_extension_id + "\x00key1"s, "value1");

  std::string both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];
  batch.Put("META:chrome-extension://" + both_extension_id, "meta");
  batch.Put("_chrome-extension://" + both_extension_id + "\x00key1"s, "value1");

  std::string move_extension_id = kMoveExtensionId;
  batch.Put("META:chrome-extension://" + move_extension_id, "meta");
  batch.Put("_chrome-extension://" + move_extension_id + "\x00key1"s, "value1");

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = db->Write(write_options, &batch);
  ASSERT_TRUE(status.ok());
}

// Prepare StateStore-like LevelDB.
void SetUpStateStore(const base::FilePath& path,
                     std::unique_ptr<leveldb::DB>& db) {
  leveldb_env::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb_env::OpenDB(options, path.value(), &db);
  ASSERT_TRUE(status.ok());

  leveldb::WriteBatch batch;
  std::string keep_extension_id =
      browser_data_migrator_util::kExtensionsAshOnly[0];
  std::string both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];
  batch.Put(keep_extension_id + ".key1", "value1");
  batch.Put(keep_extension_id + ".key2", "value2");
  batch.Put(both_extension_id + ".key1", "value1");
  batch.Put(both_extension_id + ".key2", "value2");
  batch.Put(std::string(kMoveExtensionId) + ".key1", "value1");
  batch.Put(std::string(kMoveExtensionId) + ".key2", "value2");

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = db->Write(write_options, &batch);
  ASSERT_TRUE(status.ok());
}

// Prepare Sync Data LevelDB.
void SetUpSyncData(const base::FilePath& path,
                   std::unique_ptr<leveldb::DB>& db) {
  leveldb_env::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb_env::OpenDB(options, path.value(), &db);
  ASSERT_TRUE(status.ok());

  leveldb::WriteBatch batch;
  batch.Put(syncer::FormatDataPrefix(kAshSyncDataType,
                                     syncer::StorageType::kUnspecified) +
                kMoveExtensionId,
            "ash_data");
  batch.Put(syncer::FormatMetaPrefix(kAshSyncDataType,
                                     syncer::StorageType::kUnspecified) +
                kMoveExtensionId,
            "ash_metadata");
  batch.Put(syncer::FormatGlobalMetadataKey(kAshSyncDataType,
                                            syncer::StorageType::kUnspecified),
            "ash_globalmetadata");

  batch.Put(syncer::FormatDataPrefix(kLacrosSyncDataType,
                                     syncer::StorageType::kUnspecified) +
                kMoveExtensionId,
            "lacros_data");
  batch.Put(syncer::FormatMetaPrefix(kLacrosSyncDataType,
                                     syncer::StorageType::kUnspecified) +
                kMoveExtensionId,
            "lacros_metadata");
  batch.Put(syncer::FormatGlobalMetadataKey(kLacrosSyncDataType,
                                            syncer::StorageType::kUnspecified),
            "lacros_globalmetadata");

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = db->Write(write_options, &batch);
  ASSERT_TRUE(status.ok());
}

// Return all the key-value pairs in a LevelDB.
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

TEST(BrowserDataMigratorUtilTest, NoPathOverlaps) {
  base::span<const char* const> remain_in_ash_paths =
      base::make_span(kRemainInAshDataPaths);
  base::span<const char* const> lacros_data_paths =
      base::make_span(kLacrosDataPaths);
  base::span<const char* const> deletable_paths =
      base::make_span(kDeletablePaths);
  base::span<const char* const> common_data_paths =
      base::make_span(kNeedCopyForMoveDataPaths);

  std::vector<base::span<const char* const>> paths_groups{
      remain_in_ash_paths, lacros_data_paths, deletable_paths,
      common_data_paths};

  auto overlap_checker = [](base::span<const char* const> paths_group_a,
                            base::span<const char* const> paths_group_b) {
    for (const char* path_a : paths_group_a) {
      for (const char* path_b : paths_group_b) {
        if (base::StringPiece(path_a) == base::StringPiece(path_b)) {
          LOG(ERROR) << "The following path appears in multiple sets: "
                     << path_a;
          return false;
        }
      }
    }

    return true;
  };

  for (size_t i = 0; i < paths_groups.size() - 1; i++) {
    for (size_t j = i + 1; j < paths_groups.size(); j++) {
      SCOPED_TRACE(base::StringPrintf("i %zu j %zu", i, j));
      EXPECT_TRUE(overlap_checker(paths_groups[i], paths_groups[j]));
    }
  }
}

TEST(BrowserDataMigratorUtilTest, ComputeDirectorySizeWithoutLinks) {
  base::ScopedTempDir dir_1;
  ASSERT_TRUE(dir_1.CreateUniqueTempDir());

  ASSERT_TRUE(base::WriteFile(
      dir_1.GetPath().Append(FILE_PATH_LITERAL("file1")), kTextFileContent));
  ASSERT_TRUE(
      base::CreateDirectory(dir_1.GetPath().Append(FILE_PATH_LITERAL("dir"))));
  ASSERT_TRUE(base::WriteFile(dir_1.GetPath()
                                  .Append(FILE_PATH_LITERAL("dir"))
                                  .Append(FILE_PATH_LITERAL("file2")),
                              kTextFileContent));

  // Check that `ComputeDirectorySizeWithoutLinks` returns the sum of sizes of
  // the two files in the directory.
  EXPECT_EQ(ComputeDirectorySizeWithoutLinks(dir_1.GetPath()),
            static_cast<int>(kTextFileContent.size() * 2));

  base::ScopedTempDir dir_2;
  ASSERT_TRUE(dir_2.CreateUniqueTempDir());
  ASSERT_TRUE(base::WriteFile(
      dir_2.GetPath().Append(FILE_PATH_LITERAL("file3")), kTextFileContent));

  ASSERT_TRUE(CreateSymbolicLink(
      dir_2.GetPath().Append(FILE_PATH_LITERAL("file3")),
      dir_1.GetPath().Append(FILE_PATH_LITERAL("link_to_file3"))));

  // Check that `ComputeDirectorySizeWithoutLinks` does not follow symlinks from
  // `dir_1` to `dir_2`.
  EXPECT_EQ(ComputeDirectorySizeWithoutLinks(dir_1.GetPath()),
            static_cast<int>(kTextFileContent.size() * 2));
}

TEST(BrowserDataMigratorUtilTest, GetUMAItemName) {
  base::FilePath profile_data_dir("/home/chronos/user");

  EXPECT_STREQ(GetUMAItemName(profile_data_dir.Append(kCodeCachePath)).c_str(),
               kCodeCacheUMAName);

  EXPECT_STREQ(
      GetUMAItemName(profile_data_dir.Append(FILE_PATH_LITERAL("abcd")))
          .c_str(),
      kUnknownUMAName);
}

TEST(BrowserDataMigratorUtilTest, GetExtensionKeys) {
  using std::string_literals::operator""s;

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  // Prepare LocalStorage-like LevelDB.
  std::unique_ptr<leveldb::DB> db;
  SetUpLocalStorage(
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("localstorage")), db);

  ExtensionKeys keys;
  leveldb::Status status =
      GetExtensionKeys(db.get(), LevelDBType::kLocalStorage, &keys);
  EXPECT_TRUE(status.ok());
  db.reset();

  std::string keep_extension_id =
      browser_data_migrator_util::kExtensionsAshOnly[0];
  std::string both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];
  ExtensionKeys expected_keys = {
      {keep_extension_id,
       {
           "META:chrome-extension://" + keep_extension_id,
           "_chrome-extension://" + keep_extension_id + "\x00key1"s,
       }},
      {both_extension_id,
       {
           "META:chrome-extension://" + both_extension_id,
           "_chrome-extension://" + both_extension_id + "\x00key1"s,
       }},
      {kMoveExtensionId,
       {
           "META:chrome-extension://" + std::string(kMoveExtensionId),
           "_chrome-extension://" + std::string(kMoveExtensionId) + "\x00key1"s,
       }},
  };
  EXPECT_EQ(expected_keys, keys);
  keys.clear();

  // Prepare StateStore-like LevelDB.
  SetUpStateStore(
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("statestore")), db);

  status = GetExtensionKeys(db.get(), LevelDBType::kStateStore, &keys);
  EXPECT_TRUE(status.ok());

  expected_keys = {
      {keep_extension_id,
       {
           keep_extension_id + ".key1",
           keep_extension_id + ".key2",
       }},
      {both_extension_id,
       {
           both_extension_id + ".key1",
           both_extension_id + ".key2",
       }},
      {kMoveExtensionId,
       {
           std::string(kMoveExtensionId) + ".key1",
           std::string(kMoveExtensionId) + ".key2",
       }},
  };
  EXPECT_EQ(expected_keys, keys);
}

TEST(BrowserDataMigratorUtilTest, MigrateLevelDB) {
  using std::string_literals::operator""s;

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  // Prepare LocalStorage-like LevelDB.
  std::unique_ptr<leveldb::DB> db;
  const base::FilePath localstorage_db_path =
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("localstorage"));
  const base::FilePath localstorage_new_db_path =
      localstorage_db_path.AddExtension(".new");
  SetUpLocalStorage(localstorage_db_path, db);
  db.reset();

  EXPECT_TRUE(MigrateLevelDB(localstorage_db_path, localstorage_new_db_path,
                             LevelDBType::kLocalStorage));

  leveldb_env::Options options;
  options.create_if_missing = false;
  leveldb::Status status =
      leveldb_env::OpenDB(options, localstorage_new_db_path.value(), &db);
  EXPECT_TRUE(status.ok());

  ExtensionKeys keys;
  status = GetExtensionKeys(db.get(), LevelDBType::kLocalStorage, &keys);
  EXPECT_TRUE(status.ok());
  db.reset();

  std::string keep_extension_id =
      browser_data_migrator_util::kExtensionsAshOnly[0];
  std::string both_extension_id =
      browser_data_migrator_util::kExtensionsBothChromes[0];
  ExtensionKeys expected_keys = {
      {keep_extension_id,
       {
           "META:chrome-extension://" + keep_extension_id,
           "_chrome-extension://" + keep_extension_id + "\x00key1"s,
       }},
      {both_extension_id,
       {
           "META:chrome-extension://" + both_extension_id,
           "_chrome-extension://" + both_extension_id + "\x00key1"s,
       }},
  };
  EXPECT_EQ(expected_keys, keys);
  keys.clear();

  // Prepare StateStore-like LevelDB.
  const base::FilePath statestore_db_path =
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("statestore"));
  const base::FilePath statestore_new_db_path =
      statestore_db_path.AddExtension(".new");
  SetUpStateStore(statestore_db_path, db);
  db.reset();

  EXPECT_TRUE(MigrateLevelDB(statestore_db_path, statestore_new_db_path,
                             LevelDBType::kStateStore));

  status = leveldb_env::OpenDB(options, statestore_new_db_path.value(), &db);
  EXPECT_TRUE(status.ok());

  status = GetExtensionKeys(db.get(), LevelDBType::kStateStore, &keys);
  EXPECT_TRUE(status.ok());

  expected_keys = {
      {keep_extension_id,
       {
           keep_extension_id + ".key1",
           keep_extension_id + ".key2",
       }},
      {both_extension_id,
       {
           both_extension_id + ".key1",
           both_extension_id + ".key2",
       }},
  };
  EXPECT_EQ(expected_keys, keys);
}

TEST(BrowserDataMigratorUtilTest, MigrateSyncDataLevelDB) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  // Prepare Sync Data LevelDB.
  std::unique_ptr<leveldb::DB> db;
  const base::FilePath db_path =
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("syncdata"));
  SetUpSyncData(db_path, db);
  db.reset();

  // Migrate Sync Data.
  const base::FilePath ash_db_path = db_path.AddExtension(".ash");
  const base::FilePath lacros_db_path = db_path.AddExtension(".lacros");
  EXPECT_TRUE(MigrateSyncDataLevelDB(db_path, ash_db_path, lacros_db_path));

  // Check resulting Ash database.
  auto ash_db_map = ReadLevelDB(ash_db_path);
  std::map<std::string, std::string> expected_ash_db_map = {
      {syncer::FormatDataPrefix(kAshSyncDataType,
                                syncer::StorageType::kUnspecified) +
           kMoveExtensionId,
       "ash_data"},
      {syncer::FormatMetaPrefix(kAshSyncDataType,
                                syncer::StorageType::kUnspecified) +
           kMoveExtensionId,
       "ash_metadata"},
      {syncer::FormatGlobalMetadataKey(kAshSyncDataType,
                                       syncer::StorageType::kUnspecified),
       "ash_globalmetadata"},
  };
  EXPECT_EQ(expected_ash_db_map, ash_db_map);

  // Check resulting Lacros database.
  auto lacros_db_map = ReadLevelDB(lacros_db_path);
  std::map<std::string, std::string> expected_lacros_db_map = {
      {syncer::FormatDataPrefix(kLacrosSyncDataType,
                                syncer::StorageType::kUnspecified) +
           kMoveExtensionId,
       "lacros_data"},
      {syncer::FormatMetaPrefix(kLacrosSyncDataType,
                                syncer::StorageType::kUnspecified) +
           kMoveExtensionId,
       "lacros_metadata"},
      {syncer::FormatGlobalMetadataKey(kLacrosSyncDataType,
                                       syncer::StorageType::kUnspecified),
       "lacros_globalmetadata"},
  };
  EXPECT_EQ(expected_lacros_db_map, lacros_db_map);
}

TEST(BrowserDataMigratorUtilTest, RecordUserDataSize) {
  base::HistogramTester histogram_tester;

  base::FilePath profile_data_dir("/home/chronos/user");
  // Size in bytes.
  int64_t size = 4 * 1024 * 1024;
  RecordUserDataSize(profile_data_dir.Append(kCodeCachePath), size);

  std::string uma_name =
      std::string(kUserDataStatsRecorderDataSize) + kCodeCacheUMAName;

  histogram_tester.ExpectTotalCount(uma_name, 1);
  histogram_tester.ExpectBucketCount(uma_name, size / 1024 / 1024, 1);
}

TEST(BrowserDataMigratorUtilTest, CreateHardLink) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  const base::FilePath from_file =
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("from_file"));
  const base::FilePath to_file =
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("to_file"));
  ASSERT_TRUE(base::WriteFile(from_file, "Hello, World"));

  ASSERT_TRUE(CreateHardLink(from_file, to_file));

  EXPECT_TRUE(base::PathExists(to_file));

  // Make sure that they are indeed the same file.
  EXPECT_TRUE(IsSameFile(from_file, to_file));
}

TEST(BrowserDataMigratorUtilTest, CopyDirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  const base::FilePath copy_from =
      scoped_temp_dir.GetPath().Append("copy_from");

  const char subdirectory[] = "Subdirectory";
  const char data_file[] = "data";
  const char original[] = "original";
  const char symlink[] = "symlink";
  const char sensitive[] = "sensitive";

  // Setup files/directories as described below.
  // |- sensitive/original
  // |- copy_from/
  //     |- data
  //     |- Subdirectory/
  //         |- data
  //         |- Subdirectory/data
  //     |- symlink  /* symlink to original */
  ASSERT_TRUE(
      base::CreateDirectory(scoped_temp_dir.GetPath().Append(sensitive)));
  ASSERT_TRUE(base::WriteFile(
      scoped_temp_dir.GetPath().Append(sensitive).Append(original),
      kTextFileContent));
  ASSERT_TRUE(base::CreateDirectory(copy_from));
  ASSERT_TRUE(base::CreateDirectory(copy_from.Append(subdirectory)));
  ASSERT_TRUE(base::CreateDirectory(
      copy_from.Append(subdirectory).Append(subdirectory)));
  ASSERT_TRUE(base::WriteFile(copy_from.Append(data_file), kTextFileContent));
  ASSERT_TRUE(base::WriteFile(copy_from.Append(subdirectory).Append(data_file),
                              kTextFileContent));
  ASSERT_TRUE(base::WriteFile(
      copy_from.Append(subdirectory).Append(subdirectory).Append(data_file),
      kTextFileContent));
  ASSERT_TRUE(base::CreateSymbolicLink(
      scoped_temp_dir.GetPath().Append(sensitive).Append(original),
      copy_from.Append(symlink)));

  // Test `CopyDirectory()`.
  scoped_refptr<CancelFlag> cancelled = base::MakeRefCounted<CancelFlag>();
  FakeMigrationProgressTracker progress_tracker;
  const base::FilePath copy_to = scoped_temp_dir.GetPath().Append("copy_to");
  ASSERT_TRUE(
      CopyDirectory(copy_from, copy_to, cancelled.get(), &progress_tracker));

  // Expected `copy_to` structure after `CopyDirectory()`.
  // |- copy_to/
  //     |- data
  //     |- Subdirectory/
  //         |- data
  //         |- Subdirectory/data
  EXPECT_TRUE(base::PathExists(copy_to));
  EXPECT_TRUE(base::PathExists(copy_to.Append(data_file)));
  EXPECT_TRUE(base::PathExists(copy_to.Append(subdirectory).Append(data_file)));
  EXPECT_TRUE(base::PathExists(
      copy_to.Append(subdirectory).Append(subdirectory).Append(data_file)));
  // Make sure that symlink is not copied.
  EXPECT_FALSE(base::PathExists(copy_to.Append(symlink)));
  EXPECT_FALSE(base::PathExists(copy_to.Append(original)));

  // Test `CopyDirectoryByHardLinks()`.
  const base::FilePath copy_to_hard =
      scoped_temp_dir.GetPath().Append("copy_to_hard");
  ASSERT_TRUE(CopyDirectoryByHardLinks(copy_from, copy_to_hard));

  // Expected `copy_to_hard` structure after `CopyDirectoryByHardLinks()`.
  // |- copy_to_hard/
  //     |- data
  //     |- Subdirectory/
  //         |- data
  //         |- Subdirectory/data
  EXPECT_TRUE(base::PathExists(copy_to_hard));
  EXPECT_TRUE(base::PathExists(copy_to_hard.Append(data_file)));
  EXPECT_TRUE(
      base::PathExists(copy_to_hard.Append(subdirectory).Append(data_file)));
  EXPECT_TRUE(base::PathExists(copy_to_hard.Append(subdirectory)
                                   .Append(subdirectory)
                                   .Append(data_file)));
  // Make sure that symlink is not copied.
  EXPECT_FALSE(base::PathExists(copy_to_hard.Append(symlink)));
  EXPECT_FALSE(base::PathExists(copy_to_hard.Append(original)));

  // Make sure that they are indeed the same file.
  EXPECT_TRUE(
      IsSameFile(copy_from.Append(data_file), copy_to_hard.Append(data_file)));
  EXPECT_TRUE(IsSameFile(copy_from.Append(subdirectory).Append(data_file),
                         copy_to_hard.Append(subdirectory).Append(data_file)));
  EXPECT_TRUE(IsSameFile(
      copy_from.Append(subdirectory).Append(subdirectory).Append(data_file),
      copy_to_hard.Append(subdirectory)
          .Append(subdirectory)
          .Append(data_file)));
}

TEST(BrowserDataMigratorUtilTest, EstimatedExtraBytesCreated) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // Set up the directory as below. 'Preferences' and 'Sync Data' are files that
  // are split during the migration. 'shared_proto_db' is one of the
  // need-to-copy items in `kNeedCopyForMoveDataPaths`.
  // |- Preferences
  // |- Sync Data/
  //     |- data
  // |- shared_proto_db
  const std::string pref_text = "*";
  const std::string sync_text(10, '*');
  const std::string spdb_text(100, '*');
  ASSERT_TRUE(CreateDirectory(temp_dir.GetPath().Append(kSyncDataFilePath)));
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath()
                                  .Append(kSyncDataFilePath)
                                  .Append(FILE_PATH_LITERAL("data")),
                              sync_text));
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().Append(kSharedProtoDBPath),
                              spdb_text));
  ASSERT_TRUE(base::WriteFile(
      temp_dir.GetPath().Append(chrome::kPreferencesFilename), pref_text));

  // Expected size should be size(NeedToCopy) + size(Preferences) * 2 +
  // size(Sync Data).
  const int64_t expected_size = 1 * 2 + 10 + 100;
  EXPECT_EQ(EstimatedExtraBytesCreated(temp_dir.GetPath()), expected_size);
}

TEST(BrowserDataMigratorUtilTest, IsAshOnlySyncDataType) {
  // The types that should be recognized as Ash-only are stored in
  // `browser_data_migrator_util::kAshOnlySyncDataTypes`.
  // Then any of the following can be suffixed to the type name:
  // - `kDataPrefix` = "-dt-"
  // - `kMetadataPrefix` = "-md-"
  // - `kGlobalMetadataKey` = "-GlobalMetadata"
  // `kDataPrefix` and `kMetadataPrefix` are then followed by an id, while
  // `kGlobalMetadataKey` is not.

  const constexpr char* const kTypes[] = {
      "app_list",
      "arc_package",
      "os_preferences",
      "os_priority_preferences",
      "printers",
      "printers_authorization_servers",
      "wifi_configurations",
      "workspace_desk",
  };

  const constexpr char* const kSuffixes[] = {
      "-dt-",
      "-md-",
  };

  for (const char* const type : kTypes) {
    for (const char* const suffix : kSuffixes) {
      auto key = std::string(type) + std::string(suffix) + "random_id";
      EXPECT_TRUE(IsAshOnlySyncDataType(key));
    }
    auto global_metadata_key = std::string(type) + "-GlobalMetadata";
    EXPECT_TRUE(IsAshOnlySyncDataType(global_metadata_key));
    auto global_metadata_key_with_id =
        std::string(type) + "-GlobalMetadata" + "random_id";
    EXPECT_FALSE(IsAshOnlySyncDataType(global_metadata_key_with_id));
  }

  EXPECT_FALSE(IsAshOnlySyncDataType("random_key"));
}

class BrowserDataMigratorUtilWithTargetsTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Setup profile data directory.
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    LOG(WARNING) << "Setting up tmp directory.";

    profile_data_dir_ = scoped_temp_dir_.GetPath();

    // Setup files/directories as described below.
    //
    // |- Bookmarks      /* lacros */
    // |- Cookies        /* lacros */
    // |- Downloads/     /* remain in ash */
    //     |- file
    //     |- file 2
    // |- shared_proto_db  /* need to copy */
    // |- Cache          /* deletable */
    // |- Code Cache/    /* deletable */
    //     |- file

    LOG(WARNING) << profile_data_dir_.Append(kBookmarksPath).value();

    // Lacros items.
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kBookmarksPath),
                                kTextFileContent));
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kCookiesPath),
                                kTextFileContent));
    // Remain in ash items.
    ASSERT_TRUE(
        base::CreateDirectory(profile_data_dir_.Append(kDownloadsPath)));
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kDownloadsPath)
                                    .Append(FILE_PATH_LITERAL("file")),
                                kTextFileContent));
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kDownloadsPath)
                                    .Append(FILE_PATH_LITERAL("file 2")),
                                kTextFileContent));

    // Need to copy items.
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kSharedProtoDBPath),
                                kTextFileContent));

    // Deletable items.
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kCachePath),
                                kTextFileContent));
    ASSERT_TRUE(
        base::CreateDirectory(profile_data_dir_.Append(kCodeCachePath)));
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kCodeCachePath)
                                    .Append(FILE_PATH_LITERAL("file")),
                                kTextFileContent));
  }

  void TearDown() override { EXPECT_TRUE(scoped_temp_dir_.Delete()); }

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath profile_data_dir_;
};

TEST_F(BrowserDataMigratorUtilWithTargetsTest, GetTargetItems) {
  // Check for lacros data.
  std::vector<TargetItem> expected_lacros_items = {
      {profile_data_dir_.Append(kBookmarksPath), kTextFileContent.size(),
       TargetItem::ItemType::kFile},
      {profile_data_dir_.Append(kCookiesPath), kTextFileContent.size(),
       TargetItem::ItemType::kFile}};
  TargetItems lacros_items =
      GetTargetItems(profile_data_dir_, ItemType::kLacros);
  EXPECT_EQ(lacros_items.total_size,
            static_cast<int>(kTextFileContent.size() * 2));
  ASSERT_EQ(lacros_items.items.size(), expected_lacros_items.size());
  std::sort(lacros_items.items.begin(), lacros_items.items.end(),
            TargetItemComparator());
  for (size_t i = 0; i < lacros_items.items.size(); i++) {
    SCOPED_TRACE(lacros_items.items[i].path.value());
    EXPECT_EQ(lacros_items.items[i], expected_lacros_items[i]);
  }

  // Check for remain in ash data.
  std::vector<TargetItem> expected_remain_in_ash_items = {
      {profile_data_dir_.Append(kDownloadsPath), kTextFileContent.size() * 2,
       TargetItem::ItemType::kDirectory}};
  TargetItems remain_in_ash_items =
      GetTargetItems(profile_data_dir_, ItemType::kRemainInAsh);
  EXPECT_EQ(remain_in_ash_items.total_size,
            static_cast<int>(kTextFileContent.size() * 2));
  ASSERT_EQ(remain_in_ash_items.items.size(),
            expected_remain_in_ash_items.size());
  EXPECT_EQ(remain_in_ash_items.items[0], expected_remain_in_ash_items[0]);

  // Check for items that need copies in lacros.
  std::vector<TargetItem> expected_need_copy_items = {
      {profile_data_dir_.Append(kSharedProtoDBPath), kTextFileContent.size(),
       TargetItem::ItemType::kFile}};
  TargetItems need_copy_items =
      GetTargetItems(profile_data_dir_, ItemType::kNeedCopyForMove);
  EXPECT_EQ(need_copy_items.total_size,
            static_cast<int>(kTextFileContent.size()));
  ASSERT_EQ(need_copy_items.items.size(), expected_need_copy_items.size());
  EXPECT_EQ(need_copy_items.items[0], expected_need_copy_items[0]);

  // Check for deletable items.
  std::vector<TargetItem> expected_deletable_items = {
      {profile_data_dir_.Append(kCachePath), kTextFileContent.size(),
       TargetItem::ItemType::kFile},
      {profile_data_dir_.Append(kCodeCachePath), kTextFileContent.size(),
       TargetItem::ItemType::kDirectory}};
  TargetItems deletable_items =
      GetTargetItems(profile_data_dir_, ItemType::kDeletable);
  std::sort(deletable_items.items.begin(), deletable_items.items.end(),
            TargetItemComparator());
  EXPECT_EQ(deletable_items.total_size,
            static_cast<int>(kTextFileContent.size() * 2));
  ASSERT_EQ(deletable_items.items.size(), expected_deletable_items.size());
  for (size_t i = 0; i < deletable_items.items.size(); i++) {
    SCOPED_TRACE(deletable_items.items[i].path.value());
    EXPECT_EQ(deletable_items.items[i], expected_deletable_items[i]);
  }
}

TEST_F(BrowserDataMigratorUtilWithTargetsTest, DryRunToCollectUMA) {
  base::HistogramTester histogram_tester;

  DryRunToCollectUMA(profile_data_dir_);

  const std::string uma_name_bookmarks =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "Bookmarks";
  const std::string uma_name_cookies =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "Cookies";
  const std::string uma_name_downloads =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "Downloads";
  const std::string uma_name_shared_proto_db =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "SharedProtoDb";
  const std::string uma_name_cache =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "Cache";
  const std::string uma_name_code_cache =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "CodeCache";

  histogram_tester.ExpectBucketCount(uma_name_bookmarks,
                                     kTextFileContent.size() / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_cookies,
                                     kTextFileContent.size() / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(
      uma_name_downloads, kTextFileContent.size() * 2 / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_shared_proto_db,
                                     kTextFileContent.size() / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_cache,
                                     kTextFileContent.size() / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_code_cache,
                                     kTextFileContent.size() / 1024 / 1024, 1);

  histogram_tester.ExpectBucketCount(
      kDryRunLacrosDataSize, kTextFileContent.size() * 2 / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(
      kDryRunAshDataSize, kTextFileContent.size() * 2 / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(kDryRunCommonDataSize,
                                     kTextFileContent.size() / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(
      kDryRunNoCopyDataSize, kTextFileContent.size() * 2 / 1024 / 1024, 1);

  histogram_tester.ExpectTotalCount(kDryRunCopyMigrationHasEnoughDiskSpace, 1);
  histogram_tester.ExpectTotalCount(
      kDryRunDeleteAndCopyMigrationHasEnoughDiskSpace, 1);
  histogram_tester.ExpectTotalCount(kDryRunMoveMigrationHasEnoughDiskSpace, 1);
  histogram_tester.ExpectTotalCount(
      kDryRunDeleteAndCopyMigrationHasEnoughDiskSpace, 1);
}

TEST(BrowserDataMigratorUtilTest, UpdatePreferencesKeyByType) {
  const std::string keep_extension_dict_key =
      std::string("extensions.settings.") + kExtensionsAshOnly[0];
  const std::string both_extension_dict_key =
      std::string("extensions.settings.") + kExtensionsBothChromes[0];
  const std::string move_extension_dict_key =
      std::string("extensions.settings.") + kMoveExtensionId;

  base::Value::List extension_list;
  extension_list.Append(kExtensionsAshOnly[0]);
  extension_list.Append(kExtensionsBothChromes[0]);
  extension_list.Append(kMoveExtensionId);
  const std::string extension_list_key = "extensions.pinned_extensions";

  // List of dictionaries instead of list of strings as expected.
  // {"extensions.toolbar": [
  //   { <kExtensionsAshOnly[0]> : "test1"},
  //   { <kExtensionsBothChromes[0]> : "test2"},
  //   { <kMoveExtensionId> : "test3"},
  // ]}
  base::Value::Dict wrong_type_value1;
  wrong_type_value1.Set(kExtensionsAshOnly[0], "test1");
  base::Value::Dict wrong_type_value2;
  wrong_type_value2.Set(kExtensionsBothChromes[0], "test2");
  base::Value::Dict wrong_type_value3;
  wrong_type_value3.Set(kMoveExtensionId, "test3");
  base::Value::List wrong_type_list;
  wrong_type_list.Append(std::move(wrong_type_value1));
  wrong_type_list.Append(std::move(wrong_type_value2));
  wrong_type_list.Append(std::move(wrong_type_value3));
  const std::string wrong_type_key = "extensions.toolbar";

  base::Value::Dict ash_dict;
  ash_dict.SetByDottedPath(keep_extension_dict_key, "test1");
  ash_dict.SetByDottedPath(both_extension_dict_key, "test2");
  ash_dict.SetByDottedPath(move_extension_dict_key, "test3");
  ash_dict.SetByDottedPath(extension_list_key, std::move(extension_list));
  ash_dict.SetByDottedPath(wrong_type_key, std::move(wrong_type_list));
  base::Value::Dict lacros_dict = ash_dict.Clone();

  UpdatePreferencesKeyByType(&ash_dict, "extensions.settings",
                             ChromeType::kAsh);
  UpdatePreferencesKeyByType(&lacros_dict, "extensions.settings",
                             ChromeType::kLacros);

  // Test Ash against expected results.
  base::Value::Dict* d = ash_dict.FindDictByDottedPath("extensions.settings");
  std::set<std::string> expected_keys = {kExtensionsAshOnly[0],
                                         kExtensionsBothChromes[0]};
  EXPECT_EQ(expected_keys, CollectDictKeys(d));
  // If a type other than string is found in a list, it will be left unchanged.
  base::Value::List* l = ash_dict.FindListByDottedPath(wrong_type_key);
  EXPECT_NE(nullptr, l);
  EXPECT_EQ(3u, l->size());

  // Test Lacros against expected results.
  d = lacros_dict.FindDictByDottedPath("extensions.settings");
  expected_keys = {kExtensionsBothChromes[0], kMoveExtensionId};
  EXPECT_EQ(expected_keys, CollectDictKeys(d));
  l = lacros_dict.FindListByDottedPath(wrong_type_key);
  EXPECT_NE(nullptr, l);
  EXPECT_EQ(3u, l->size());
}

TEST(BrowserDataMigratorUtilTest, MigratePreferencesContents) {
  const std::string keep_extension_dict_key =
      std::string("extensions.settings.") + kExtensionsAshOnly[0];
  const std::string both_extension_dict_key =
      std::string("extensions.settings.") + kExtensionsBothChromes[0];
  const std::string move_extension_dict_key =
      std::string("extensions.settings.") + kMoveExtensionId;

  base::Value::List extension_list;
  extension_list.Append(kExtensionsAshOnly[0]);
  extension_list.Append(kExtensionsBothChromes[0]);
  extension_list.Append(kMoveExtensionId);
  const std::string extension_list_key = "extensions.pinned_extensions";

  std::string original_contents;
  base::Value::Dict dict;
  dict.SetByDottedPath(kLacrosOnlyPreferencesKeys[0], "test1");
  dict.SetByDottedPath(kAshOnlyPreferencesKeys[0], "test2");
  dict.SetByDottedPath("unrelated.key", "test3");
  dict.SetByDottedPath(keep_extension_dict_key, "test4");
  dict.SetByDottedPath(move_extension_dict_key, "test5");
  dict.SetByDottedPath(extension_list_key, std::move(extension_list));
  base::JSONWriter::Write(dict, &original_contents);

  auto contents = MigratePreferencesContents(original_contents);
  EXPECT_TRUE(contents.has_value());

  absl::optional<base::Value> ash_root = base::JSONReader::Read(contents->ash);
  EXPECT_TRUE(ash_root.has_value());
  base::Value::Dict* ash_root_dict = ash_root->GetIfDict();
  EXPECT_NE(nullptr, ash_root_dict);
  EXPECT_EQ(nullptr, ash_root_dict->FindStringByDottedPath(
                         kLacrosOnlyPreferencesKeys[0]));
  EXPECT_EQ("test2",
            *ash_root_dict->FindStringByDottedPath(kAshOnlyPreferencesKeys[0]));
  EXPECT_EQ("test3", *ash_root_dict->FindStringByDottedPath("unrelated.key"));
  EXPECT_EQ("test4",
            *ash_root_dict->FindStringByDottedPath(keep_extension_dict_key));
  EXPECT_EQ(nullptr,
            ash_root_dict->FindStringByDottedPath(move_extension_dict_key));
  base::Value::List* ash_extension_list =
      ash_root_dict->FindListByDottedPath(extension_list_key);
  EXPECT_NE(nullptr, ash_extension_list);
  EXPECT_EQ(2u, ash_extension_list->size());
  EXPECT_EQ(kExtensionsAshOnly[0], (*ash_extension_list)[0].GetString());
  EXPECT_EQ(kExtensionsBothChromes[0], (*ash_extension_list)[1].GetString());

  absl::optional<base::Value> lacros_root =
      base::JSONReader::Read(contents->lacros);
  EXPECT_TRUE(lacros_root.has_value());
  base::Value::Dict* lacros_root_dict = lacros_root->GetIfDict();
  EXPECT_NE(nullptr, lacros_root_dict);
  EXPECT_EQ("test1", *lacros_root_dict->FindStringByDottedPath(
                         kLacrosOnlyPreferencesKeys[0]));
  EXPECT_EQ(nullptr, lacros_root_dict->FindStringByDottedPath(
                         kAshOnlyPreferencesKeys[0]));
  EXPECT_EQ("test3",
            *lacros_root_dict->FindStringByDottedPath("unrelated.key"));
  EXPECT_EQ(nullptr,
            lacros_root_dict->FindStringByDottedPath(keep_extension_dict_key));
  EXPECT_EQ("test5",
            *lacros_root_dict->FindStringByDottedPath(move_extension_dict_key));
  base::Value::List* lacros_extension_list =
      lacros_root_dict->FindListByDottedPath(extension_list_key);
  EXPECT_NE(nullptr, lacros_extension_list);
  EXPECT_EQ(2u, lacros_extension_list->size());
  EXPECT_EQ(kExtensionsBothChromes[0], (*lacros_extension_list)[0].GetString());
  EXPECT_EQ(kMoveExtensionId, (*lacros_extension_list)[1].GetString());
}

TEST(BrowserDataMigratorUtilTest, MigratePreferences) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  const base::FilePath original_preferences_path =
      scoped_temp_dir.GetPath().Append("Preferences.original");
  const base::FilePath ash_preferences_path =
      scoped_temp_dir.GetPath().Append("Preferences.ash");
  const base::FilePath lacros_preferences_path =
      scoped_temp_dir.GetPath().Append("Preferences.lacros");

  std::string original_contents;
  base::Value::Dict dict;
  dict.SetByDottedPath(kLacrosOnlyPreferencesKeys[0], "test1");
  dict.SetByDottedPath(kAshOnlyPreferencesKeys[0], "test2");
  dict.SetByDottedPath("unrelated.key", "test3");
  ASSERT_TRUE(base::JSONWriter::Write(dict, &original_contents));
  ASSERT_TRUE(base::WriteFile(original_preferences_path, original_contents));

  EXPECT_TRUE(browser_data_migrator_util::MigratePreferences(
      original_preferences_path, ash_preferences_path,
      lacros_preferences_path));

  std::string ash_contents;
  std::string lacros_contents;
  EXPECT_TRUE(base::ReadFileToString(ash_preferences_path, &ash_contents));
  EXPECT_TRUE(
      base::ReadFileToString(lacros_preferences_path, &lacros_contents));

  absl::optional<base::Value> ash_root = base::JSONReader::Read(ash_contents);
  EXPECT_TRUE(ash_root.has_value());
  base::Value::Dict* ash_root_dict = ash_root->GetIfDict();
  EXPECT_NE(nullptr, ash_root_dict);
  EXPECT_EQ(nullptr, ash_root_dict->FindStringByDottedPath(
                         kLacrosOnlyPreferencesKeys[0]));
  EXPECT_EQ("test2",
            *ash_root_dict->FindStringByDottedPath(kAshOnlyPreferencesKeys[0]));
  EXPECT_EQ("test3", *ash_root_dict->FindStringByDottedPath("unrelated.key"));

  absl::optional<base::Value> lacros_root =
      base::JSONReader::Read(lacros_contents);
  EXPECT_TRUE(lacros_root.has_value());
  base::Value::Dict* lacros_root_dict = lacros_root->GetIfDict();
  EXPECT_NE(nullptr, lacros_root_dict);
  EXPECT_EQ(nullptr, lacros_root_dict->FindStringByDottedPath(
                         kAshOnlyPreferencesKeys[0]));
  EXPECT_EQ("test1", *lacros_root_dict->FindStringByDottedPath(
                         kLacrosOnlyPreferencesKeys[0]));
  EXPECT_EQ("test3",
            *lacros_root_dict->FindStringByDottedPath("unrelated.key"));
}

}  // namespace ash::browser_data_migrator_util
