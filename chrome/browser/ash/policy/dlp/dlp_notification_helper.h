// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_NOTIFICATION_HELPER_H_

#include <string>

#include "base/callback_forward.h"

namespace policy {

// Shows a notification that printing is not allowed due to DLP rules.
void ShowDlpPrintDisabledNotification();

// Shows a warning dialog that printing is not recommended and allows the user
// to choose whether to continue or not. Based on the response, only one of
// |continue_cb| and |cancel_cb| will run.
void ShowDlpPrintWarningDialog(base::OnceClosure continue_cb,
                               base::OnceClosure cancel_cb);

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

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_NOTIFICATION_HELPER_H_
