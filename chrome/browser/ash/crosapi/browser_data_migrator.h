// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/migration_progress_tracker.h"
#include "components/account_id/account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

// Local state pref name, which is used to keep track of what step migration is
// at. This ensures that ash does not get restarted repeatedly for migration.
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

// Injects the restart function called from
// `BrowserDataMigratorImpl::AttemptRestart()` in RAII manner.
class ScopedRestartAttemptForTesting {
 public:
  explicit ScopedRestartAttemptForTesting(base::RepeatingClosure callback);
  ~ScopedRestartAttemptForTesting();
};

// The interface is exposed to be inherited by fakes in tests.
class BrowserDataMigrator {
 public:
  // Represents a kind of the result status.
  enum class ResultKind {
    kSkipped,
    kSucceeded,
    kFailed,
    kCancelled,
  };

  // Represents a result status.
  struct Result {
    ResultKind kind;

    // If the migration is failed (kind must be kFailed) due to
    // out-of-diskspace, this field will be filled with the size of the disk
    // in bytes where the user required to free up.
    absl::optional<uint64_t> required_size;
  };

  // TODO(crbug.com/1296174): Currently, dependency around callback is not
  // clean enough. Clean it up.
  using MigrateCallback = base::OnceCallback<void(Result)>;

  virtual ~BrowserDataMigrator() = default;

  // Carries out the migration with the mode specified by `MigrationMode`. It
  // needs to be called on UI thread. |callback| will be called on the end of
  // the migration procedure.
  virtual void Migrate(crosapi::browser_util::MigrationMode mode,
                       MigrateCallback callback) = 0;

  // Cancels the migration. This should be called on UI thread.
  // If this is called during the migration, it is expected that |callback|
  // passed to Migrate() will be called with kCancelled *in most cases*.
  // Note that, there's timing issue, so the migration may be completed
  // just before the notification to cancel, and in the case |callback|
  // may be called with other ResultKind.
  virtual void Cancel() = 0;
};

// BrowserDataMigratorImpl is responsible for one time browser data migration
// from ash-chrome to lacros-chrome. It is responsible for coordination the
// overrall flow of the migration from checking whether migration is required to
// marking migration as completed. The actual task of migration (i.e. setting up
// the profile directories for ash and lacros) is delegated to
// `MigratorDelegate`.
class BrowserDataMigratorImpl : public BrowserDataMigrator {
 public:
  // The value for `kMigrationStep`.
  enum class MigrationStep {
    kCheckStep = 0,      // Migration check should run.
    kRestartCalled = 1,  // `MaybeRestartToMigrate()` called restart.
    kStarted = 2,        // `Migrate()` was called.
    kEnded = 3  // Migration ended. It was either skipped, failed or succeeded.
  };

  enum class DataWipeResult { kSkipped, kSucceeded, kFailed };

  // TODO(ythjkt): Move this struct to browser_data_migrator_util.h.
  // Return value of `MigrateInternal()`.
  struct MigrationResult {
    // Describes the end result of user data wipe.
    DataWipeResult data_wipe_result;
    // Describes the end result of data migration.
    Result data_migration_result;
  };

  // Delegate interface which is responsible for the actual task of setting up
  // the profile directories for ash and lacros. The class should call
  // `MigrateInternalFinishedUIThread()` once migration is completed.
  class MigratorDelegate {
   public:
    virtual ~MigratorDelegate() = default;
    virtual void Migrate() = 0;
  };

  // `BrowserDataMigratorImpl` migrates browser data from `original_profile_dir`
  // to a new profile location for lacros chrome. `progress_callback` is called
  // to update the progress bar on the screen. `completion_callback` passed as
  // an argument will be called on the UI thread where `Migrate()` is called
  // once migration has completed or failed.
  BrowserDataMigratorImpl(const base::FilePath& original_profile_dir,
                          const std::string& user_id_hash,
                          const ProgressCallback& progress_callback,
                          PrefService* local_state);
  BrowserDataMigratorImpl(const BrowserDataMigratorImpl&) = delete;
  BrowserDataMigratorImpl& operator=(const BrowserDataMigratorImpl&) = delete;
  ~BrowserDataMigratorImpl() override;

  // Calls `chrome::AttemptRestart()` unless `ScopedRestartAttemptForTesting` is
  // in scope.
  static void AttemptRestart();

  // Check if move migration has to be resumed. This has to be checked before a
  // Profile object is created using the user's profile data directory. Like
  // `MaybeRestartToMigrate()` it returns true if the D-Bus call to the
  // session_manager is made and successful. The return value of true means that
  // `chrome::AttemptRestart()` has been called.
  static bool MaybeForceResumeMoveMigration(
      PrefService* local_state,
      const AccountId& account_id,
      const std::string& user_id_hash,
      crosapi::browser_util::PolicyInitState policy_init_state);

