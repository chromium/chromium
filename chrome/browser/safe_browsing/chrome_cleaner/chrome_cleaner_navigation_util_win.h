// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_NAVIGATION_UTIL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_NAVIGATION_UTIL_WIN_H_

#include "chrome/browser/ui/browser.h"

namespace chrome_cleaner_util {

// Returns the last active tabbed browser or NULL if no such browser currently
// exists.
Browser* FindBrowser();

// Returns true if the page with Cleaner UI is the currently active tab.
bool CleanupPageIsActiveTab(Browser* browser);

// Opens a new settings tab containing Cleaner UI card.
void OpenCleanupPage(Browser* browser, WindowOpenDisposition disposition);

}  // namespace chrome_cleaner_util

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_NAVIGATION_UTIL_WIN_H_
