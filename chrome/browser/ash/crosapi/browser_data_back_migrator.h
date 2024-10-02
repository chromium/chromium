// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_H_

#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/account_id/account_id.h"

class PrefService;

namespace ash {

namespace standalone_browser::migrator_util {
enum class PolicyInitState;
}  // namespace standalone_browser::migrator_util

namespace browser_data_back_migrator {
// Temporary directory for back migration.
constexpr char kTmpDir[] = "back_migrator_tmp";
}  // namespace browser_data_back_migrator

// Injects the restart function called from
// `BrowserDataBackMigrator::AttemptRestart()` in RAII manner.
class ScopedBackMigratorRestartAttemptForTesting {
 public:
  explicit ScopedBackMigratorRestartAttemptForTesting(
      base::RepeatingClosure callback);
  ~ScopedBackMigratorRestartAttemptForTesting();
};

// The interface is exposed to be inherited by fakes in tests.
class BrowserDataBackMigratorBase {
 public:
  // Represents a result status.
  enum class Result {
    kSucceeded,
    kFailed,
  };

  using BackMigrationFinishedCallback = base::OnceCallback<void(Result)>;
  using BackMigrationProgressCallback = base::RepeatingCallback<void(int)>;
  using BackMigrationCanceledCallback = base::OnceClosure;

  virtual ~BrowserDataBackMigratorBase() = default;

  virtual void Migrate(BackMigrationProgressCallback progress_callback,
                       BackMigrationFinishedCallback finished_callback) = 0;

  virtual void CancelMigration(
      BackMigrationCanceledCallback canceled_callback) = 0;
};

class BrowserDataBackMigrator : public BrowserDataBackMigratorBase {
 public:
  // A list of all the possible results of migration, including success and all
  // failure types in each step of the migration.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // When adding new cases to this enum, also update the following:
  // - `BrowserDataBackMigrator::ToResult()`
  // - `browser_data_back_migrator_metrics::TaskStatusToString()`
  // - `Ash.BrowserDataBackMigrator.PosixErrno.{TaskStatus}` histogram
  // - `BrowserDataBackMigratorFinalStatus` histogram enum
  enum class TaskStatus {
    kSucceeded = 0,
    kPreMigrationCleanUpDeleteTmpDirFailed = 1,
    kMergeSplitItemsCreateTmpDirFailed = 2,
    kMergeSplitItemsCopyExtensionsFailed = 3,
    kMergeSplitItemsCopyExtensionStorageFailed = 4,
    kMergeSplitItemsCreateDirFailed = 5,
    kMergeSplitItemsMergeIndexedDBFailed = 6,
    kMergeSplitItemsMergePrefsFailed = 7,
    kMergeSplitItemsMergeLocalStorageLevelDBFailed = 8,
    kMergeSplitItemsMergeStateStoreLevelDBFailed = 9,
    kMergeSplitItemsMergeSyncDataFailed = 10,
    kDeleteAshItemsDeleteExtensionsFailed = 11,
    kDeleteAshItemsDeleteLacrosItemFailed = 12,
    kDeleteTmpDirDeleteFailed = 13,
    kDeleteLacrosDirDeleteFailed = 14,
    kMoveLacrosItemsToAshDirFailed = 15,
    kMoveMergedItemsBackToAshMoveFileFailed = 16,
    kMoveMergedItemsBackToAshCopyDirectoryFailed = 17,
    kMaxValue = kMoveMergedItemsBackToAshCopyDirectoryFailed,
  };

  struct TaskResult {
    TaskStatus status;

    // Value of `errno` set after a task has failed.
    std::optional<int> posix_errno;
  };

  explicit BrowserDataBackMigrator(const base::FilePath& ash_profile_dir,
                                   const std::string& user_id_hash,
                                   PrefService* local_state);
  BrowserDataBackMigrator(const BrowserDataBackMigrator&) = delete;
  BrowserDataBackMigrator& operator=(const BrowserDataBackMigrator&) = delete;
  ~BrowserDataBackMigrator() override;

  // Calls `chrome::AttemptRestart()` unless
  // `ScopedBackMigratorRestartAttemptForTesting` is in scope.
  static void AttemptRestart();

  // Migrate performs the Lacros -> Ash migration.
  // progress_callback is called repeatedly with the current progress.
  // finished_callback is called when migration completes successfully or with
  // an error.
  // Migrate can only be called once.
  void Migrate(BackMigrationProgressCallback progress_callback,
               BackMigrationFinishedCallback finished_callback) override;

  // IsBackMigrationEnabled determines if the feature is enabled.
  // It checks the following in order:
  // 1. The kForceBrowserDataBackwardMigration debug flag.
  // 2. The LacrosDataBackwardMigrationMode policy.
  // 3. The kLacrosProfileBackwardMigration feature flag.
  // The policy value is cached at the beginning of the session and not
  // updated.
  static bool IsBackMigrationEnabled(
      ash::standalone_browser::migrator_util::PolicyInitState
          policy_init_state);