  // Checks if migration is required for the user identified by `user_id_hash`
  // and if it is required, calls a D-Bus method to session_manager and
  // terminates ash-chrome. It returns true if the D-Bus call to the
  // session_manager is made and successful. The return value of true means that
  // `chrome::AttemptRestart()` has been called.
  static bool MaybeRestartToMigrate(
      const AccountId& account_id,
      const std::string& user_id_hash,
      crosapi::browser_util::PolicyInitState policy_init_state);

  // Very similar to `MaybeRestartToMigrate`, but this checks the disk space in
  // addition, and reports an error if out of disk space case.
  // |callback| will be called on completion.
  // On success, the first argument of the |callback| will be true, and the
  // second argument should be ignored.
  // On error, the first argument of the |callback| will be false. If the error
  // is caused by out-of-disk, the required size to be freed up is passed
  // to the second argument. Otherwise the second argument is nullopt.
  static void MaybeRestartToMigrateWithDiskCheck(
      const AccountId& account_id,
      const std::string& user_id_hash,
      base::OnceCallback<void(bool, const absl::optional<uint64_t>&)> callback);

  // `BrowserDataMigrator` methods.
  void Migrate(crosapi::browser_util::MigrationMode mode,
               MigrateCallback callback) override;
  void Cancel() override;

  // Registers boolean pref `kCheckForMigrationOnRestart` with default as false.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Clears the value of `kMigrationStep` in Local State.
  static void ClearMigrationStep(PrefService* local_state);

  // Resets the number of migration attempts for the user stored in
  // `kMigrationAttemptCountPref.
  static void ClearMigrationAttemptCountForUser(
      PrefService* local_state,
      const std::string& user_id_hash);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorImplTest,
                           ManipulateMigrationAttemptCount);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorImplTest, Migrate);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorImplTest, MigrateCancelled);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorImplTest,
                           MigrateOutOfDiskForCopy);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorImplTest,
                           MigrateOutOfDiskForMove);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorRestartTest,
                           MaybeRestartToMigrateWithMigrationStep);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorRestartTest,
                           MaybeRestartToMigrateMoveAfterCopy);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorRestartTest,
                           LacrosProfileMigrationForAnyUserDisabled);
  FRIEND_TEST_ALL_PREFIXES(BrowserDataMigratorRestartTest,
                           LacrosProfileMigrationForAnyUserDisabledForGoogler);

  // The common implementation of `MaybeRestartToMigrate` and
  // `MaybeRestartToMigrateWithDiskCheck`.
  static bool MaybeRestartToMigrateInternal(
      const AccountId& account_id,
      const std::string& user_id_hash,
      crosapi::browser_util::PolicyInitState policy_init_state);

  // A part of `MaybeRestartToMigrateWithDiskCheck`, runs after the disk check.
  static void MaybeRestartToMigrateWithDiskCheckAfterDiskCheck(
      const AccountId& account_id,
      const std::string& user_id_hash,
      base::OnceCallback<void(bool, const absl::optional<uint64_t>&)> callback,
      uint64_t required_size);

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

  // Called from `MaybeRestartToMigrate()` to proceed with restarting to start
  // the migration. It returns true if D-Bus call was successful.
  static bool RestartToMigrate(
      const AccountId& account_id,
      const std::string& user_id_hash,
      PrefService* local_state,
      crosapi::browser_util::PolicyInitState policy_init_state);

  // Called on UI thread once migration is finished.
  void MigrateInternalFinishedUIThread(
      crosapi::browser_util::MigrationMode mode,
      MigrationResult result);

  // Path to the original profile data directory, which is directly under the
  // user data directory.
  const base::FilePath original_profile_dir_;
  // A hash string of the profile user ID.
  const std::string user_id_hash_;
  // `progress_tracker_` is used to report progress status to the screen.
  std::unique_ptr<MigrationProgressTracker> progress_tracker_;
  // Callback to be called once migration is done. It is called regardless of
  // whether migration succeeded or not.
  MigrateCallback completion_callback_;
  // `cancel_flag_` gets set by `Cancel()` and tasks posted to worker threads
  // can check if migration is cancelled or not.
  scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag_;
  // Local state prefs, not owned.
  PrefService* local_state_ = nullptr;
  std::unique_ptr<MigratorDelegate> migrator_delegate_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrowserDataMigratorImpl> weak_factory_{this};
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_MIGRATOR_H_
