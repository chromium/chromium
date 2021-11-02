// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "base/timer/elapsed_timer.h"
#include "base/version.h"
#include "chromeos/login/auth/user_context.h"

namespace ash {

// User data directory name for lacros.
constexpr char kLacrosDir[] = "lacros";

// Profile data directory name for lacros.
constexpr char kLacrosProfilePath[] = "Default";

// The following are UMA names.
constexpr char kFinalStatus[] = "Ash.BrowserDataMigrator.FinalStatus";
constexpr char kCopiedDataSize[] = "Ash.BrowserDataMigrator.CopiedDataSizeMB";
constexpr char kNoCopyDataSize[] = "Ash.BrowserDataMigrator.NoCopyDataSizeMB";
constexpr char kAshDataSize[] = "Ash.BrowserDataMigrator.AshDataSizeMB";
constexpr char kLacrosDataSize[] = "Ash.BrowserDataMigrator.LacrosDataSizeMB";
constexpr char kCommonDataSize[] = "Ash.BrowserDataMigrator.CommonDataSizeMB";
constexpr char kTotalTime[] = "Ash.BrowserDataMigrator.TotalTimeTakenMS";
constexpr char kLacrosDataTime[] =
    "Ash.BrowserDataMigrator.LacrosDataTimeTakenMS";
constexpr char kCommonDataTime[] =
    "Ash.BrowserDataMigrator.CommonDataTimeTakenMS";
constexpr char kCreateDirectoryFail[] =
    "Ash.BrowserDataMigrator.CreateDirectoryFailure";

// The following UMAs are recorded from
// `BrowserDataMigrator::DryRunToCollectUMA()`.
constexpr char kDryRunNoCopyDataSize[] =
    "Ash.BrowserDataMigrator.DryRunNoCopyDataSizeMB";
constexpr char kDryRunAshDataSize[] =
    "Ash.BrowserDataMigrator.DryRunAshDataSizeMB";
constexpr char kDryRunLacrosDataSize[] =
    "Ash.BrowserDataMigrator.DryRunLacrosDataSizeMB";
constexpr char kDryRunCommonDataSize[] =
    "Ash.BrowserDataMigrator.DryRunCommonDataSizeMB";

constexpr char kDryRunCopyMigrationHasEnoughDiskSpace[] =
    "Ash.BrowserDataMigrator.DryRunHasEnoughDiskSpace.Copy";
constexpr char kDryRunMoveMigrationHasEnoughDiskSpace[] =
    "Ash.BrowserDataMigrator.DryRunHasEnoughDiskSpace.Move";
constexpr char kDryRunDeleteAndCopyMigrationHasEnoughDiskSpace[] =
    "Ash.BrowserDataMigrator.DryRunHasEnoughDiskSpace.DeleteAndCopy";
constexpr char kDryRunDeleteAndMoveMigrationHasEnoughDiskSpace[] =
    "Ash.BrowserDataMigrator.DryRunHasEnoughDiskSpace.DeleteAndMove";

// BrowserDataMigrator is responsible for one time browser data migration from
// ash-chrome to lacros-chrome.
class BrowserDataMigrator {
 public:
  // Used to describe a file/dir that has to be migrated.
  struct TargetItem {
    enum class ItemType { kFile, kDirectory };
    TargetItem(base::FilePath path, int64_t size, ItemType item_type);
    ~TargetItem() = default;
    bool operator==(const TargetItem& rhs) const;

    base::FilePath path;
    // The size of the TargetItem. If TargetItem is a directory, it is the sum
    // of all files under the directory.
    int64_t size;
    bool is_directory;
  };

  // Used to describe what files/dirs have to be migrated to the new location
  // and the total byte size of those files.
  struct TargetInfo {
    TargetInfo();
    ~TargetInfo();
    TargetInfo(TargetInfo&&);

    // Items that should stay in ash data dir.
    std::vector<TargetItem> ash_data_items;
    // Items that should be moved to lacros data dir.
    std::vector<TargetItem> lacros_data_items;
    // Items that will be duplicated in both ash and lacros data dir.
    std::vector<TargetItem> common_data_items;
    // Items that can be deleted from both ash and lacros data dir.
    std::vector<TargetItem> no_copy_data_items;
    // The total size of `ash_data_items`.
    int64_t ash_data_size;
    // The total size of items that can be deleted during the migration.
    int64_t no_copy_data_size;
    // The total size of `lacros_data_items`.
    int64_t lacros_data_size;
    // The total size of `common_data_items`.
    int64_t common_data_size;
    // The size of data that are duplicated. Equivalent to the extra space
    // needed for the migration. Currently this is equal to `lacros_data_size +
    // common_data_size` since we are copying lacros data rather than moving
    // them.
    int64_t TotalCopySize() const;
    // The total size of the profile data directory. It is the sum of ash,
    // no_copy, lacros and common sizes.
    int64_t TotalDirSize() const;
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
    kSizeLimitExceeded = 8,
    kMaxValue = kSizeLimitExceeded
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

