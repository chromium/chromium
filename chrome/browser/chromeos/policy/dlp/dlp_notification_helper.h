// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_NOTIFICATION_HELPER_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace policy {

// Shows a notification that printing is not allowed due to DLP rules.
void ShowDlpPrintDisabledNotification();

// Shows a notification that screen share is not allowed due to DLP rules.
void ShowDlpScreenShareDisabledNotification(const std::u16string& app_title);

// Shows/hides a notification that screen share was paused because confidential
// content appeared in the captured area, or resumed when it left the captured
// area. Different screen shares are identified by |share_id| and separated
// notifications are shown for them. |app_title| is provided for a customized
// message.
void HideDlpScreenSharePausedNotification(const std::string& share_id);
void ShowDlpScreenSharePausedNotification(const std::string& share_id,
                                          const std::u16string& app_title);
void HideDlpScreenShareResumedNotification(const std::string& share_id);
void ShowDlpScreenShareResumedNotification(const std::string& share_id,
                                           const std::u16string& app_title);

// Shows a notification that screen capture is not allowed due to DLP rules.
void ShowDlpScreenCaptureDisabledNotification();

// Shows a notification that video capture is stopped due to DLP rules.
void ShowDlpVideoCaptureStoppedNotification();

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_NOTIFICATION_HELPER_H_
