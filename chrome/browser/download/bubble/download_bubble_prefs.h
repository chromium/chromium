// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PREFS_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PREFS_H_

#include "chrome/browser/profiles/profile.h"

namespace download {

// Called when deciding whether to show the bubble or the old download shelf UI.
bool IsDownloadBubbleEnabled(Profile* profile);

// V2 is only eligible to be enabled if V1 is also enabled.
bool IsDownloadBubbleV2Enabled(Profile* profile);

// Called when deciding whether to show or hide the bubble.
bool ShouldShowDownloadBubble(Profile* profile);

bool IsDownloadConnectorEnabled(Profile* profile);

// Whether to suppress the download bubble IPH. This will be true for users
// who have interacted with the download bubble prior to the IPH being added.
bool ShouldSuppressDownloadBubbleIph(Profile* profile);
// TODO(crbug.com/1420750): This setter is temporary and should be removed when
// the IPH is implemented.
void SetShouldSuppressDownloadBubbleIph(Profile* profile, bool suppress);

}  // namespace download

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PREFS_H_
