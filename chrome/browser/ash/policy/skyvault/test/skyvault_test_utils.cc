// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/test/skyvault_test_utils.h"

#include "base/files/file_path.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_manager.h"
#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"

namespace policy::local_user_files {

MockMigrationObserver::MockMigrationObserver() = default;
MockMigrationObserver::~MockMigrationObserver() = default;

MockMigrationNotificationManager::MockMigrationNotificationManager(
    content::BrowserContext* context)
    : MigrationNotificationManager(context) {}
MockMigrationNotificationManager::~MockMigrationNotificationManager() = default;

MockMigrationCoordinator::MockMigrationCoordinator(Profile* profile)
    : MigrationCoordinator(profile) {
  ON_CALL(*this, Run)
      .WillByDefault([this](CloudProvider cloud_provider,
                            std::vector<base::FilePath> file_paths,
                            const std::string& upload_root,
                            MigrationDoneCallback callback) {
        is_running_ = true;
        if (run_cb_) {
          run_cb_.Run();
          return;
        }
        // Simulate upload lasting a while.
        base::SequencedTaskRunner::GetCurrentDefault()
            ->GetCurrentDefault()
            ->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(&MockMigrationCoordinator::OnMigrationDone,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback),
                               std::map<base::FilePath, MigrationUploadError>(),
                               base::FilePath(), base::FilePath()),
                base::Minutes(5));  // Delay 5 minutes
      });

  ON_CALL(*this, Cancel)
      .WillByDefault([this](MigrationStoppedCallback callback) {
        is_running_ = false;
        std::move(callback).Run(true);
      });
}

MockMigrationCoordinator::~MockMigrationCoordinator() = default;

void MockMigrationCoordinator::OnMigrationDone(
    MigrationDoneCallback callback,
    std::map<base::FilePath, MigrationUploadError> errors,
    base::FilePath upload_root_path,
    base::FilePath error_log_path) {
  if (is_running_) {
    std::move(callback).Run(std::move(errors), upload_root_path,
                            error_log_path);
    is_running_ = false;
  }
}

void MockMigrationCoordinator::SetRunCallback(base::RepeatingClosure run_cb) {
  CHECK(run_cb);
  run_cb_ = std::move(run_cb);
}

}  // namespace policy::local_user_files
