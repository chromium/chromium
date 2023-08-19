// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_MOVE_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_MOVE_MIGRATOR_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/migration_progress_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

using MigrationFinishedCallback =
    base::OnceCallback<void(BrowserDataMigratorImpl::MigrationResult)>;

// Dictionary pref storing user id hash as key and `ResumeStep` as value.
constexpr char kMoveMigrationResumeStepPref[] =
    "ash.browser_data_migrator.move_migration_resume_step";

// Dictionary pref storing user id hash as key and number of resumes in int as
// value.
constexpr char kMoveMigrationResumeCountPref[] =
    "ash.browser_data_migrator.move_migration_resume_count";

// The number of maximum resume tries for `MoveMigrator`. If the limit is
// reached then move migration is marked as completed without actually
// completing the migration.
constexpr int kMoveMigrationResumeCountLimit = 5;

// The following are UMA names.
constexpr char kMoveMigratorResumeCount[] =
    "Ash.BrowserDataMigrator.MoveMigrator.ResumeCount";
constexpr char kMoveMigratorResumeStepUMA[] =
    "Ash.BrowserDataMigrator.MoveMigrator.ResumeStep";
constexpr char kMoveMigratorMaxResumeReached[] =
    "Ash.BrowserDataMigrator.MoveMigrator.MaxResumeReached";
constexpr char kMoveMigratorTaskStatusUMA[] =
    "Ash.BrowserDataMigrator.MoveMigrator.TaskStatus";
constexpr char kMoveMigratorExtraSpaceRequiredMB[] =
    "Ash.BrowserDataMigrator.MoveMigrator.ExtraSpaceRequiredMB";
constexpr char kMoveMigratorPreMigrationCleanUpTimeUMA[] =
    "Ash.BrowserDataMigrator.MoveMigrator.PreMigrationCleanUpTime";
constexpr char kMoveMigratorSetupLacrosDirCopyTargetItemsTimeUMA[] =
    "Ash.BrowserDataMigrator.MoveMigrator.SetupLacrosDirCopyTargetItemsTime";
constexpr char kMoveMigratorCancelledMigrationTimeUMA[] =
    "Ash.BrowserDataMigrator.MoveMigrator.CancelledMigrationTime";
constexpr char kMoveMigratorSuccessfulMigrationTimeUMA[] =
    "Ash.BrowserDataMigrator.MoveMigrator.SuccessfulMigrationTime";
constexpr char kMoveMigratorMoveLacrosItemsTimeUMA[] =
    "Ash.BrowserDataMigrator.MoveMigrator.MoveLacrosItemsTime";
constexpr char kMoveMigratorPosixErrnoUMA[] =
    "Ash.BrowserDataMigrator.MoveMigrator.PosixErrno.";
constexpr char kMoveMigratorTmpProfileDirSize[] =
    "Ash.BrowserDataMigrator.MoveMigrator.TmpProfileDirSize";
constexpr char kMoveMigratorTmpSplitDirSize[] =
    "Ash.BrowserDataMigrator.MoveMigrator.TmpSplitDirSize";
constexpr char kMoveMigratorExtraDiskSpaceOccupied[] =
    "Ash.BrowserDataMigrator.MoveMigrator.ExtraDiskSpaceOccupied";
constexpr char kMoveMigratorExtraDiskSpaceOccupiedDiffWithEst[] =
    "Ash.BrowserDataMigrator.MoveMigrator.ExtraDiskSpaceOccupied."
    "DiffWithEstimate";

