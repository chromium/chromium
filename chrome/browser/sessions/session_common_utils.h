// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_COMMON_UTILS_H_
#define CHROME_BROWSER_SESSIONS_SESSION_COMMON_UTILS_H_

namespace sessions {
struct SessionTab;
}  // namespace sessions

class GURL;

// Returns whether sessions code should track a URL for restoring in the context
// of //chrome. In particular, does not allow chrome://quit and
// chrome://restart to avoid quit or restart loops.
bool ShouldTrackURLForRestore(const GURL& url);

// Returns the current navigation index from the tab. If current navigation's
// url is the sign out page and the url of previous one is the setting page,
// returns the index of previous one.
int GetNavigationIndexToSelect(const sessions::SessionTab& tab);

#endif  // CHROME_BROWSER_SESSIONS_SESSION_COMMON_UTILS_H_
