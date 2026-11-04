// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_BAR_UTILS_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_BAR_UTILS_H_

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace chrome {

// Toggles whether the bookmark bar is shown only on the new tab page or on
// all tabs. This is a preference modifier, not a visual modifier.
void ToggleBookmarkBarWhenVisible(content::BrowserContext* browser_context);

// Called upon direct user interaction with the bookmarks bar. If the user is in
// the NTP Simplification transition period (i.e. the visibility state is at its
// default value), updates the preference to explicitly keep the bookmarks bar
// visible on the NTP.
void UpdateBookmarkBarVisibilityPrefOnUserAction(Profile* profile);

// Returns whether the Apps shortcut is enabled. If true, then the visibility
// of the Apps shortcut should be controllable via an item in the bookmark
// context menu.
bool IsAppsShortcutEnabled(Profile* profile);

// Returns true if the Apps shortcut should be displayed in the bookmark bar.
bool ShouldShowAppsShortcutInBookmarkBar(Profile* profile);

// Returns true if the tab groups should be displayed in the bookmark bar.
bool ShouldShowTabGroupsInBookmarkBar(Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_BAR_UTILS_H_