// This class "moves" Lacros data from Ash to Lacros. It migrates user data from
// `original_profile_dir` (/home/user/<hash>/), denoted as <Ash PDD> from here
// forward, to the new profile data directory
// (/<Ash PDD>/lacros/Default/) with the steps described below. The renaming of
// <kMoveTmpDir> is the last step of the migration so that the existence of
// <Ash PDD>/lacros/ is equivalent to having completed the migration.
// 1) Delete any `ItemType::kDeletable` items in <Ash PDD>.
// 2) Setup <Ash PDD>/<kMoveTmpDir> by copying `ItemType::kNeedCopy`
// items into it.
// 3) Setup <Ash PDD>/<kSplitTmpDir> by generating split data that will have to
// remain in Ash.
// 4) Move `ItemType::kLacros` in <Ash PDD> to <lacros PDD>.
// 5) Move split items in <Ash PDD>/<kSplitTmpDir> to <Ash PDD>.
// 6) Rename <Ash PDD>/<kMoveTmpDir>/ as <Ash PDD>/lacros/.
class MoveMigrator : public BrowserDataMigratorImpl::MigratorDelegate {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Indicate which step the migration should be resumed from if left unfinished
  // in the previous attempt.
  enum class ResumeStep {
    kStart = 0,
    kMoveLacrosItems = 1,
    kMoveSplitItems = 2,
    kMoveTmpDir = 3,
    kCompleted = 4,
    kMaxValue = kCompleted,
  };

  MoveMigrator(
      const base::FilePath& original_profile_dir,
      const std::string& user_id_hash,
      std::unique_ptr<MigrationProgressTracker> progress_tracker,
      scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag,
      PrefService* local_state,
      MigrationFinishedCallback finished_callback);
  MoveMigrator(const MoveMigrator&) = delete;
  MoveMigrator& operator=(const MoveMigrator&) = delete;
  ~MoveMigrator() override;

  // BrowserDataMigratorImpl::MigratorDelegate override.
  void Migrate() override;

  // Gets the `ResumeStep` for the user stored in `local_state` and checks if
  // move migration has to be resumed by calling `IsResumeStep()`.
  static bool ResumeRequired(PrefService* local_state,
                             const std::string& user_id_hash);

  // Resets the number of resume attempts for the user stored in
  // `kMoveMigrationResumeCountPref.
  static void ClearResumeAttemptCountForUser(PrefService* local_state,
                                             const std::string& user_id_hash);

