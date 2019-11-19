// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_SAFETY_TIP_UI_HELPER_H_
#define CHROME_BROWSER_REPUTATION_SAFETY_TIP_UI_HELPER_H_

#include "base/strings/string16.h"
#include "chrome/browser/reputation/safety_tip_ui.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// URL that the "leave site" button aborts to by default.
extern const char kSafetyTipLeaveSiteUrl[];

// Records a histogram for a user's interaction with a Safety Tip in the given
// |web_contents|.
void RecordSafetyTipInteractionHistogram(content::WebContents* web_contents,
                                         SafetyTipInteraction interaction);

// Invokes action when 'leave site' button is clicked, and records a histogram.
// Navigates to a safe URL, replacing the current page in the process.
void LeaveSiteFromSafetyTip(content::WebContents* web_contents,
                            const GURL& safe_url);

// Invoke action when 'Learn more' button is clicked, and records a histogram.
// Navigates to the help center URL.
void OpenHelpCenterFromSafetyTip(content::WebContents* web_contents);

// Get the titles, descriptions, and button strings or IDs needed to describe
// the applicable warning type.  Handles both Android and desktop warnings.
// |url| is the URL of the current page. |suggested_url| is the suggested URL to
// navigate to. Both URLs are used in formatting some strings.
base::string16 GetSafetyTipTitle(security_state::SafetyTipStatus warning_type,
                                 const GURL& suggested_url);
base::string16 GetSafetyTipDescription(
    security_state::SafetyTipStatus warning_type,
    const GURL& url,
    const GURL& suggested_url);
int GetSafetyTipLeaveButtonId(security_state::SafetyTipStatus warning_type);

#endif  // CHROME_BROWSER_REPUTATION_SAFETY_TIP_UI_HELPER_H_
