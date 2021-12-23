// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_

#include <atomic>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/atomic_flag.h"
#include "base/timer/elapsed_timer.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/migration_progress_tracker.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"

class PrefService;

namespace ash {

// User data directory name for lacros.
constexpr char kLacrosDir[] = "lacros";

// Profile data directory name for lacros.
constexpr char kLacrosProfilePath[] = "Default";

// The name of temporary directory that will store copies of files from the
// original user data directory. At the end of the migration, it will be moved
// to the appropriate destination.
constexpr char kTmpDir[] = "browser_data_migrator";

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
// `BrowserDataMigratorImpl::DryRunToCollectUMA()`.
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

// Local state pref name, which is used to keep track of what step migration is
// at. This ensures that ash does not get repeatedly for migration.
// 1. The user logs in and restarts ash if necessary to apply flags.
// 2. Migration check runs.
// 3. Restart ash to run migration.
// 4. Restart ash again to show the home screen.
constexpr char kMigrationStep[] = "ash.browser_data_migrator.migration_step";

// Local state pref name to keep track of the number of migration attempts a
// user has gone through before. It is a dictionary of the form
// `{<user_id_hash>: <count>}`.
constexpr char kMigrationAttemptCountPref[] =
    "ash.browser_data_migrator.migration_attempt_count";

// Maximum number of migration attempts. Migration will be skipped for the user
// after
constexpr int kMaxMigrationAttemptCount = 3;

// CancelFlag
class CancelFlag : public base::RefCountedThreadSafe<CancelFlag> {
 public:
  CancelFlag();
  CancelFlag(const CancelFlag&) = delete;
  CancelFlag& operator=(const CancelFlag&) = delete;

  void Set() { cancelled_ = true; }
  bool IsSet() const { return cancelled_; }

 private:
  friend base::RefCountedThreadSafe<CancelFlag>;

  ~CancelFlag();
  std::atomic_bool cancelled_;
};

// The interface is exposed to be inherited by fakes in tests.
class BrowserDataMigrator {
 public:
  virtual ~BrowserDataMigrator() = default;
  // Carries out the migration. It needs to be called on UI thread.
  virtual void Migrate() = 0;
  // Cancels the migration.
  virtual void Cancel() = 0;
};

// BrowserDataMigratorImpl is responsible for one time browser data migration
// from ash-chrome to lacros-chrome.
class BrowserDataMigratorImpl : public BrowserDataMigrator {
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
    kSizeLimitExceeded = 8,  // No longer in use.
    kCancelled = 9,
    kMaxValue = kCancelled
  };

  // The value for `kMigrationStep`.
  enum class MigrationStep {
    kCheckStep = 0,      // Migration check should run.
    kRestartCalled = 1,  // `MaybeRestartToMigrate()` called restart.
    kStarted = 2,        // `Migrate()` was called.
    kEnded = 3  // Migration ended. It was either skipped, failed or succeeded.
  };

  enum class ResultValue { kSkipped, kSucceeded, kFailed, kCancelled };

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

  // `BrowserDataMigratorImpl` migrates browser data from `original_profile_dir`
  // to a new profile location for lacros chrome. `progress_callback` is called
  // to update the progress bar on the screen. `completion_callback` passed as
  // an argument will be called on the UI thread where `Migrate()` is called
  // once migration has completed or failed.
  BrowserDataMigratorImpl(const base::FilePath& original_profile_dir,
                          const std::string& user_id_hash,
                          const ProgressCallback& progress_callback,
                          base::OnceClosure completion_callback,
                          PrefService* local_state);
  BrowserDataMigratorImpl(const BrowserDataMigratorImpl&) = delete;
  BrowserDataMigratorImpl& operator=(const BrowserDataMigratorImpl&) = delete;
  ~BrowserDataMigratorImpl() override;

  // Checks if migration is required for the user identified by `user_id_hash`
  // and if it is required, calls a D-Bus method to session_manager and
  // terminates ash-chrome. It returns true if the D-Bus call to the
  // session_manager is made and successful. The return value of true means that
  // `chrome::AttemptRestart()` has been called.
  static bool MaybeRestartToMigrate(const AccountId& account_id,
                                    const std::string& user_id_hash);

  // `BrowserDataMigrator` methods.
  void Migrate() override;
  void Cancel() override;

  // Registers boolean pref `kCheckForMigrationOnRestart` with default as false.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Clears the value of `kMigrationStep` in Local State.
  static void ClearMigrationStep(PrefService* local_state);

