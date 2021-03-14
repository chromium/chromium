// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_NOTIFICATION_HELPER_H_

#include <string>


namespace policy {

// Shows a notification that printing is not allowed due to DLP rules.
void ShowDlpPrintDisabledNotification();

// Shows/hides a notification that screen capture was paused because
// confidential content appeared in the captured area, or resumed when it left
// the captured area.
// Different captures are identified by |capture_id| and separated notifications
// are shown for them. |app_title| is provided for a customized message.
void HideDlpScreenCapturePausedNotification(const std::string& capture_id);
void ShowDlpScreenCapturePausedNotification(const std::string& capture_id,
                                            const std::u16string& app_title);
void HideDlpScreenCaptureResumedNotification(const std::string& capture_id);
void ShowDlpScreenCaptureResumedNotification(const std::string& capture_id,
                                             const std::u16string& app_title);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_NOTIFICATION_HELPER_H_
