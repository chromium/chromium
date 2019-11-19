// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace browser_defaults {

#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
const bool kBrowserAliveWithNoWindows = true;
const bool kShowExitMenuItem = false;
#else
const bool kBrowserAliveWithNoWindows = false;
const bool kShowExitMenuItem = true;
#endif

#if defined(OS_CHROMEOS)
const bool kShowUpgradeMenuItem = false;
const bool kShowImportOnBookmarkBar = false;
const bool kAlwaysOpenIncognitoWindow = true;
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = false;
#else
const bool kShowUpgradeMenuItem = true;
const bool kShowImportOnBookmarkBar = true;
const bool kAlwaysOpenIncognitoWindow = false;
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = true;
#endif

#if defined(OS_CHROMEOS)
const bool kShowHelpMenuItemIcon = true;
#else
const bool kShowHelpMenuItemIcon = false;
#endif

const bool kDownloadPageHasShowInFolder = true;

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
const bool kSyncAutoStarts = true;
#else
const bool kSyncAutoStarts = false;
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
const bool kScrollEventChangesTab = true;
#else
const bool kScrollEventChangesTab = false;
#endif

#if !defined(OS_ANDROID)
const bool kPasswordEchoEnabled = false;
#endif

bool bookmarks_enabled = true;

bool enable_help_app = true;

}  // namespace browser_defaults
