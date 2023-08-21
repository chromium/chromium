// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines various defaults whose values varies depending upon the OS.

#ifndef CHROME_BROWSER_DEFAULTS_H_
#define CHROME_BROWSER_DEFAULTS_H_

#include "build/build_config.h"

namespace browser_defaults {

// Can the browser be alive without any browser windows?
extern const bool kBrowserAliveWithNoWindows;

// Whether various menu items are shown.
extern const bool kShowExitMenuItem;
extern const bool kShowUpgradeMenuItem;

// Only used in branded builds.
extern const bool kShowHelpMenuItemIcon;

// Should a link be shown on the bookmark bar allowing the user to import
// bookmarks?
extern const bool kShowImportOnBookmarkBar;

// If true, redefines `--incognito` switch to cause all browsers to be in
// incognito mode rather than just the initial browser.
extern const bool kAlwaysOpenIncognitoBrowserIfStartedWithIncognitoSwitch;

// Indicates whether session restore should always create a new
// tabbed browser. This is true every where except on ChromeOS
// where we want the desktop to show through in this situation.
extern const bool kAlwaysCreateTabbedBrowserOnSessionRestore;

// Should scroll events on the tabstrip change tabs?
extern const bool kScrollEventChangesTab;

//=============================================================================
// Runtime "const" - set only once after parsing command line option and should
// never be modified after that.

// Are bookmark enabled? True by default.
extern bool bookmarks_enabled;

}  // namespace browser_defaults

#endif  // CHROME_BROWSER_DEFAULTS_H_
