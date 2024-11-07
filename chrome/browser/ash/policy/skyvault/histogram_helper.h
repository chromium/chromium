// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_HISTOGRAM_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_HISTOGRAM_HELPER_H_

#include "chrome/browser/ash/policy/skyvault/policy_utils.h"

namespace policy::local_user_files {

// Helper functions to log UMA stats related to the file upload flow.
void SkyVaultDeleteErrorHistogram(UploadTrigger trigger,
                                  CloudProvider provider,
                                  bool value);
void SkyVaultOneDriveSignInErrorHistogram(UploadTrigger trigger, bool value);

// Helper functions to log UMA stats related to local storage settings.
void SkyVaultLocalStorageEnabledHistogram(bool value);
void SkyVaultLocalStorageMisconfiguredHistogram(bool value);

// Helper functions to log UMA stats specific for the migration flow.
void SkyVaultMigrationEnabledHistogram(CloudProvider provider, bool value);
void SkyVaultMigrationMisconfiguredHistogram(CloudProvider provider,
                                             bool value);
void SkyVaultMigrationResetHistogram(bool value);
void SkyVaultMigrationRetryHistogram(int count);
void SkyVaultMigrationStoppedHistogram(CloudProvider provider, bool value);
void SkyVaultMigrationWrongStateHistogram(CloudProvider provider,
                                          StateErrorContext context,
                                          State state);
void SkyVaultMigrationFailedHistogram(CloudProvider provider, bool value);
void SkyVaultMigrationWriteAccessErrorHistogram(bool value);
void SkyVaultMigrationUploadErrorHistogram(CloudProvider provider,
                                           MigrationUploadError error);

// Helper functions to log UMA stats on migration dialog interactions.
void SkyVaultMigrationDialogActionHistogram(CloudProvider provider,
                                            DialogAction action);
void SkyVaultMigrationDialogShownHistogram(CloudProvider provider, bool value);

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_HISTOGRAM_HELPER_H_