  // CancelMigration is called when the user chooses to cancel the migration
  // from OOBE and it cleans up the in-progress migration.
  void CancelMigration(
      BackMigrationCanceledCallback canceled_callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest, PreMigrationCleanUp);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest,
                           MergeCommonExtensionsDataFiles);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorFilesSetupTest,
                           MergeCommonIndexedDB);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorFilesSetupTest,
                           MergeLocalStorageLevelDB);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorFilesSetupTest,
                           MergeStateStoreLevelDB);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorFilesSetupTest,
                           DeletesLacrosItemsFromAshDirCorrectly);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorFilesSetupTest,
                           MovesLacrosItemsToAshDirCorrectly);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest,
                           MovesMergedItemsBackToAshCorrectly);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest,
                           MergesAshOnlyPreferencesCorrectly);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest,
                           MergesDictSplitPreferencesCorrectly);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest,
                           MergesListSplitPreferencesCorrectly);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest,
                           MergesLacrosPreferencesCorrectly);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest,
                           MergesDictWithKeysContainingDot);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorShouldMigrateBackTest,
                           CommandLineForceMigration);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorShouldMigrateBackTest,
                           CommandLineForceSkip);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorShouldMigrateBackTest,
                           MaybeRestartToMigrateSecondaryUser);

  enum class MigrationStep {
    kStart = 0,
    kPreMigrationCleanUp = 1,
    kMergeSplitItems = 2,
    kDeleteAshItems = 3,
    kMoveLacrosItemsToAshDir = 4,
    kMoveMergedItemsBackToAsh = 5,
    kDeleteLacrosDir = 6,
    kDeleteTmpDir = 7,
    kMarkMigrationComplete = 8,
    kDone = 9,
    kMaxValue = kDone,
  };

  bool running_ = false;
  BackMigrationProgressCallback progress_callback_;
  BackMigrationFinishedCallback finished_callback_;
  BackMigrationCanceledCallback canceled_callback_;

  void SetProgress(MigrationStep step);

  // Creates `kTmpDir` and deletes its contents if it already exists. Deletes
  // ash and lacros `ItemType::kDeletable` items to free up extra space but this
  // does not affect `PreMigrationCleanUpResult::success`.
  static TaskResult PreMigrationCleanUp(const base::FilePath& ash_profile_dir,
                                        const base::FilePath& lacros_dir);

  // Called as a reply to `PreMigrationCleanUp()`.
  void OnPreMigrationCleanUp(TaskResult result);

  // Merges items that were split between Ash and Lacros and puts them into
  // the temporary directory created in `PreMigrationCleanUp()`.
  static TaskResult MergeSplitItems(const base::FilePath& ash_profile_dir);

  // Called as a reply to `MergeSplitItems()`.
  void OnMergeSplitItems(TaskResult result);

  // Deletes Ash items that will be overwritten by either Lacros items or items
  // merged in `MergeSplitItems()`. This prevents conflicts during the calls to
  // `MoveLacrosItemsToAshDir()` and `MoveMergedItemsBackToAsh()`.
  static TaskResult DeleteAshItems(const base::FilePath& ash_profile_dir);

  // Called as a reply to `DeleteAshItems()`.
  void OnDeleteAshItems(TaskResult result);

  // Moves Lacros-only items back into the Ash profile directory.
  static TaskResult MoveLacrosItemsToAshDir(
      const base::FilePath& ash_profile_dir);

  // Called as a reply to `MoveLacrosItemsToAshDir()`.
  void OnMoveLacrosItemsToAshDir(TaskResult result);

  // Moves the temporary directory into the Ash profile directory.
  static TaskResult MoveMergedItemsBackToAsh(
      const base::FilePath& ash_profile_dir);

  // Called as a reply to `MoveMergedItemsBackToAsh()`.
  void OnMoveMergedItemsBackToAsh(TaskResult result);

  // Deletes the Lacros profile directory.
  static TaskResult DeleteLacrosDir(const base::FilePath& ash_profile_dir);

  // Called as a reply to `DeleteLacrosDir()`.
  void OnDeleteLacrosDir(TaskResult result);

  // Deletes the temporary directory.
  static TaskResult DeleteTmpDir(const base::FilePath& ash_profile_dir);

  // Called as a reply to `DeleteTmpDir()`.
  void OnDeleteTmpDir(TaskResult result);

  // Marks backward migration as complete.
  TaskResult MarkMigrationComplete();

  // Called as a reply to `MarkMigrationComplete()`.
  void OnMarkMigrationComplete();

  // Calls `DeleteLacrosDir()` and `DeleteTmpDir()` to clean up residual Lacros
  // data upon failed migration that was canceled by the user.
  static TaskResult FailedMigrationCleanUp(
      const base::FilePath& ash_profile_dir);

  // Called as a reply to `FailedMigrationCleanUp()`.
  void OnFailedMigrationCleanUp(TaskResult result);

  // For `target_dir` copy subdirectories belonging to extensions that are in
  // both Chromes from `lacros_default_profile_dir` to `tmp_user_dir`.
  static bool MergeCommonExtensionsDataFiles(
      const base::FilePath& ash_profile_dir,
      const base::FilePath& lacros_default_profile_dir,
      const base::FilePath& tmp_user_dir,
      const std::string& target_dir);

  // For `target_dir` delete the subdirectories belonging to extensions from
  // `ash_profile_dir` so that there are no conflicts when `tmp_user_dir` is
  // moved to `ash_profile_dir`.
  static bool RemoveAshCommonExtensionsDataFiles(
      const base::FilePath& ash_profile_dir,
      const std::string& target_dir);

  // Merge IndexedDB objects for extensions that are both in Ash and Lacros.
  // If both exist, delete Ash version and move Lacros version to its place.
  // If only Ash exists, do not delete it, i.e. do nothing.
  // If only Lacros exists, move to the expected Ash location.
  // If neither exists, do nothing.
  static bool MergeCommonIndexedDB(
      const base::FilePath& ash_profile_dir,
      const base::FilePath& lacros_default_profile_dir,
      const char* extension_id);

  // Merge Preferences.
  static bool MergePreferences(const base::FilePath& ash_pref_path,
                               const base::FilePath& lacros_pref_path,
                               const base::FilePath& tmp_pref_path);

  // For Lacros preferences that were neither split nor ash-only,
  // simply prefer them over the ones that are currently in Ash.
  // Traverse all JSON dotted paths in Lacros preferences using
  // depth-first search and merge them into |ash_root_dict|.
  static bool MergeLacrosPreferences(base::Value::Dict& ash_root_dict,
                                     const std::vector<std::string>& path,
                                     const base::Value& current_value);

  // Decides whether preferences for the given `extension_id` should be migrated
  // back from Lacros to Ash.
  static bool IsLacrosOnlyExtension(std::string_view extension_id);

  // Copy the LevelDB database from Lacros to the temporary directory to be used
  // as basis for the merge.
  static bool CopyLevelDBBase(const base::FilePath& lacros_leveldb_dir,
                              const base::FilePath& tmp_leveldb_dir);

  // Overwrite some parts of the LevelDB database copied from Lacros with keys
  // and values from Ash.
  static bool MergeLevelDB(
      const base::FilePath& ash_db_path,
      const base::FilePath& tmp_db_path,
      const browser_data_migrator_util::LevelDBType leveldb_type);

  // Create the Sync Data LevelDB that will be used bu Ash upon backward
  // migration. If only Ash or only Lacros Sync database exists, copy that file
  // directly to the temporary directory. If both databases exist, then perform
  // a full merge by opening both databases, getting the corresponding sync data
  // from each and merging the results into the temporary directory database.
  static bool MergeSyncDataLevelDB(const base::FilePath& ash_db_path,
                                   const base::FilePath& lacros_db_path,
                                   const base::FilePath& tmp_db_path);

  // Go through all top-level items in the directory. If they are files move
  // them directly. If they are directories, call the same function recursively.
  static bool MoveFilesToAshDirectory(const base::FilePath& source_dir,
                                      const base::FilePath& dest_dir,
                                      unsigned int recursion_depth);

  // IsBackMigrationForceEnabled checks if backward migration has been force
  // enabled using the kLacrosProfileBackwardMigration flag.
  static bool IsBackMigrationForceEnabled();

  // ShouldMigrateBack determines if backward migration should run.
  // Called by MaybeRestartToMigrateBack.
  // May block to check if the lacros folder is present.
  static bool ShouldMigrateBack(
      const AccountId& account_id,
      const std::string& user_id_hash,
      ash::standalone_browser::migrator_util::PolicyInitState
          policy_init_state);

  // RestartToMigrateBack triggers a Chrome restart to start backward migration.
  // Called by MaybeRestartToMigrateBack.
  static bool RestartToMigrateBack(const AccountId& account_id);

  // Transforms `TaskResult` to `Result`, which is then returned to the caller.
  static Result ToResult(TaskResult result);

  // Records UMA metrics and calls `finished_callback_`. This function gets
  // called once regardless of whether the migration succeeded or not.
  void InvokeCallback(TaskResult);

  // Path to the ash profile directory.
  const base::FilePath ash_profile_dir_;

  // A hash string of the profile user ID.
  const std::string user_id_hash_;

  // Local state prefs, not owned.
  raw_ptr<PrefService> local_state_ = nullptr;

  // Used to record how long the migration takes in UMA.
  base::TimeTicks migration_start_time_;

  base::WeakPtrFactory<BrowserDataBackMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_H_
