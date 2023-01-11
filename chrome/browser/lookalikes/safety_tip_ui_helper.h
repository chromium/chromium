// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_UI_HELPER_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_UI_HELPER_H_

#include <string>

#include "chrome/browser/lookalikes/safety_tip_ui.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Navigates to either |safe_url| (when !is_empty()), or a safe default
// otherwise, replacing the current page in the process. Invoked when 'leave
// site' button is clicked.
void LeaveSiteFromSafetyTip(content::WebContents* web_contents,
                            const GURL& safe_url);

// Opens the Help Center URL when 'Learn more' button is clicked.
void OpenHelpCenterFromSafetyTip(content::WebContents* web_contents);

// Get the titles, descriptions, and button strings or IDs needed to describe
// the applicable warning type.  Handles both Android and desktop warnings.
// |suggested_url| is the suggested URL to navigate to, used in some strings.
std::u16string GetSafetyTipTitle(security_state::SafetyTipStatus warning_type,
                                 const GURL& suggested_url);
std::u16string GetSafetyTipDescription(
    security_state::SafetyTipStatus warning_type,
    const GURL& suggested_url);
int GetSafetyTipLeaveButtonId(security_state::SafetyTipStatus warning_type);

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_UI_HELPER_H_