  // Clears `ResumeStep` for user stored in `kMoveMigrationResumeStepPref`.
  static void ClearResumeStepForUser(PrefService* local_state,
                                     const std::string& user_id_hash);

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest, ResumeRequired);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest, PreMigrationCleanUp);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest, SetupLacrosDir);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest, SetupAshSplitDir);
  FRIEND_TEST_ALL_PREFIXES(
      MoveMigratorTest,
      MoveLacrosItemsToNewDirFailIfNoWritePermForLacrosItem);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest, MoveLacrosItemsToNewDir);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest, RecordPosixErrnoUMA);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorMigrateTest,
                           MigrateResumeFromMoveLacrosItems);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorMigrateTest,
                           MigrateResumeFromMoveSplitItems);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorMigrateTest,
                           MigrateResumeFromMoveTmpDir);
  friend class BrowserDataMigratorResumeOnSignIn;
  friend class BrowserDataMigratorResumeRestartInSession;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // This enum corresponds to MoveMigratorTaskStatus in histograms.xml
  // and enums.xml.
  enum class TaskStatus {
    kSucceeded = 0,
    kCancelled = 1,
    kPreMigrationCleanUpDeleteLacrosDirFailed = 2,
    kPreMigrationCleanUpDeleteTmpDirFailed = 3,
    kPreMigrationCleanUpDeleteTmpSplitDirFailed = 4,
    kPreMigrationCleanUpNotEnoughSpace = 5,
    kSetupLacrosDirCreateTmpDirFailed = 6,
    kSetupLacrosDirCreateTmpProfileDirFailed = 7,
    kSetupLacrosDirCopyTargetItemsFailed = 8,
    kSetupLacrosDirWriteFirstRunSentinelFileFailed = 9,
    kSetupAshDirCreateSplitDirFailed = 10,
    kSetupAshDirMigrateLevelDBForLocalStateFailed = 11,
    kSetupAshDirMigrateLevelDBForStateFailed = 12,
    kSetupAshDirMigratePreferencesFailed = 13,
    kMoveLacrosItemsToNewDirNoWritePerm = 14,
    kMoveLacrosItemsToNewDirMoveFailed = 15,
    kMoveSplitItemsToOriginalDirMoveSplitItemsFailed = 16,
    kMoveSplitItemsToOriginalDirCreateDirFailed = 17,
    kMoveSplitItemsToOriginalDirMoveExtensionsFailed = 18,
    kMoveSplitItemsToOriginalDirMoveIndexedDBFailed = 19,
    kMoveTmpDirToLacrosDirMoveFailed = 20,
    kSetupAshDirCreateDirFailed = 21,
    kSetupAshDirCopyExtensionsFailed = 22,
    kSetupAshDirCopyIndexedDBFailed = 23,
    kSetupAshDirMigrateSyncDataLevelDBFailed = 24,
    kSetupAshDirCopyStorageFailed = 25,
    kMoveSplitItemsToOriginalDirMoveStorageFailed = 26,
    kMoveLacrosItemsCreateDirFailed = 27,
    kMaxValue = kMoveLacrosItemsCreateDirFailed,
  };

  struct TaskResult {
    TaskStatus status;

    // Value of `errno` set after a task has failed.
    absl::optional<int> posix_errno;

    // Extra bytes required to be freed if the migrator requires more space to
    // be carried out. Only set if `status` is
    // `kPreMigrationCleanUpNotEnoughSpace`.
    absl::optional<uint64_t> extra_bytes_required_to_be_freed;

    // Extra bytes that are estimated to be created due to the migration. It
    // will later be used to calculate the diff between this estimate and the
    // actual value.
    absl::optional<int64_t> estimated_extra_bytes_created;
  };

  // Called to determine where to start the migration. Returns
  // `ResumeStep::kStart` unless there is a step recorded in `Local State` from
  // the previous migration i.e. the previous migration did not complete and
  // crashed halfway.
  static ResumeStep GetResumeStep(PrefService* local_state,
                                  const std::string& user_id_hash);

  // Sets `kMoveMigrationResumeStepPref` in `Local State` for `user_id_hash`.
  static void SetResumeStep(PrefService* local_state,
                            const std::string& user_id_hash,
                            ResumeStep step);

  // Returns true if `resume_step` indicates that the migration had been left
  // unfinished in the previous attempt and that it must be resumed before user
  // profile is created.
  static bool IsResumeStep(ResumeStep resume_step);

  //  Increments the resume attempt count stored in
  // `kMoveMigrationResumeCountPref` by 1 for the user identified by
  // `user_id_hash`. Returns the updated resume count.
  int UpdateResumeAttemptCountForUser(PrefService* local_state,
                                      const std::string& user_id_hash);

  // Deletes lacros user directory and `kMoveTmpDir` if they exist. Set
  // `PreMigrationCleanUpResult::success` to true if the deletion of those
  // directories are successful. If the deletion is successful then it also
  // checks if there is enough disk space available and set
  // `PreMigrationCleanUpResult::extra_bytes_required_to_be_freed`. It also
  // deletes `ItemType::kDeletable` items to free up extra space but this does
  // not affect `PreMigrationCleanUpResult::success`.
  static TaskResult PreMigrationCleanUp(
      const base::FilePath& original_profile_dir);

  // Called as a reply to `PreMigrationCleanUp()`.  Posts
  // `SetupLacrosRemoveHardLinksFromAshDir()` as the next step.
  void OnPreMigrationCleanUp(TaskResult);

  // Set up lacros user directory by copying `ItemType::kNeedCopy` items
  // and also creating `First Run` file in Lacros user data dir.
  static TaskResult SetupLacrosDir(
      const base::FilePath& original_profile_dir,
      std::unique_ptr<MigrationProgressTracker> progress_tracker,
      scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag);

  // Called as a reply to `SetupLacrosDir()`. Posts
  // `SetupAshSplitDir()` as the next step.
  void OnSetupLacrosDir(TaskResult);

  // Set up a temporary directory to hold items that need to be split between
  // ash and lacros. This folder will hold ash's version of the items.
  static TaskResult SetupAshSplitDir(
      const base::FilePath& original_profile_dir,
      const int64_t estimated_extra_bytes_created);

  // Called as a reply to `SetupAshSplitDir()`. Posts `MoveLacrosItemsToNewDir`
  // as the next step.
  void OnSetupAshSplitDir(TaskResult);

  // Move `ItemType::kLacros` in the original profile
  // directory to the temp dir.
  static TaskResult MoveLacrosItemsToNewDir(
      const base::FilePath& original_profile_dir);

  // Called as a reply to `MoveLacrosItemsToNewDir()`.
  void OnMoveLacrosItemsToNewDir(TaskResult);

  // Moves newly created split items to the original profile directory.
  static TaskResult MoveSplitItemsToOriginalDir(
      const base::FilePath& original_profile_dir);

  // Called as a reply to `MoveSplitItemsToOriginalDir`.
  void OnMoveSplitItemsToOriginalDir(TaskResult);

  // Moves newly created `kMoveTmpDir` to `kLacrosDir`.
  // Completes the migration.
  static TaskResult MoveTmpDirToLacrosDir(
      const base::FilePath& original_profile_dir);

  // Called as a reply to `MoveTmpDirToLacrosDir()`.
  void OnMoveTmpDirToLacrosDir(TaskResult);

  // Selectively copy `target_dir` from `original_profile_dir` to
  // `tmp_split_dir`. Only copy the subdirectories belonging to extensions
  // that have to stay in both Ash and Lacros.
  // If copying fails, return a TaskResult with `copy_fail_status` TaskStatus.
  static TaskResult CopyBothChromesSubdirs(
      const base::FilePath& original_profile_dir,
      const base::FilePath& tmp_split_dir,
      const std::string& target_dir,
      TaskStatus copy_fail_status);

  // Selectively move `target_dir` from `tmp_profile_dir` to
  // `original_profile_dir`. Only move the subdirectories belonging to
  // extensions that have to be in Ash only.
  // If moving fails, return a TaskResult with `move_fail_status` TaskStatus.
  static TaskResult MoveAshSubdirs(const base::FilePath& tmp_profile_dir,
                                   const base::FilePath& original_profile_dir,
                                   const std::string& target_dir,
                                   TaskStatus move_fail_status);

  // Records the final status of the migration in `kMoveMigratorTaskStatusUMA`
  // and calls `finished_callback_`. This function gets called once regardless
  // of whether the migration succeeded or not.
  void InvokeCallback(TaskResult);

  // Converts `TaskResult` to `BrowserDataMigratorImpl::MigrationResult`.
  BrowserDataMigratorImpl::MigrationResult ToBrowserDataMigratorMigrationResult(
      TaskResult result);

  // Record UMA of the form
  // "Ash.BrowserDataMigrator.MoveMigrator.PosixErrno.{task_status}" with the
  // value of `errno`.
  static void RecordPosixErrnoUMA(TaskStatus task_status,
                                  const int posix_errno);

  // Convert `TaskStatus` to string.
  static std::string TaskStatusToString(TaskStatus task_status);

  // Path to the original profile data directory, which is directly under the
  // user data directory.
  const base::FilePath original_profile_dir_;

  // A hash string of the profile user ID.
  const std::string user_id_hash_;

  // `progress_tracker_` is used to report progress status to the screen.
  std::unique_ptr<MigrationProgressTracker> progress_tracker_;

  // `cancel_flag_` gets set by `BrowserDataMigratorImpl::Cancel()` and tasks
  // posted to worker threads can check if migration is cancelled or not.
  scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag_;

  // Local state prefs, not owned.
  const raw_ptr<PrefService, ExperimentalAsh> local_state_;

  // `finished_callback_` should be called once migration is completed/failed.
  // Call this on UI thread.
  MigrationFinishedCallback finished_callback_;

  // Timer to count time since the initialization of the class. Used to get UMA
  // data on how long the migration takes.
  const base::ElapsedTimer timer_;

  // Extra bytes that are estimated to be created due to the migration.
  absl::optional<int64_t> estimated_extra_bytes_created_;

  base::WeakPtrFactory<MoveMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CROSAPI_MOVE_MIGRATOR_H_
