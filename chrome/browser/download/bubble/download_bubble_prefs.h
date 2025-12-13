// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PREFS_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PREFS_H_

#include "chrome/browser/profiles/profile.h"

namespace download {

// Called when deciding whether to show or hide the bubble.
bool ShouldShowDownloadBubble(Profile* profile);

// Whether the partial view should be shown automatically when downloads are
// finished.
bool IsDownloadBubblePartialViewEnabled(Profile* profile);

// Set the pref governing whether the partial view should be shown automatically
// when downloads are finished.
void SetDownloadBubblePartialViewEnabled(Profile* profile, bool enabled);

// Whether the setting is controlled by pref and is the default value (not set
// by the user).
bool IsDownloadBubblePartialViewEnabledDefaultPrefValue(Profile* profile);

// The number of partial view impressions.
int DownloadBubblePartialViewImpressions(Profile* profile);
void SetDownloadBubblePartialViewImpressions(Profile* profile, int count);

}  // namespace download

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PREFS_H_
