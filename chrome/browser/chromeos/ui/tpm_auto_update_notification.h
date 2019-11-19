// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_TPM_AUTO_UPDATE_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_UI_TPM_AUTO_UPDATE_NOTIFICATION_H_

namespace chromeos {

// For the TPM firmware auto-update flow with user acknowledgment users will be
// shown two notifications, one informing that an auto-update will be performed
// at the first reboot after 24 hours and one displayed after at least 24 hours
// informing that an update will happen at the next reboot.
enum class TpmAutoUpdateUserNotification {
  kNone = 0,
  // Corresponds to the notification informing the user that an update that will
  // clear user data will be performed at the first reboot after 24 hours.
  kPlanned = 1,
  // Corresponds to the notification informing the user that an update that will
  // clear user data will be performed after the next reboot.
  kOnNextReboot = 2
};

void ShowAutoUpdateNotification(
    TpmAutoUpdateUserNotification notification_type);

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_UI_TPM_AUTO_UPDATE_NOTIFICATION_H_
