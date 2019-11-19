// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

// Should always open incognito windows when started with --incognito switch?
extern const bool kAlwaysOpenIncognitoWindow;

// Indicates whether session restore should always create a new
// tabbed browser. This is true every where except on ChromeOS
// where we want the desktop to show through in this situation.
extern const bool kAlwaysCreateTabbedBrowserOnSessionRestore;

// Does the download page have the show in folder option?
extern const bool kDownloadPageHasShowInFolder;

// If true, we want to automatically start sync signin whenever we have
// credentials (user doesn't need to go through the startup flow). This is
// typically enabled on platforms (like ChromeOS) that have their own
// distinct signin flow.
extern const bool kSyncAutoStarts;

// Should scroll events on the tabstrip change tabs?
extern const bool kScrollEventChangesTab;

// Last character display for passwords.
extern const bool kPasswordEchoEnabled;

//=============================================================================
// Runtime "const" - set only once after parsing command line option and should
// never be modified after that.

// Are bookmark enabled? True by default.
extern bool bookmarks_enabled;

// Whether HelpApp is enabled. True by default. This is only used by Chrome OS
// today.
extern bool enable_help_app;

}  // namespace browser_defaults

#endif  // CHROME_BROWSER_DEFAULTS_H_
