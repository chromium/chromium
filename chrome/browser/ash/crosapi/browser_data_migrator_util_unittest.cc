// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"

#include <sys/stat.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/crosapi/fake_migration_progress_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace ash::browser_data_migrator_util {

namespace {

constexpr char kDownloadsPath[] = "Downloads";
constexpr char kLoginDataPath[] = "Login Data";
constexpr char kBookmarksPath[] = "Bookmarks";
constexpr char kCookiesPath[] = "Cookies";
constexpr char kCachePath[] = "Cache";
constexpr char kCodeCachePath[] = "Code Cache";
constexpr char kCodeCacheUMAName[] = "CodeCache";
constexpr char kTextFileContent[] = "Hello, World!";
constexpr int kTextFileSize = sizeof(kTextFileContent);
constexpr char kMoveExtensionId[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

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
      browser_data_migrator_util::kExtensionKeepList[0];
  batch.Put("META:chrome-extension://" + keep_extension_id, "meta");
  batch.Put("_chrome-extension://" + keep_extension_id + "\x00key1"s, "value1");

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
      browser_data_migrator_util::kExtensionKeepList[0];
  batch.Put(keep_extension_id + ".key1", "value1");
  batch.Put(keep_extension_id + ".key2", "value2");
  batch.Put(std::string(kMoveExtensionId) + ".key1", "value1");
  batch.Put(std::string(kMoveExtensionId) + ".key2", "value2");

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = db->Write(write_options, &batch);
  ASSERT_TRUE(status.ok());
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
      base::make_span(kNeedCopyDataPaths);

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

  ASSERT_TRUE(
      base::WriteFile(dir_1.GetPath().Append(FILE_PATH_LITERAL("file1")),
                      kTextFileContent, kTextFileSize));
  ASSERT_TRUE(
      base::CreateDirectory(dir_1.GetPath().Append(FILE_PATH_LITERAL("dir"))));
  ASSERT_TRUE(base::WriteFile(dir_1.GetPath()
                                  .Append(FILE_PATH_LITERAL("dir"))
                                  .Append(FILE_PATH_LITERAL("file2")),
                              kTextFileContent, kTextFileSize));

  // Check that `ComputeDirectorySizeWithoutLinks` returns the sum of sizes of
  // the two files in the directory.
  EXPECT_EQ(ComputeDirectorySizeWithoutLinks(dir_1.GetPath()),
            kTextFileSize * 2);

  base::ScopedTempDir dir_2;
  ASSERT_TRUE(dir_2.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::WriteFile(dir_2.GetPath().Append(FILE_PATH_LITERAL("file3")),
                      kTextFileContent, kTextFileSize));

  ASSERT_TRUE(CreateSymbolicLink(
      dir_2.GetPath().Append(FILE_PATH_LITERAL("file3")),
      dir_1.GetPath().Append(FILE_PATH_LITERAL("link_to_file3"))));

  // Check that `ComputeDirectorySizeWithoutLinks` does not follow symlinks from
  // `dir_1` to `dir_2`.
  EXPECT_EQ(ComputeDirectorySizeWithoutLinks(dir_1.GetPath()),
            kTextFileSize * 2);
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
      browser_data_migrator_util::kExtensionKeepList[0];
  ExtensionKeys expected_keys = {
      {keep_extension_id,
       {
           "META:chrome-extension://" + keep_extension_id,
           "_chrome-extension://" + keep_extension_id + "\x00key1"s,
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
      browser_data_migrator_util::kExtensionKeepList[0];
  ExtensionKeys expected_keys = {
      {keep_extension_id,
       {
           "META:chrome-extension://" + keep_extension_id,
           "_chrome-extension://" + keep_extension_id + "\x00key1"s,
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
  };
  EXPECT_EQ(expected_keys, keys);
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
  base::WriteFile(from_file, "Hello, World", sizeof("Hello, World"));

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

  ASSERT_TRUE(base::WriteFile(
      scoped_temp_dir.GetPath().Append(sensitive).Append(original),
      kTextFileContent, kTextFileSize));
  ASSERT_TRUE(base::CreateDirectory(copy_from));
  ASSERT_TRUE(base::CreateDirectory(copy_from.Append(subdirectory)));
  ASSERT_TRUE(base::CreateDirectory(
      copy_from.Append(subdirectory).Append(subdirectory)));
  ASSERT_TRUE(base::WriteFile(copy_from.Append(data_file), kTextFileContent,
                              kTextFileSize));
  ASSERT_TRUE(base::WriteFile(copy_from.Append(subdirectory).Append(data_file),
                              kTextFileContent, kTextFileSize));
  ASSERT_TRUE(base::WriteFile(
      copy_from.Append(subdirectory).Append(subdirectory).Append(data_file),
      kTextFileContent, kTextFileSize));
  base::CreateSymbolicLink(
      scoped_temp_dir.GetPath().Append(sensitive).Append(original),
      copy_from.Append(symlink));

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

TEST(BrowserDataMigratorUtilTest, HasEnoughDiskSpace) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const int64_t free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(temp_dir.GetPath());
  ASSERT_GE(free_disk_space, 0);
  ASSERT_GE(static_cast<uint64_t>(free_disk_space), kBuffer);

  // If total copy size is the same as `free_disk_space` then the disk is
  // exactly `kBuffer` bytes short of free space.
  EXPECT_EQ(ExtraBytesRequiredToBeFreed(free_disk_space, temp_dir.GetPath()),
            kBuffer);
  EXPECT_FALSE(HasEnoughDiskSpace(free_disk_space, temp_dir.GetPath()));

  // If total copy size is the same as `free_disk_space - kBuffer` then the disk
  // has just enough space for the migration.
  EXPECT_EQ(ExtraBytesRequiredToBeFreed(free_disk_space - kBuffer,
                                        temp_dir.GetPath()),
            0u);
  EXPECT_TRUE(
      HasEnoughDiskSpace(free_disk_space - kBuffer, temp_dir.GetPath()));

  // If there is nothing to be copied then as long as `free_disk_space >=
  // kBuffer`, there should be no extra space required to be freed.
  EXPECT_EQ(ExtraBytesRequiredToBeFreed(0, temp_dir.GetPath()), 0u);
  EXPECT_TRUE(HasEnoughDiskSpace(0, temp_dir.GetPath()));
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
    // |- Login Data     /* need to copy */
    // |- Cache          /* deletable */
    // |- Code Cache/    /* deletable */
    //     |- file

    LOG(WARNING) << profile_data_dir_.Append(kBookmarksPath).value();

    // Lacros items.
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kBookmarksPath),
                                kTextFileContent, kTextFileSize));
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kCookiesPath),
                                kTextFileContent, kTextFileSize));
    // Remain in ash items.
    ASSERT_TRUE(
        base::CreateDirectory(profile_data_dir_.Append(kDownloadsPath)));
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kDownloadsPath)
                                    .Append(FILE_PATH_LITERAL("file")),
                                kTextFileContent, kTextFileSize));
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kDownloadsPath)
                                    .Append(FILE_PATH_LITERAL("file 2")),
                                kTextFileContent, kTextFileSize));

    // Need to copy items.
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kLoginDataPath),
                                kTextFileContent, kTextFileSize));

    // Deletable items.
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kCachePath),
                                kTextFileContent, kTextFileSize));
    ASSERT_TRUE(
        base::CreateDirectory(profile_data_dir_.Append(kCodeCachePath)));
    ASSERT_TRUE(base::WriteFile(profile_data_dir_.Append(kCodeCachePath)
                                    .Append(FILE_PATH_LITERAL("file")),
                                kTextFileContent, kTextFileSize));
  }

  void TearDown() override { EXPECT_TRUE(scoped_temp_dir_.Delete()); }

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath profile_data_dir_;
};

