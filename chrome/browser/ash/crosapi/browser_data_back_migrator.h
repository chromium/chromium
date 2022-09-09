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
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  explicit BrowserDataBackMigrator(const base::FilePath& ash_profile_dir);
  BrowserDataBackMigrator(const BrowserDataBackMigrator&) = delete;
  BrowserDataBackMigrator& operator=(const BrowserDataBackMigrator&) = delete;
  ~BrowserDataBackMigrator();

  void Migrate(BackMigrationFinishedCallback finished_callback);

 private:
  // A list of all the possible results of migration, including success and all
  // failure types in each step of the migration.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class TaskStatus {
    kSucceeded = 0,
    kPreMigrationCleanUpDeleteTmpDirFailed = 1,
    kMaxValue = kPreMigrationCleanUpDeleteTmpDirFailed,
  };

  struct TaskResult {
    TaskStatus status;

    // Value of `errno` set after a task has failed.
    absl::optional<int> posix_errno;
  };

  // Creates `kTmpDir` and deletes its contents if it already exists. Deletes
  // ash and lacros `ItemType::kDeletable` items to free up extra space but this
  // does not affect `PreMigrationCleanUpResult::success`.
  static TaskResult PreMigrationCleanUp(
      const base::FilePath& ash_profile_dir,
      const base::FilePath& lacros_profile_dir);

  // Called as a reply to `PreMigrationCleanUp()`.
  void OnPreMigrationCleanUp(BackMigrationFinishedCallback finished_callback,
                             TaskResult result);

  void InvokeCallback(TaskResult);

  // Path to the ash profile data directory.
  const base::FilePath ash_profile_dir_;

  base::WeakPtrFactory<BrowserDataBackMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_H_
