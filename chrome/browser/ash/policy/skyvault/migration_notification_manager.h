// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_NOTIFICATION_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace policy::local_user_files {

// Shows notifications and dialogs related to SkyVault migration status.
class MigrationNotificationManager {
 public:
  MigrationNotificationManager();
  ~MigrationNotificationManager();

  // Shows the migration in progress notification.
  void ShowMigrationProgressNotification();

  // Shows the migration completed successfully notification with a button to
  // open the folder specified by `destination_path`.
  void ShowMigrationCompletedNotification(
      const base::FilePath& destination_path);

  // Shows the migration error notification.
  void ShowMigrationErrorNotification(const std::string& message);

 private:
  // Opens the location where the files are uploaded.
  void HandleCompletedNotificationClick(const base::FilePath& destination_path,
                                        std::optional<int> button_index);

  base::WeakPtrFactory<MigrationNotificationManager> weak_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_NOTIFICATION_MANAGER_H_
