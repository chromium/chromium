// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/account_id/account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace ash {

namespace browser_data_back_migrator {
// Temporary directory for back migration.
constexpr char kTmpDir[] = "back_migrator_tmp";
}  // namespace browser_data_back_migrator

class BrowserDataBackMigrator {
 public:
  // Represents a result status.
  enum class Result {
    kSucceeded,
    kFailed,
  };

  using BackMigrationFinishedCallback =
      base::OnceCallback<void(BrowserDataBackMigrator::Result)>;
  using BackMigrationProgressCallback = base::RepeatingCallback<void(int)>;

  explicit BrowserDataBackMigrator(const base::FilePath& ash_profile_dir,
                                   const std::string& user_id_hash,
                                   PrefService* local_state);
  BrowserDataBackMigrator(const BrowserDataBackMigrator&) = delete;
  BrowserDataBackMigrator& operator=(const BrowserDataBackMigrator&) = delete;
  ~BrowserDataBackMigrator();

  // Migrate performs the Lacros -> Ash migration.
  // progress_callback is called repeatedly with the current progress.
  // finished_callback is called when migration completes successfully or with
  // an error.

  // Migrate can only be called once.
  void Migrate(BackMigrationProgressCallback progress_callback,
               BackMigrationFinishedCallback finished_callback);

