// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_MOVE_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_MOVE_MIGRATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
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

// This class "moves" Lacros data from Ash to Lacros. It migrates user data from
// `original_profile_dir` (/home/user/<hash>/), denoted as <Ash PDD> from here
// forward, to the new profile data directory
// (/<Ash PDD>/lacros/Default/) with the steps described below. The renaming of
// <kMoveTmpDir> is the last step of the migration so that the existence of
// <Ash PDD>/lacros/ is equivalent to having completed the migration. Also it
// ensures that there is no state in which Ash and Lacros have access to the
// same inode via two hardlinks.
// 1) Delete any `ItemType::kDeletable` items in <Ash PDD>.
// 2) Setup <Ash PDD>/<kMoveTmpDir> by copying `ItemType::kNeedCopy`
// items into it and creating hard links for `ItemType::kLacros` in it.
// 3) Delete the original hard links for `ItemType::kLacros` in <Ash PDD>. If
// deletion fails, move them to `<Ash PDD>/<kRemoveDir>` to make them
// inaccessible by Ash.
// 4) Rename <Ash PDD>/<kMoveTmpDir>/ as <Ash PDD>/lacros/.
class MoveMigrator : public BrowserDataMigratorImpl::MigratorDelegate {
 public:
  // Indicate which step the migration should be resumed from if left unfinished
  // in the previous attempt.
  enum class ResumeStep {
    kStart = 0,
    kRemoveHardLinks = 1,
    kMoveTmpDir = 2,
    kCompleted = 3,
  };

  // Return value of `PreMigrationCleanUp()`.
  struct PreMigrationCleanUpResult {
    // Whether cleanup has succeeded or not.
    bool success;

    // Extra bytes required to be freed if the migrator requires more space to
    // be carried out. Only set if `success` is true. This value is set to 0 if
    // no freeing up of disk is required.
    absl::optional<uint64_t> extra_bytes_required_to_be_freed;
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

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest, PreMigrationCleanUp);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest, SetupLacrosDir);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest,
                           SetupLacrosDirFailIfNoWritePermForLacrosItem);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorTest, RemoveHardLinksFromOriginalDir);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorMigrateTest,
                           MigrateResumeFromRemoveHardLinks);
  FRIEND_TEST_ALL_PREFIXES(MoveMigratorMigrateTest, MigrateResumeFromMove);

  // Called in `Migrate()` to determine where to start the migration. Returns
  // `ResumeStep::kStart` unless there is a step recorded in `Local State` from
  // the previous migration i.e. the previous migration did not complete and
  // crashed halfway.
  static ResumeStep GetResumeStep(PrefService* local_state,
                                  const std::string& user_id_hash);

  // Sets `kMoveMigrationResumeStepPref` in `Local State` for `user_id_hash`.
  static void SetResumeStep(PrefService* local_state,
                            const std::string& user_id_hash,
                            const ResumeStep step);

  // Deletes lacros user directory and `kMoveTmpDir` if they exist. Set
  // `PreMigrationCleanUpResult::success` to true if the deletion of those
  // directories are successful. If the deletion is successful then it also
  // checks if there is enough disk space available and set
  // `PreMigrationCleanUpResult::extra_bytes_required_to_be_freed`. It also
  // deletes `ItemType::kDeletable` items to free up extra space but this does
  // not affect `PreMigrationCleanUpResult::success`.
  static PreMigrationCleanUpResult PreMigrationCleanUp(
      const base::FilePath& original_profile_dir);

  // Called as a reply to `PreMigrationCleanUp()`.  Posts
  // `SetupLacrosRemoveHardLinksFromAshDir()` as the next step.
  void OnPreMigrationCleanUp(PreMigrationCleanUpResult);

  // Set up lacros user directory by copying `ItemType::kNeedCopy` items and
  // creating hard links for `ItemType::kLacros` into it.
  static bool SetupLacrosDir(
      const base::FilePath& original_profile_dir,
      std::unique_ptr<MigrationProgressTracker> progress_tracker,
      scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag);

  // Called as a reply to `SetupLacrosDir()`. Posts
  // `SetupLacrosRemoveHardLinksFromAshDir()` as the next step.
  void OnSetupLacrosDir(bool success);

  // Removes hard links for `ItemType::kLacros` in the original profile
  // directory. Hard links pointing to the same inode should have been created
  // in `OnSetupLacrosDir()` inside lacros profile directory.
  static bool RemoveHardLinksFromOriginalDir(
      const base::FilePath& original_profile_dir);

  // Called as a reply to `RemoveHardLinksFromOriginalDir()`.
  void OnRemoveHardLinksFromOriginalDir(bool success);

  // Moves newly created `kMoveTmpDir` to `kLacrosDir` to complete the
  // migration.
  static bool MoveTmpDirToLacrosDir(const base::FilePath& original_profile_dir);

  // Called as a reply to `MoveTmpDirToLacrosDir()`.
  void OnMoveTmpDirToLacrosDir(bool success);

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
  PrefService* local_state_;

  // `finished_callback_` should be called once migration is completed/failed.
  // Call this on UI thread.
  MigrationFinishedCallback finished_callback_;

  base::WeakPtrFactory<MoveMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CROSAPI_MOVE_MIGRATOR_H_
