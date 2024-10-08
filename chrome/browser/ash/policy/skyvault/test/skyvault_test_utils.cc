// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/test/skyvault_test_utils.h"

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
                            const std::string& destination_dir,
                            MigrationDoneCallback callback) {
        is_running_ = true;
        if (run_cb_) {
          std::move(run_cb_).Run();
          return;
        }
        // Simulate upload lasting a while.
        base::SequencedTaskRunner::GetCurrentDefault()
            ->GetCurrentDefault()
            ->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(
                    &MockMigrationCoordinator::OnMigrationDone,
                    weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                    std::map<base::FilePath, MigrationUploadError>()),
                base::Minutes(5));  // Delay 5 minutes
      });

  ON_CALL(*this, Stop).WillByDefault([this]() { is_running_ = false; });
}

MockMigrationCoordinator::~MockMigrationCoordinator() = default;

void MockMigrationCoordinator::OnMigrationDone(
    MigrationDoneCallback callback,
    std::map<base::FilePath, MigrationUploadError> errors) {
  if (is_running_) {
    std::move(callback).Run(std::move(errors));
    is_running_ = false;
  }
}

void MockMigrationCoordinator::SetRunCallback(base::OnceClosure run_cb) {
  CHECK(run_cb);
  run_cb_ = std::move(run_cb);
}

}  // namespace policy::local_user_files
