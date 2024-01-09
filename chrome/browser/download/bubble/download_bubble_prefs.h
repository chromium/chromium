// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PREFS_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PREFS_H_

#include "chrome/browser/profiles/profile.h"

namespace download {

// Called when deciding whether to show the bubble or the old download shelf UI.
bool IsDownloadBubbleEnabled();

// Called when deciding whether to show or hide the bubble.
bool ShouldShowDownloadBubble(Profile* profile);

// The enterprise download connectors can be enabled in blocking or nonblocking
// mode. This returns false if the connector is disabled.
bool DoesDownloadConnectorBlock(Profile* profile, const GURL& url);

// Whether the partial view is controlled by prefs. If not controlled by prefs,
// the partial view defaults to disabled.
bool IsDownloadBubblePartialViewControlledByPref();

// Whether the partial view should be shown automatically when downloads are
// finished.
bool IsDownloadBubblePartialViewEnabled(Profile* profile);

// Set the pref governing whether the partial view should be shown automatically
// when downloads are finished. Note that on Lacros, the pref may be ignored
// if the SysUI integration is enabled.
void SetDownloadBubblePartialViewEnabled(Profile* profile, bool enabled);

// Whether the setting is controlled by pref and is the default value (not set
// by the user).
bool IsDownloadBubblePartialViewEnabledDefaultPrefValue(Profile* profile);

// The number of partial view impressions.
int DownloadBubblePartialViewImpressions(Profile* profile);
void SetDownloadBubblePartialViewImpressions(Profile* profile, int count);

}  // namespace download

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PREFS_H_
