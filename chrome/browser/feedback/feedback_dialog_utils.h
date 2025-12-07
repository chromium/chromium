// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_DIALOG_UTILS_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_DIALOG_UTILS_H_

#include <string>

class BrowserWindowInterface;
class GURL;
class Profile;

// Utility functions for the feedback dialog.
namespace chrome {

// List of possible WebUI sources that could request to open the feedback
// dialog. Additional sources can be added over time.
enum class WebUIFeedbackSource { kConnectivityDiagnostics };

// Get the URL of the given tab, if it still exists.
GURL GetTargetTabUrl(BrowserWindowInterface* bwi, int index);

// Get the profile that should be used to open the feedback dialog.
Profile* GetFeedbackProfile(BrowserWindowInterface* bwi);

// Show the feedback dialog from WebUI.
void ShowFeedbackDialogForWebUI(WebUIFeedbackSource source,
                                const std::string& extra_diagnostics);

}  // namespace chrome

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_DIALOG_UTILS_H_
