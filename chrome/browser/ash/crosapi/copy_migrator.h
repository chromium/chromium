// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_COPY_MIGRATOR_H_
#define CHROME_BROWSER_ASH_CROSAPI_COPY_MIGRATOR_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/migration_progress_tracker.h"

namespace ash {

// The following are UMA names.
constexpr char kFinalStatus[] = "Ash.BrowserDataMigrator.FinalStatus";
constexpr char kCopiedDataSize[] = "Ash.BrowserDataMigrator.CopiedDataSizeMB";
constexpr char kLacrosDataSize[] = "Ash.BrowserDataMigrator.LacrosDataSizeMB";
constexpr char kCommonDataSize[] = "Ash.BrowserDataMigrator.CommonDataSizeMB";
constexpr char kTotalTime[] = "Ash.BrowserDataMigrator.TotalTimeTakenMS";
constexpr char kLacrosDataTime[] =
    "Ash.BrowserDataMigrator.LacrosDataTimeTakenMS";
constexpr char kCommonDataTime[] =
    "Ash.BrowserDataMigrator.CommonDataTimeTakenMS";
constexpr char kCreateDirectoryFail[] =
    "Ash.BrowserDataMigrator.CreateDirectoryFailure";
constexpr char kTotalCopySizeWhenNotEnoughSpace[] =
    "Ash.BrowserDataMigrator.TotalCopySizeWhenNotEnoughSpace";

using MigrationFinishedCallback =
    base::OnceCallback<void(BrowserDataMigratorImpl::MigrationResult)>;

class CopyMigrator : public BrowserDataMigratorImpl::MigratorDelegate {
 public:
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

  CopyMigrator(
      const base::FilePath& original_profile_dir,
      const std::string& user_id_hash,
      std::unique_ptr<MigrationProgressTracker> progress_tracker,
      scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag,
      MigrationFinishedCallback finished_callback);
  CopyMigrator(const CopyMigrator&) = delete;
  CopyMigrator& operator=(const CopyMigrator&) = delete;
  ~CopyMigrator() override;

  // BrowserDataMigratorImpl::MigratorDelegate override.
  void Migrate() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CopyMigratorTest, SetupTmpDir);
  FRIEND_TEST_ALL_PREFIXES(CopyMigratorTest, CancelSetupTmpDir);
  FRIEND_TEST_ALL_PREFIXES(CopyMigratorTest, MigrateInternal);
  FRIEND_TEST_ALL_PREFIXES(CopyMigratorTest, MigrateInternalOutOfDisk);

  // Handles the migration on a worker thread. Returns the end status of data
  // wipe and migration. `progress_callback` gets posted on UI thread whenever
  // an update to the UI is required
  static BrowserDataMigratorImpl::MigrationResult MigrateInternal(
      const base::FilePath& original_profile_dir,
      std::unique_ptr<MigrationProgressTracker> progress_tracker,
      scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag);

  // Set up the temporary directory `tmp_dir` by copying items into it.
  static bool SetupTmpDir(
      const browser_data_migrator_util::TargetItems& lacros_items,
      const browser_data_migrator_util::TargetItems& need_copy_items,
      const base::FilePath& tmp_dir,
      browser_data_migrator_util::CancelFlag* cancel_flag,
      MigrationProgressTracker* progress_tracker);

  // Path to the original profile data directory, which is directly under the
  // user data directory.
  const raw_ref<const base::FilePath, ExperimentalAsh> original_profile_dir_;
  // A hash string of the profile user ID.
  const std::string user_id_hash_;

  std::unique_ptr<MigrationProgressTracker> progress_tracker_;
  // `cancel_flag_` gets set by `Cancel()` and tasks posted to worker threads
  // can check if migration is cancelled or not.
  scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag_;
  // `finished_callback_` should be called once migration is completed/failed.
  MigrationFinishedCallback finished_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CROSAPI_COPY_MIGRATOR_H_