  // Specifies the mode of migration.
  enum class Mode {
    kCopy = 0,  // Copies browser related files to lacros.
    kMove = 1,  // Moves browser related files to lacros while copying files
                // that are needed by both ash and lacros.
    kDeleteAndCopy = 2,  // Similar to kCopy but deletes
                         // TargetInfo::no_copy_items to make extra space.
    kDeleteAndMove = 3   // Similar to kMove but deletes
                         // TargetInfo::no_copy_items to make extra space.
  };

  // Checks if migration is required for the user identified by `user_id_hash`
  // and if it is required, calls a DBus method to session_manager and
  // terminates ash-chrome.
  static void MaybeRestartToMigrate(const UserContext& user_context);

  // The method needs to be called on UI thread. It posts `MigrateInternal()` on
  // a worker thread with `callback` which will be called on the original thread
  // once migration has completed or failed.
  static void Migrate(const std::string& user_id_hash,
                      base::OnceClosure callback);

  // Collects migration specific UMAs without actually running the migration. It
  // does not check if lacros is enabled.
  static void DryRunToCollectUMA(const base::FilePath& profile_data_dir);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest,
                           IsMigrationRequiredOnWorker);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, GetTargetInfo);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, CopyDirectory);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, RecordStatus);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, Migrate);

  // The method includes a blocking operation. It checks if lacros user data dir
  // already exists or not. Check if lacros is enabled or not beforehand.
  static bool IsMigrationRequiredOnWorker(base::FilePath user_data_dir,
                                          const std::string& user_id_hash);

  // Handles the migration on a worker thread. Returns the end status of data
  // wipe and migration.
  static MigrationResult MigrateInternal(
      const base::FilePath& original_user_dir);
  // Called on UI thread once migration is finished.
  static void MigrateInternalFinishedUIThread(base::OnceClosure callback,
                                              const std::string& user_id_hash,
                                              MigrationResult result);

  // Records to UMA histograms. Note that if `target_info` is nullptr, timer
  // will be ignored.
  static void RecordStatus(const FinalStatus& final_status,
                           const TargetInfo* target_info = nullptr,
                           const base::ElapsedTimer* timer = nullptr);

  // Create `TargetInfo` from `original_user_dir`. `TargetInfo` will include
  // paths of files/dirs that needs to be migrated.
  static TargetInfo GetTargetInfo(const base::FilePath& original_user_dir);

  // Compares space available for `to_dir` against total byte size that
  // needs to be copied.
  static bool HasEnoughDiskSpace(const TargetInfo& target_info,
                                 const base::FilePath& original_user_dir,
                                 Mode mode);

  // TODO(crbug.com/1248318):Remove this arbitrary cap for migration once a long
  // term solution is found. Temporarily limit the migration size to 4GB until
  // the slow migration speed issue is resolved.
  static bool IsMigrationSmallEnough(const TargetInfo& target_info);

  // Set up the temporary directory `tmp_dir` by copying items into it.
  static bool SetupTmpDir(const TargetInfo& target_info,
                          const base::FilePath& from_dir,
                          const base::FilePath& tmp_dir);

  // Copies `items` to `to_dir`. `items_size` and `category_name` are used for
  // logging.
  static bool CopyTargetItems(const base::FilePath& to_dir,
                              const std::vector<TargetItem>& items,
                              int64_t items_size,
                              base::StringPiece category_name);

  // Copies `item` to location pointed by `dest`. Returns true on success and
  // false on failure.
  static bool CopyTargetItem(const BrowserDataMigrator::TargetItem& item,
                             const base::FilePath& dest);

  // Copies the contents of `from_path` to `to_path` recursively. Unlike
  // `base::CopyDirectory()` it skips symlinks.
  static bool CopyDirectory(const base::FilePath& from_path,
                            const base::FilePath& to_path);

  // Records the sizes of `TargetItem`s.
  static void RecordTargetItemSizes(const std::vector<TargetItem>& items);
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
