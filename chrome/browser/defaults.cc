// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace browser_defaults {

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
const bool kBrowserAliveWithNoWindows = true;
const bool kShowExitMenuItem = false;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
// Note: Lacros can get kicked out of memory when the last window closes.
const bool kBrowserAliveWithNoWindows = false;
const bool kShowExitMenuItem = false;
#else
const bool kBrowserAliveWithNoWindows = false;
const bool kShowExitMenuItem = true;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const bool kShowUpgradeMenuItem = false;
const bool kShowImportOnBookmarkBar = false;
const bool kAlwaysOpenIncognitoBrowserIfStartedWithIncognitoSwitch = true;
#else
const bool kShowUpgradeMenuItem = true;
const bool kShowImportOnBookmarkBar = true;
const bool kAlwaysOpenIncognitoBrowserIfStartedWithIncognitoSwitch = false;
#endif

#if BUILDFLAG(IS_CHROMEOS)
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = false;
#else
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = true;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const bool kShowHelpMenuItemIcon = true;
#else
const bool kShowHelpMenuItemIcon = false;
#endif

const bool kDownloadPageHasShowInFolder = true;

#if BUILDFLAG(IS_LINUX)
const bool kScrollEventChangesTab = true;
#else
const bool kScrollEventChangesTab = false;
#endif

#if !BUILDFLAG(IS_ANDROID)
const bool kPasswordEchoEnabled = false;
#endif

bool bookmarks_enabled = true;

bool enable_help_app = true;

}  // namespace browser_defaults
