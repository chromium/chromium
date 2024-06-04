// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_MIGRATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_MIGRATION_MANAGER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "components/prefs/pref_change_registrar.h"

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
  void MaybeMigrateFiles(base::OnceClosure callback);

  void StartMigration(base::OnceClosure callback);

  // Handles the completion of the migration process (success or failure).
  void OnMigrationDone();

  // Observers for migration events.
  base::ObserverList<Observer>::Unchecked observers_;

  // Indicates if migration is currently running.
  bool in_progress_ = false;

  // Whether local user files are allowed by policy.
  bool local_user_files_allowed_ = true;

  // Whether migration is enabled by policy.
  bool local_user_files_migration_enabled_ = false;

  // Stores any error that occurred during migration.
  std::optional<std::string> error_;

  // Shows and manages migration notifications and dialogs.
  std::unique_ptr<MigrationNotificationManager> notification_manager_;

  // Timer for delaying the start of migration.
  std::unique_ptr<base::WallClockTimer> start_delay_timer_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<LocalFilesMigrationManager> weak_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_MIGRATION_MANAGER_H_
