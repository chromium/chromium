// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_HISTOGRAM_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_HISTOGRAM_HELPER_H_

#include "base/time/time.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"

namespace policy::local_user_files {

// Helper functions to log UMA stats related to the file upload flow.
void SkyVaultDeleteErrorHistogram(UploadTrigger trigger,
                                  MigrationDestination destination,
                                  bool value);
void SkyVaultOneDriveSignInErrorHistogram(UploadTrigger trigger, bool value);

// Helper functions to log UMA stats related to local storage settings.
void SkyVaultLocalStorageEnabledHistogram(bool value);
void SkyVaultLocalStorageMisconfiguredHistogram(bool value);

// Helper functions to log UMA stats specific for the migration flow.
void SkyVaultMigrationEnabledHistogram(MigrationDestination destination,
                                       bool value);
void SkyVaultMigrationMisconfiguredHistogram(MigrationDestination destination,
                                             bool value);
void SkyVaultMigrationResetHistogram(bool value);
void SkyVaultMigrationRetryHistogram(int count);
void SkyVaultDeletionRetryHistogram(int count);
void SkyVaultMigrationStoppedHistogram(MigrationDestination destination,
                                       bool value);
void SkyVaultMigrationWrongStateHistogram(MigrationDestination destination,
                                          StateErrorContext context,
                                          State state);
void SkyVaultDeletionDoneHistogram(bool success);
void SkyVaultMigrationDoneHistograms(MigrationDestination destination,
                                     bool success,
                                     base::TimeDelta duration);
void SkyVaultMigrationWriteAccessErrorHistogram(bool value);
void SkyVaultMigrationUploadErrorHistogram(MigrationDestination destination,
                                           MigrationUploadError error);
void SkyVaultMigrationWaitForConnectionHistogram(
    MigrationDestination destination,
    bool waiting_for_connection);
void SkyVaultMigrationReconnectionDurationHistogram(
    MigrationDestination destination,
    base::TimeDelta duration);
void SkyVaultMigrationCleanupErrorHistogram(MigrationDestination destination,
                                            bool value);
void SkyVaultMigrationScheduledTimeInPastInformUser(
    MigrationDestination destination,
    bool value);
void SkyVaultMigrationScheduledTimeInPastScheduleMigration(
    MigrationDestination destination,
    bool value);

// Helper functions to log UMA stats on migration dialog interactions.
void SkyVaultMigrationDialogActionHistogram(MigrationDestination destination,
                                            DialogAction action);
void SkyVaultMigrationDialogShownHistogram(MigrationDestination destination,
                                           bool value);

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_HISTOGRAM_HELPER_H_
