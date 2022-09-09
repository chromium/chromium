// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_DIALOG_UTILS_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_DIALOG_UTILS_H_

#include "components/sessions/core/session_id.h"

class Browser;
class GURL;
class Profile;

// Utility functions for the feedback dialog.
namespace chrome {

// List of possible WebUI sources that could request to open the feedback
// dialog. Additional sources can be added over time.
enum class WebUIFeedbackSource { kConnectivityDiagnostics };

// Get the GURL of the active tab when the feedback dialog was invoked, if
// any.
GURL GetTargetTabUrl(SessionID session_id, int index);

// Get the profile that should be used to open the feedback dialog.
Profile* GetFeedbackProfile(const Browser* browser);

// Show the feedback dialog from WebUI.
void ShowFeedbackDialogForWebUI(WebUIFeedbackSource source,
                                const std::string& extra_diagnostics);

}  // namespace chrome

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_DIALOG_UTILS_H_