  // IsBackMigrationEnabled determines if the feature is enabled.
  // It checks the following in order:
  // 1. The kForceBrowserDataBackwardMigration debug flag.
  // 2. The LacrosDataBackwardMigrationMode policy.
  // 3. The kLacrosProfileBackwardMigration feature flag.
  // The policy value is cached at the beginning of the session and not
  // updated.
  static bool IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState policy_init_state);

  // MaybeRestartToMigrateBack checks if backward migration should be
  // triggered. Migration is started by adding extra flags to Chrome using
  // session_manager and then restarting.
  // Returns true if Chrome needs to restart to trigger backward migration.
  // May block to check if the lacros folder is present.
  static bool MaybeRestartToMigrateBack(
      const AccountId& account_id,
      const std::string& user_id_hash,
      crosapi::browser_util::PolicyInitState policy_init_state);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest, PreMigrationCleanUp);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorTest,
                           MergeCommonExtensionsDataFiles);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataBackMigratorFilesSetupTest,
                           MergeCommonIndexedDB);

  // A list of all the possible results of migration, including success and all
  // failure types in each step of the migration.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
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
    kMoveLacrosItemsToTmpDirMoveFailed = 15,
    kMoveMergedItemsBackToAshMoveFileFailed = 16,
    kMoveMergedItemsBackToAshCopyDirectoryFailed = 17,
    kMaxValue = kMoveMergedItemsBackToAshCopyDirectoryFailed,
  };

  enum class MigrationStep {
    kStart = 0,
    kPreMigrationCleanUp = 1,
    kMergeSplitItems = 2,
    kMoveLacrosItemsToTmpDir = 3,
    kDeleteAshItems = 4,
    kMoveMergedItemsBackToAsh = 5,
    kDeleteLacrosDir = 6,
    kDeleteTmpDir = 7,
    kMarkMigrationComplete = 8,
    kDone = 9,
    kMaxValue = kDone,
  };

  struct TaskResult {
    TaskStatus status;

    // Value of `errno` set after a task has failed.
    absl::optional<int> posix_errno;
  };

  bool running_ = false;
  BackMigrationProgressCallback progress_callback_;
  BackMigrationFinishedCallback finished_callback_;

  void SetProgress(MigrationStep step);

  // Creates `kTmpDir` and deletes its contents if it already exists. Deletes
  // ash and lacros `ItemType::kDeletable` items to free up extra space but this
  // does not affect `PreMigrationCleanUpResult::success`.
  static TaskResult PreMigrationCleanUp(
      const base::FilePath& ash_profile_dir,
      const base::FilePath& lacros_profile_dir);

  // Called as a reply to `PreMigrationCleanUp()`.
  void OnPreMigrationCleanUp(TaskResult result);

  // Merges items that were split between Ash and Lacros and puts them into
  // the temporary directory created in `PreMigrationCleanUp()`.
  static TaskResult MergeSplitItems(const base::FilePath& ash_profile_dir);

  // Called as a reply to `MergeSplitItems()`.
  void OnMergeSplitItems(TaskResult result);

  // Deletes Ash items that will be overwritten by either Lacros items or items
  // merged in `MergeSplitItems()`. This prevents conflicts during the calls to
  // `MoveLacrosItemsToTmpDir()` and `MoveMergedItemsBackToAsh()`.
  static TaskResult DeleteAshItems(const base::FilePath& ash_profile_dir);

  // Called as a reply to `DeleteAshItems()`.
  void OnDeleteAshItems(TaskResult result);

  // Moves Lacros-only items back into the Ash profile directory.
  static TaskResult MoveLacrosItemsToTmpDir(
      const base::FilePath& ash_profile_dir);

  // Called as a reply to `MoveLacrosItemsToTmpDir()`.
  void OnMoveLacrosItemsToTmpDir(TaskResult result);

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

  // For `target_dir` copy subdirectories belonging to extensions that are in
  // both Chromes from `lacros_profile_dir` to `tmp_user_dir`.
  static bool MergeCommonExtensionsDataFiles(
      const base::FilePath& ash_profile_dir,
      const base::FilePath& lacros_profile_dir,
      const base::FilePath& tmp_user_dir,
      const std::string& target_dir);

  // For `target_dir` delete the subdirectories belonging to extensions from
  // `ash_profile_dir` so that there are no conflicts when `tmp_user_dir` is
  // moved to `ash_profile_dir`.
  static bool RemoveAshCommonExtensionsDataFiles(
      const base::FilePath& ash_profile_dir,
      const std::string& target_dir);

  // Merge IndexedDB objects for extensions that are both in Ash and Lacros.
  // If both exists, delete Ash version and move Lacros version to its place.
  // If only Ash exists, do not delete it, i.e. do nothing.
  // If only Lacros exists, move to the expected Ash location.
  // If neither exists, do nothing.
  static bool MergeCommonIndexedDB(const base::FilePath& ash_profile_dir,
                                   const base::FilePath& lacros_profile_dir,
                                   const char* extension_id);

  // Merge Preferences.
  static bool MergePreferences(const base::FilePath& ash_pref_path,
                               const base::FilePath& lacros_pref_path,
                               const base::FilePath& tmp_pref_path);

  // Decides whether preferences for the given `extension_id` should be migrated
  // back from Lacros to Ash.
  static bool IsLacrosOnlyExtension(const base::StringPiece extension_id);

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

  // Transforms `TaskResult` to `Result`, which is then returned to the caller.
  static Result ToResult(TaskResult result);

  // IsBackMigrationForceEnabled checks if backward migration has been force
  // enabled using the kLacrosProfileBackwardMigration flag.
  static bool IsBackMigrationForceEnabled();

  // ShouldMigrateBack determines if backward migration should run.
  // Called by MaybeRestartToMigrateBack.
  // May block to check if the lacros folder is present.
  static bool ShouldMigrateBack(
      const AccountId& account_id,
      const std::string& user_id_hash,
      crosapi::browser_util::PolicyInitState policy_init_state);

  // RestartToMigrateBack triggers a Chrome restart to start backward migration.
  // Called by MaybeRestartToMigrateBack.
  static bool RestartToMigrateBack(const AccountId& account_id);

  // Path to the ash profile directory.
  const base::FilePath ash_profile_dir_;

  // A hash string of the profile user ID.
  const std::string user_id_hash_;

  // Local state prefs, not owned.
  PrefService* local_state_ = nullptr;

  base::WeakPtrFactory<BrowserDataBackMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_H_
