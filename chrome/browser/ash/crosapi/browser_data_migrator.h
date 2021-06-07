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
// Lacros' user data is backward compatible up until this version.
constexpr char kRequiredDataVersion[] = "92.0.0.0";
// The following are UMA names.
constexpr char kFinalStatus[] = "Ash.BrowserDataMigrator.FinalStatus";
constexpr char kCopiedDataSize[] = "Ash.BrowserDataMigrator.CopiedDataSizeMB";
constexpr char kTotalTime[] = "Ash.BrowserDataMigrator.TotalTimeTakenMS";
constexpr char kCreateDirectoryFail[] =
    "Ash.BrowserDataMigrator.CreateDirectoryFailure";

// BrowserDataMigrator is responsible for one time browser data migration from
// ash-chrome to lacros-chrome. The static method `MaybeMigrate()` instantiates
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
    kSkipped = 0,
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

  // Called on UI thread. If `async` is true, it posts `MigrateInternal()` to a
  // worker thread with callback as reply. If `async` is false, the whole
  // process will be done on UI thread. Since the migration copies user data
  // files, it has to be completed before ash chrome starts accessing those
  // files. Files are copied to `tmp_dir_` first and then moved to `to_dir_` in
  // an atomic way.
  static void MaybeMigrate(const AccountId& account_id,
                           const std::string& user_id_hash,
                           bool async,
                           base::OnceClosure callback);

  // Checks if lacros' data directory needs to be wiped before migration.
  // `data_version` is the version of last data wipe. `current_version` is the
  // version of ash-chrome. `required_version` is the version that introduces
  // some breaking change. `data_version` needs to be greater or equal to
  // `required_version`. If `required_version` is newer than `current_version`,
  // data wipe is not required.
  static bool IsDataWipeRequired(base::Version data_version,
                                 const base::Version& current_version,
                                 const base::Version& required_version);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, IsMigrationRequiredOnUI);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest,
                           IsMigrationRequiredOnWorker);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, IsDataWipeRequiredInvalid);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest,
                           IsDataWipeRequiredFutureVersion);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest,
                           IsDataWipeRequiredSameVersion);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, IsDataWipeRequired);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, IsDataWipeRequired2);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, MaybeWipeUserDir);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, GetTargetInfo);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, RecordStatus);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, Migrate);

  // Handles the migration on a worker thread. Returns whether a migration
  // occurred.
  MigrationResult MigrateInternal(bool is_data_wipe_required);
  // Called when the migration is finished on the UI thread.
  static void MigrateInternalFinishedUIThread(base::OnceClosure callback,
                                              const std::string& user_id_hash,
                                              MigrationResult result);
  // Records to UMA histograms. Note that if `target_info` is nullptr, timer
  // will be ignored.
  static void RecordStatus(const FinalStatus& final_status,
                           const TargetInfo* target_info = nullptr,
                           const base::ElapsedTimer* timer = nullptr);
  // Checks if migration should happen. Called on UI thread.
  static bool IsMigrationRequiredOnUI(const user_manager::User* user);
  // Checks if migration should happen. Called on worker thread.
  bool IsMigrationRequiredOnWorker() const;
  // Gets what files/dirs need to be copied and the total byte size of files to
  // be copied.
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
