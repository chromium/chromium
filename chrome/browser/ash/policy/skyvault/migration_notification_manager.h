// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_NOTIFICATION_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"

namespace policy::local_user_files {

constexpr char kSkyVaultMigrationNotificationId[] = "skyvault-migration";

// Shows notifications and dialogs related to SkyVault migration status.
class MigrationNotificationManager {
 public:
  explicit MigrationNotificationManager(Profile* profile);
  ~MigrationNotificationManager();

  // Shows a dialog informing the user that the migration will happen after
  // `migration_delay` (e.g. 24 h or 1 h). From the dialog, the user can select
  // to start the migration immediately which executes the `migration_callback`.
  void ShowMigrationInfoDialog(CloudProvider provider,
                               base::TimeDelta migration_delay,
                               base::OnceClosure migration_callback);

  // Shows the migration in progress notification.
  void ShowMigrationProgressNotification(CloudProvider provider);

  // Shows the migration completed successfully notification with a button to
  // open the folder specified by `destination_path`.
  void ShowMigrationCompletedNotification(
      CloudProvider provider,
      const base::FilePath& destination_path);

  // Shows the migration error notification.
  void ShowMigrationErrorNotification(CloudProvider provider,
                                      const std::string& message);

  // Closes any open notification or dialog.
  void CloseAll();

 private:
  // Profile for which this instance was created.
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<MigrationNotificationManager> weak_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_NOTIFICATION_MANAGER_H_
