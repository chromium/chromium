// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_NOTIFICATION_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace policy::local_user_files {

constexpr char kSkyVaultMigrationNotificationId[] = "skyvault-migration";

// Shows notifications and dialogs related to SkyVault migration status.
class MigrationNotificationManager : public KeyedService {
 public:
  using SignInCallback = base::OnceCallback<void(base::File::Error)>;
  using SignInCallbacks = base::OnceCallbackList<void(base::File::Error)>;

  explicit MigrationNotificationManager(content::BrowserContext* context);
  ~MigrationNotificationManager() override;

  // Shows a dialog informing the user that the migration will happen at
  // `migration_start_time`, e.g. 24 h or 1 h from now. From the dialog, the
  // user can select to start the migration immediately which executes the
  // `migration_callback`.
  // Virtual to override in tests.
  virtual void ShowMigrationInfoDialog(CloudProvider provider,
                                       base::Time migration_start_time,
                                       base::OnceClosure migration_callback);

  // Shows the migration in progress notification.
  void ShowMigrationProgressNotification(CloudProvider provider);

  // Shows the migration completed successfully notification with a button to
  // open the folder specified by `destination_path`.
  void ShowMigrationCompletedNotification(
      CloudProvider provider,
      const base::FilePath& destination_path);

  // Shows a notification that migration completed with errors.
  void ShowMigrationErrorNotification(
      CloudProvider provider,
      const base::FilePath& destination_path,
      std::map<base::FilePath, MigrationUploadError> errors);

  // Shows the policy configuration error notification.
  // Virtual to override in tests.
  virtual void ShowConfigurationErrorNotification(CloudProvider provider);

  // Displays a single notification prompting the user to sign in to OneDrive.
  // Queues any subsequent sign-in requests until the user responds which
  // executes all queued callbacks with the result of the sign-in process.
  base::CallbackListSubscription ShowOneDriveSignInNotification(
      SignInCallback callback);

  // Closes any open notification or dialog.
  void CloseAll();

  // Closes the migration dialog. No-op if dialog isn't opened.
  void CloseDialog();

 private:
  Profile* profile();

  // Callback invoked when the user responds to the OneDrive sign-in
  // notification.
  // Shows a progress notification if the setup and sign in completed
  // successfully (base::File::Error::FILE_ERROR_OK).
  void OnSignInResponse(base::File::Error error);

  // Context for which this instance was created.
  raw_ptr<content::BrowserContext> context_;

  // List of sign-in result callbacks, mapped to a single sign-in notification.
  // This ensures that all callers who requested the sign-in receive the final
  // result (success or error) once the notification is dismissed.
  SignInCallbacks sign_in_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MigrationNotificationManager> weak_factory_{this};
};

// Manages all MigrationNotificationManager instances and associates them with
// Profiles.
class MigrationNotificationManagerFactory : public ProfileKeyedServiceFactory {
 public:
  MigrationNotificationManagerFactory(
      const MigrationNotificationManagerFactory&) = delete;
  MigrationNotificationManagerFactory& operator=(
      const MigrationNotificationManagerFactory&) = delete;

  // Gets the singleton instance of the factory.
  static MigrationNotificationManagerFactory* GetInstance();

  // Gets the LocalFilesMigrationManager instance associated with the given
  // BrowserContext.
  static MigrationNotificationManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<MigrationNotificationManagerFactory>;

  MigrationNotificationManagerFactory();
  ~MigrationNotificationManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsNULLWhileTesting() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_NOTIFICATION_MANAGER_H_
