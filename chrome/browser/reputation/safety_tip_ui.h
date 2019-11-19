// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_SAFETY_TIP_UI_H_
#define CHROME_BROWSER_REPUTATION_SAFETY_TIP_UI_H_

#include "build/build_config.h"
#include "components/security_state/core/security_state.h"

namespace content {
class WebContents;
}

class GURL;

// Represents the different user interactions with a Safety Tip dialog.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with SafetyTipStatus.
enum class SafetyTipInteraction {
  // The user dismissed the safety tip. Every time the user dismisses the
  // dialog, a histogram will be recorded once with this value, and again with a
  // more specific way of dismissing the safety tip (e.g. kDismissWithEsc).
  kDismiss = 0,
  // The user followed the safety tip's call to action to leave the site.
  kLeaveSite = 1,
  kNoAction = 2,
  // The values below record specific ways that the user dismissed the safety
  // tip.
  kDismissWithEsc = 3,
  kDismissWithClose = 4,
  kDismissWithIgnore = 5,
  // The user clicked the 'learn more' button.
  kLearnMore = 6,
  kMaxValue = kLearnMore,
};

// Shows Safety Tip UI using the specified information if it is not already
// showing. |virtual_url| is the virtual url of the page/frame the info applies
// to. |suggested_url| is the URL that Chrome thinks the user may have wanted to
// navigate to (if applicable). |close_callback| will be called when the dialog
// is closed; the argument indicates the action that the user took (if any) to
// close the dialog. Implemented in platform-specific files.
void ShowSafetyTipDialog(
    content::WebContents* web_contents,
    security_state::SafetyTipStatus type,
    const GURL& virtual_url,
    const GURL& suggested_url,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback);

#endif  // CHROME_BROWSER_REPUTATION_SAFETY_TIP_UI_H_