TEST_F(BrowserDataMigratorUtilWithTargetsTest, GetTargetItems) {
  // Check for lacros data.
  std::vector<TargetItem> expected_lacros_items = {
      {profile_data_dir_.Append(kBookmarksPath), kTextFileSize,
       TargetItem::ItemType::kFile},
      {profile_data_dir_.Append(kCookiesPath), kTextFileSize,
       TargetItem::ItemType::kFile}};
  TargetItems lacros_items =
      GetTargetItems(profile_data_dir_, ItemType::kLacros);
  EXPECT_EQ(lacros_items.total_size, kTextFileSize * 2);
  ASSERT_EQ(lacros_items.items.size(), expected_lacros_items.size());
  std::sort(lacros_items.items.begin(), lacros_items.items.end(),
            TargetItemComparator());
  for (size_t i = 0; i < lacros_items.items.size(); i++) {
    SCOPED_TRACE(lacros_items.items[i].path.value());
    EXPECT_EQ(lacros_items.items[i], expected_lacros_items[i]);
  }

  // Check for remain in ash data.
  std::vector<TargetItem> expected_remain_in_ash_items = {
      {profile_data_dir_.Append(kDownloadsPath), kTextFileSize * 2,
       TargetItem::ItemType::kDirectory}};
  TargetItems remain_in_ash_items =
      GetTargetItems(profile_data_dir_, ItemType::kRemainInAsh);
  EXPECT_EQ(remain_in_ash_items.total_size, kTextFileSize * 2);
  ASSERT_EQ(remain_in_ash_items.items.size(),
            expected_remain_in_ash_items.size());
  EXPECT_EQ(remain_in_ash_items.items[0], expected_remain_in_ash_items[0]);

  // Check for items that need copies in lacros.
  std::vector<TargetItem> expected_need_copy_items = {
      {profile_data_dir_.Append(kLoginDataPath), kTextFileSize,
       TargetItem::ItemType::kFile}};
  TargetItems need_copy_items =
      GetTargetItems(profile_data_dir_, ItemType::kNeedCopy);
  EXPECT_EQ(need_copy_items.total_size, kTextFileSize);
  ASSERT_EQ(need_copy_items.items.size(), expected_need_copy_items.size());
  EXPECT_EQ(need_copy_items.items[0], expected_need_copy_items[0]);

  // Check for deletable items.
  std::vector<TargetItem> expected_deletable_items = {
      {profile_data_dir_.Append(kCachePath), kTextFileSize,
       TargetItem::ItemType::kFile},
      {profile_data_dir_.Append(kCodeCachePath), kTextFileSize,
       TargetItem::ItemType::kDirectory}};
  TargetItems deletable_items =
      GetTargetItems(profile_data_dir_, ItemType::kDeletable);
  std::sort(deletable_items.items.begin(), deletable_items.items.end(),
            TargetItemComparator());
  EXPECT_EQ(deletable_items.total_size, kTextFileSize * 2);
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
  const std::string uma_name_login_data =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "LoginData";
  const std::string uma_name_cache =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "Cache";
  const std::string uma_name_code_cache =
      std::string(browser_data_migrator_util::kUserDataStatsRecorderDataSize) +
      "CodeCache";

  histogram_tester.ExpectBucketCount(uma_name_bookmarks,
                                     kTextFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_cookies,
                                     kTextFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_downloads,
                                     kTextFileSize * 2 / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_login_data,
                                     kTextFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_cache,
                                     kTextFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(uma_name_code_cache,
                                     kTextFileSize / 1024 / 1024, 1);

  histogram_tester.ExpectBucketCount(kDryRunLacrosDataSize,
                                     kTextFileSize * 2 / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(kDryRunAshDataSize,
                                     kTextFileSize * 2 / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(kDryRunCommonDataSize,
                                     kTextFileSize / 1024 / 1024, 1);
  histogram_tester.ExpectBucketCount(kDryRunNoCopyDataSize,
                                     kTextFileSize * 2 / 1024 / 1024, 1);

  histogram_tester.ExpectTotalCount(kDryRunCopyMigrationHasEnoughDiskSpace, 1);
  histogram_tester.ExpectTotalCount(
      kDryRunDeleteAndCopyMigrationHasEnoughDiskSpace, 1);
  histogram_tester.ExpectTotalCount(kDryRunMoveMigrationHasEnoughDiskSpace, 1);
  histogram_tester.ExpectTotalCount(
      kDryRunDeleteAndCopyMigrationHasEnoughDiskSpace, 1);
}

}  // namespace ash::browser_data_migrator_util
