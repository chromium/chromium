// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/version.h"
#include "chromeos/login/auth/user_context.h"

namespace ash {

// The new profile data directory location is
// '/home/chronos/u-<hash>/lacros/Default'. User data directory for lacros.
constexpr char kLacrosDir[] = "lacros";
// Profile data directory for lacros.
constexpr char kLacrosProfilePath[] = "Default";
// The following are UMA names.
constexpr char kFinalStatus[] = "Ash.BrowserDataMigrator.FinalStatus";
constexpr char kCopiedDataSize[] = "Ash.BrowserDataMigrator.CopiedDataSizeMB";
constexpr char kTotalTime[] = "Ash.BrowserDataMigrator.TotalTimeTakenMS";
constexpr char kCreateDirectoryFail[] =
    "Ash.BrowserDataMigrator.CreateDirectoryFailure";

// BrowserDataMigrator is responsible for one time browser data migration from
// ash-chrome to lacros-chrome. The static method `Migrate()` instantiates
// an instance and calls `MigrateInternal()`.
class BrowserDataMigrator {
 public:
  // Used to describe a file/dir that has to be migrated.
  struct TargetItem {
    enum class ItemType { kFile, kDirectory };
    TargetItem(base::FilePath path, ItemType item_type);
    ~TargetItem() = default;
    bool operator==(const TargetItem& rhs) const;

    base::FilePath path;
    bool is_directory;
  };

  // Used to describe what files/dirs have to be migrated to the new location
  // and the total byte size of those files.
  struct TargetInfo {
    TargetInfo();
    ~TargetInfo();
    TargetInfo(const TargetInfo&);

    // Items that have to be copied that are directly under user data directory.
    std::vector<TargetItem> user_data_items;
    // Items that have to be copied that are directly under profile data
    // directory. Profile data directory itself is inside user data directory.
    std::vector<TargetItem> profile_data_items;
    int64_t total_byte_count;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // This enum corresponds to BrowserDataMigratorFinalStatus in hisograms.xml
  // and enums.xml.
  enum class FinalStatus {
    kSkipped = 0,  // No longer in use.
    kSuccess = 1,
    kGetPathFailed = 2,
    kDeleteTmpDirFailed = 3,
    kNotEnoughSpace = 4,
    kCopyFailed = 5,
    kMoveFailed = 6,
    kDataWipeFailed = 7,
    kMaxValue = kDataWipeFailed
  };

  enum class ResultValue {
    kSkipped,
    kSucceeded,
    kFailed,
  };

  // Return value of `MigrateInternal()`.
  struct MigrationResult {
    // Describes the end result of user data wipe.
    ResultValue data_wipe;
    // Describes the end result of data migration.
    ResultValue data_migration;
  };

  // The class is instantiated on UI thread, bound to `MigrateInternal()` and
  // then posted to worker thread.
  explicit BrowserDataMigrator(const base::FilePath& from);
  BrowserDataMigrator(const BrowserDataMigrator&) = delete;
  BrowserDataMigrator& operator=(const BrowserDataMigrator&) = delete;
  ~BrowserDataMigrator();

  // Checks if migration is required for the user identified by `user_context`
  // and if it is required, calls a DBus method to session_manager and
  // terminates ash-chrome.
  static void MaybeRestartToMigrate(const UserContext& user_context);

  // The method needs to be called on UI thread. It instantiates
  // BrowserDataMigrator and posts `MigrateInternal()` on a worker thread. It
  // calls `callback` on the original thread when migration has completed or
  // failed.
  static void Migrate(const std::string& user_id_hash,
                      base::OnceClosure callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest,
                           IsMigrationRequiredOnWorker);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, GetTargetInfo);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, RecordStatus);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, Migrate);

  // The method includes a blocking operation. It checks if lacros user data dir
  // already exists or not. Check if lacros is enabled or not beforehand.
  static bool IsMigrationRequiredOnWorker(base::FilePath user_data_dir,
                                          const std::string& user_id_hash);
  // Handles the migration on a worker thread. Returns the end status of data
  // wipe and migration.
  MigrationResult MigrateInternal();
  // Called on UI thread once migration is finished.
  static void MigrateInternalFinishedUIThread(base::OnceClosure callback,
                                              const std::string& user_id_hash,
                                              MigrationResult result);
  // Records to UMA histograms. Note that if `target_info` is nullptr, timer
  // will be ignored.
  static void RecordStatus(const FinalStatus& final_status,
                           const TargetInfo* target_info = nullptr,
                           const base::ElapsedTimer* timer = nullptr);
  TargetInfo GetTargetInfo() const;
  // Compares space available under `from_dir_` against total byte size that
  // needs to be copied.
  bool HasEnoughDiskSpace(const TargetInfo& target_info) const;
  // Copies files from `from_dir_` to `tmp_dir_`.
  bool CopyToTmpDir(const TargetInfo& target_info) const;
  // Moves `tmp_dir_` to `to_dir_`.
  bool MoveTmpToTargetDir() const;

  // Path to the original profile data directory. It is directly under the
  // user data directory.
  base::FilePath from_dir_;
  // Path to the new profile data directory.
  base::FilePath to_dir_;
  // Path to temporary directory.
  base::FilePath tmp_dir_;
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
