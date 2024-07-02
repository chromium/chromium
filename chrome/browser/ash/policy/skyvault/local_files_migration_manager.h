// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_MIGRATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_MIGRATION_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/policy/skyvault/local_user_files_policy_observer.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace policy::local_user_files {

// Manages the migration of local files to the cloud when SkyVault is enabled.
// Handles starting, monitoring, and completing the migration process.
class LocalFilesMigrationManager : public LocalUserFilesPolicyObserver,
                                   public KeyedService {
 public:
  class Observer {
   public:
    // Called when the migration of files to the cloud has completed
    // successfully.
    virtual void OnMigrationSucceeded() = 0;
  };

  explicit LocalFilesMigrationManager(content::BrowserContext* context);
  LocalFilesMigrationManager(const LocalFilesMigrationManager&) = delete;
  LocalFilesMigrationManager& operator=(const LocalFilesMigrationManager&) =
      delete;
  ~LocalFilesMigrationManager() override;

  // KeyedService overrides:
  void Shutdown() override;

  // Adds an observer to receive notifications about migration events.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

 private:
  // policy::local_user_files::Observer overrides:
  void OnLocalUserFilesPolicyChanged() override;

  // If conditions are met, schedules the file migration to the cloud in 24
  // hours. The migration starts automatically after the delay elapses. If the
  // user chooses to skip the delay (via the info dialog), the migration starts
  // immediately.
  void InitiateMigration();

  // Bypasses the migration delay and initiates the upload process immediately.
  // Called when the user clicks the "Upload now" button in the info dialog.
  void SkipMigrationDelay();

  // Starts the migration process.
  void StartMigration();

  // Handles the completion of the migration process (success or failure).
  void OnMigrationDone();

  // Stops the migration if currently ongoing.
  void MaybeStopMigration();

  // Observers for migration events.
  base::ObserverList<Observer>::Unchecked observers_;

  // Indicates if migration is currently running.
  bool in_progress_ = false;

  // Whether local user files are allowed by policy.
  bool local_user_files_allowed_ = true;

  // Cloud provider to which files are uploaded. If not specified, no migration
  // happens.
  CloudProvider cloud_provider_;

  // Stores any error that occurred during migration.
  std::optional<std::string> error_;

  // Context for which this instance is created.
  raw_ptr<content::BrowserContext> context_;

  // Shows and manages migration notifications and dialogs.
  std::unique_ptr<MigrationNotificationManager> notification_manager_;

  // Timer for delaying the start of migration.
  std::unique_ptr<base::WallClockTimer> start_delay_timer_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<LocalFilesMigrationManager> weak_factory_{this};
};

// Manages all LocalFilesMigrationManager instances and associates them with
// Profiles.
class LocalFilesMigrationManagerFactory : public ProfileKeyedServiceFactory {
 public:
  LocalFilesMigrationManagerFactory(const LocalFilesMigrationManagerFactory&) =
      delete;
  LocalFilesMigrationManagerFactory& operator=(
      const LocalFilesMigrationManagerFactory&) = delete;

  // Gets the singleton instance of the factory.
  static LocalFilesMigrationManagerFactory* GetInstance();

  // Gets the LocalFilesMigrationManager instance associated with the given
  // BrowserContext.
  static LocalFilesMigrationManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<LocalFilesMigrationManagerFactory>;

  LocalFilesMigrationManagerFactory();
  ~LocalFilesMigrationManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsNULLWhileTesting() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_LOCAL_FILES_MIGRATION_MANAGER_H_