  ResultValue GetFinalStatus();

  // Collects migration specific UMAs without actually running the migration. It
  // does not check if lacros is enabled.
  static void DryRunToCollectUMA(const base::FilePath& profile_data_dir);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest,
                           ManipulateMigrationAttemptCount);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, GetTargetInfo);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, CopyDirectory);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, SetupTmpDir);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, CancelSetupTmpDir);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, RecordStatus);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, MigrateInternal);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, Migrate);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorTest, MigrateCancelled);

  // Sets the value of `kMigrationStep` in Local State.
  static void SetMigrationStep(PrefService* local_state, MigrationStep step);

  // Gets the value of `kMigrationStep` in Local State.
  static MigrationStep GetMigrationStep(PrefService* local_state);

  // Increments the migration attempt count stored in
  // `kMigrationAttemptCountPref` by 1 for the user identified by
  // `user_id_hash`.
  static void UpdateMigrationAttemptCountForUser(
      PrefService* local_state,
      const std::string& user_id_hash);

  // Gets the number of migration attempts for the user stored in
  // `kMigrationAttemptCountPref.
  static int GetMigrationAttemptCountForUser(PrefService* local_state,
                                             const std::string& user_id_hash);

  // Resets the number of migration attempts for the user stored in
  // `kMigrationAttemptCountPref.
  static void ClearMigrationAttemptCountForUser(
      PrefService* local_state,
      const std::string& user_id_hash);

  // Handles the migration on a worker thread. Returns the end status of data
  // wipe and migration. `progress_callback` gets posted on UI thread whenever
  // an update to the UI is required
  static MigrationResult MigrateInternal(
      const base::FilePath& original_user_dir,
      std::unique_ptr<MigrationProgressTracker> progress_tracker,
      scoped_refptr<CancelFlag> cancel_flag);

  // Called from `MaybeRestartToMigrate()` to proceed with restarting to start
  // the migration. It returns true if D-Bus call was successful.
  static bool RestartToMigrate(const AccountId& account_id,
                               const std::string& user_id_hash);

  // Called on UI thread once migration is finished.
  void MigrateInternalFinishedUIThread(MigrationResult result);

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

  // Set up the temporary directory `tmp_dir` by copying items into it.
  static bool SetupTmpDir(const TargetInfo& target_info,
                          const base::FilePath& from_dir,
                          const base::FilePath& tmp_dir,
                          CancelFlag* cancel_flag,
                          MigrationProgressTracker* progress_tracker);

  // Copies `items` to `to_dir`. `items_size` and `category_name` are used for
  // logging.
  static bool CopyTargetItems(const base::FilePath& to_dir,
                              const std::vector<TargetItem>& items,
                              CancelFlag* cancel_flag,
                              int64_t items_size,
                              base::StringPiece category_name,
                              MigrationProgressTracker* progress_tracker);

  // Copies `item` to location pointed by `dest`. Returns true on success and
  // false on failure.
  static bool CopyTargetItem(const BrowserDataMigratorImpl::TargetItem& item,
                             const base::FilePath& dest,
                             CancelFlag* cancel_flag,
                             MigrationProgressTracker* progress_tracker);

  // Copies the contents of `from_path` to `to_path` recursively. Unlike
  // `base::CopyDirectory()` it skips symlinks.
  static bool CopyDirectory(const base::FilePath& from_path,
                            const base::FilePath& to_path,
                            CancelFlag* cancel_flag,
                            MigrationProgressTracker* progress_tracker);

  // Records the sizes of `TargetItem`s.
  static void RecordTargetItemSizes(const std::vector<TargetItem>& items);

  // Path to the original profile data directory, which is directly under the
  // user data directory.
  const base::FilePath original_profile_dir_;
  // A hash string of the profile user ID.
  const std::string user_id_hash_;
  // `progress_tracker_` is used to report progress status to the screen.
  std::unique_ptr<MigrationProgressTracker> progress_tracker_;
  // Callback to be called once migration is done. It is called regardless of
  // whether migration succeeded or not.
  base::OnceClosure completion_callback_;
  // `cancel_flag_` gets set by `Cancel()` and tasks posted to worker threads
  // can check if migration is cancelled or not.
  scoped_refptr<CancelFlag> cancel_flag_;
  // Local state prefs, not owned.
  PrefService* local_state_ = nullptr;
  // Final status of the migration.
  ResultValue final_status_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrowserDataMigratorImpl> weak_factory_{this};
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
