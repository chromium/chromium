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
#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/files_cleanup_handler.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "components/keyed_service/core/keyed_service.h"

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

  // Creates an instance of LocalFilesMigrationManager with overridden
  // dependencies.
  static LocalFilesMigrationManager* CreateForTesting(
      content::BrowserContext* context,
      MigrationNotificationManager* notification_manager,
      std::unique_ptr<MigrationCoordinator> coordinator);

  // Creates an instance of LocalFilesMigrationManager.
  explicit LocalFilesMigrationManager(content::BrowserContext* context);
  LocalFilesMigrationManager(const LocalFilesMigrationManager&) = delete;
  LocalFilesMigrationManager& operator=(const LocalFilesMigrationManager&) =
      delete;
  ~LocalFilesMigrationManager() override;

  // Initializes this instance.
  void Initialize();

  // KeyedService overrides:
  void Shutdown() override;

  // Adds an observer to receive notifications about migration events.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

  // Injects a mock MigrationNotificationManager for tests.
  void SetNotificationManagerForTesting(
      MigrationNotificationManager* notification_manager);

  // Injects a mock MigrationCoordinator for tests.
  void SetCoordinatorForTesting(
      std::unique_ptr<MigrationCoordinator> coordinator);

 private:
  // policy::local_user_files::Observer overrides:
  void OnLocalUserFilesPolicyChanged() override;

  // Informs the user about the upcoming migration. Schedules another dialog to
  // appear closer to the start. From the dialog, the user can also choose to
  // start the migration immediately.
  void InformUser();

  // After initial delay, informs the user again and schedules the migration to
  // start automatically. From the dialog, the user can also choose to start the
  // migration immediately.
  void ScheduleMigrationAndInformUser();

  // Bypasses the migration delay and initiates the upload process immediately.
  // Called when the user clicks the "Upload now" button in the info dialog.
  void SkipMigrationDelay();

  // Called after the full migration timeout elapses. Closes the dialog if
  // opened, and starts migration.
  void OnTimeoutExpired();

  // Gathers all file paths that need to be uploaded.
  void GetPathsToUpload();

  // Starts the migration process by uploading `files` to `cloud_provider_`.
  void StartMigration(std::vector<base::FilePath> files);

  // Handles the completion of the migration process (success or failure).
  // If the migration was successful, starts the cleanup process, and handles
  // the errors otherwise.
  void OnMigrationDone(std::map<base::FilePath, MigrationUploadError> errors);

  // Completes the migration process, taking into account any errors that
  // occurred during the migration.
  void ProcessErrors(std::map<base::FilePath, MigrationUploadError> errors);

  // Cleans up any remaining files from the device after a successful migration.
  void CleanupLocalFiles();

  // Handles the completion of the local files cleanup process.
  void OnCleanupDone(
      std::unique_ptr<chromeos::FilesCleanupHandler> cleanup_handler,
      const std::optional<std::string>& error_message);

  // Sends a D-Bus call to enable or disable write access to MyFiles.
  void SetLocalUserFilesWriteEnabled(bool enabled);

  // Handles the response of the SetUserDataStorageWriteEnabled D-Bus call.
  void OnFilesWriteRestricted(
      std::optional<user_data_auth::SetUserDataStorageWriteEnabledReply> reply);

  // Stops the migration if currently ongoing.
  void MaybeStopMigration(CloudProvider previous_provider);

  // Sets and stores the state on the device.
  void SetState(State new_state);

  // Observers for migration events.
  base::ObserverList<Observer>::Unchecked observers_;

  // Indicates the migration state.
  State state_ = State::kUninitialized;

  // Indicates if local files cleanup is currently running.
  bool cleanup_in_progress_ = false;

  // Whether local user files are allowed by policy.
  bool local_user_files_allowed_ = true;

  // Cloud provider to which files are uploaded. If not specified, no migration
  // happens.
  CloudProvider cloud_provider_ = CloudProvider::kNotSpecified;

  // The time at which the migration will start automatically.
  base::Time migration_start_time_;

  // Context for which this instance is created.
  raw_ptr<content::BrowserContext> context_;

  // Shows and manages migration notifications and dialogs.
  raw_ptr<MigrationNotificationManager> notification_manager_;

  // Manages the upload of local files to the cloud.
  std::unique_ptr<MigrationCoordinator> coordinator_;

  // Timer for delaying the start of migration and showing dialogs.
  std::unique_ptr<base::WallClockTimer> scheduling_timer_;

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
  // BrowserContext. If `create` is true, an instance is created if it doesn't
  // exist.
  static LocalFilesMigrationManager* GetForBrowserContext(
      content::BrowserContext* context,
      bool create = true);

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
