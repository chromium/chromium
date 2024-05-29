// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_MIGRATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_MIGRATION_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"

namespace policy::local_user_files {

// Manages the migration of local files to the cloud when SkyVault is enabled.
// Handles starting, monitoring, and completing the migration process.
class LocalFilesMigrationManager : public LocalUserFilesPolicyObserver {
 public:
  class Observer {
   public:
    // Called when the migration of files to the cloud has completed
    // successfully.
    virtual void OnMigrationSucceeded() = 0;
  };

  LocalFilesMigrationManager();
  ~LocalFilesMigrationManager() override;

  // Adds an observer to receive notifications about migration events.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

 private:
  // policy::local_user_files::Observer overrides:
  void OnLocalUserFilesPolicyChanged() override;

  // Determines if the migration should start based on the following conditions:
  //    * Migration is not already in progress.
  //    * SkyVault policies are set.
  bool ShouldStart();

  // Initiates the file migration to the cloud if conditions are met.
  void MaybeMigrateFiles(base::OnceCallback<void()> callback);

  // Handles the completion of the migration process (success or failure).
  void OnMigrationDone();

  std::unique_ptr<MigrationNotificationManager> notification_manager_;
  base::ObserverList<Observer>::Unchecked observers_;
  bool in_progress_ = false;  // Indicates if migration is currently running.
  std::optional<std::string> error_;  // Stores migration error, if any.

  base::WeakPtrFactory<LocalFilesMigrationManager> weak_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_MIGRATION_MANAGER_H_
