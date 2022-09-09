// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"

#include <errno.h>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace ash {

BrowserDataBackMigrator::BrowserDataBackMigrator(
    const base::FilePath& ash_profile_dir)
    : ash_profile_dir_(ash_profile_dir) {}

BrowserDataBackMigrator::~BrowserDataBackMigrator() = default;

void BrowserDataBackMigrator::Migrate(
    BackMigrationFinishedCallback finished_callback) {
  LOG(WARNING) << "BrowserDataBackMigrator::Migrate() is called.";

  DCHECK(base::FeatureList::IsEnabled(
      ash::features::kLacrosProfileBackwardMigration));

  const base::FilePath lacros_profile_dir =
      ash_profile_dir_.Append(browser_data_migrator_util::kLacrosDir);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::PreMigrationCleanUp,
                     ash_profile_dir_, lacros_profile_dir),
      base::BindOnce(&BrowserDataBackMigrator::OnPreMigrationCleanUp,
                     weak_factory_.GetWeakPtr(), std::move(finished_callback)));
}

// static
BrowserDataBackMigrator::TaskResult
BrowserDataBackMigrator::PreMigrationCleanUp(
    const base::FilePath& ash_profile_dir,
    const base::FilePath& lacros_profile_dir) {
  LOG(WARNING) << "Running PreMigrationCleanUp()";

  const base::FilePath tmp_user_dir =
      ash_profile_dir.Append(browser_data_back_migrator::kTmpDir);
  if (base::PathExists(tmp_user_dir)) {
    // Delete tmp_user_dir if any were left from a previous failed back
    // migration attempt.
    if (!base::DeletePathRecursively(tmp_user_dir)) {
      PLOG(ERROR) << "Deleting " << tmp_user_dir.value() << " failed: ";
      return {TaskStatus::kPreMigrationCleanUpDeleteTmpDirFailed, errno};
    }
  }

  // Delete ash deletable items to free up space.
  browser_data_migrator_util::TargetItems ash_deletable_items =
      browser_data_migrator_util::GetTargetItems(
          ash_profile_dir, browser_data_migrator_util::ItemType::kDeletable);
  for (const auto& item : ash_deletable_items.items) {
    bool result = item.is_directory ? base::DeletePathRecursively(item.path)
                                    : base::DeleteFile(item.path);
    if (!result) {
      // This is not critical to the migration so log the error but do stop the
      // migration.
      PLOG(ERROR) << "Could not delete " << item.path.value();
    }
  }

  // Delete ash deletable items to free up space.
  browser_data_migrator_util::TargetItems lacros_deletable_items =
      browser_data_migrator_util::GetTargetItems(
          lacros_profile_dir, browser_data_migrator_util::ItemType::kDeletable);
  for (const auto& item : lacros_deletable_items.items) {
    bool result = item.is_directory ? base::DeletePathRecursively(item.path)
                                    : base::DeleteFile(item.path);
    if (!result) {
      // This is not critical to the migration so log the error but do stop the
      // migration.
      PLOG(ERROR) << "Could not delete " << item.path.value();
    }
  }

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnPreMigrationCleanUp(
    BackMigrationFinishedCallback finished_callback,
    BrowserDataBackMigrator::TaskResult result) {
  LOG(WARNING) << "Backward migration completed successfully.";
  // TODO(b/244573664): Continue the migration
}

}  // namespace ash
