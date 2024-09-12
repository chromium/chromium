// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TPM_TPM_FIRMWARE_UPDATE_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_TPM_TPM_FIRMWARE_UPDATE_NOTIFICATION_H_

class Profile;

namespace ash {
namespace tpm_firmware_update {

// Displays a message that informs the user about a pending TPM Firmware Update,
// direction the user to the about page to trigger the update and allowing the
// notification to be silenced.
void ShowNotificationIfNeeded(Profile* profile);

}  // namespace tpm_firmware_update
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TPM_TPM_FIRMWARE_UPDATE_NOTIFICATION_H_
