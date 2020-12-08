// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace browser_defaults {

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC)
const bool kBrowserAliveWithNoWindows = true;
const bool kShowExitMenuItem = false;
#else
const bool kBrowserAliveWithNoWindows = false;
const bool kShowExitMenuItem = true;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
const bool kShowHelpMenuItemIcon = true;
#else
const bool kShowHelpMenuItemIcon = false;
#endif

const bool kDownloadPageHasShowInFolder = true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
const bool kSyncAutoStarts = true;
#else
const bool kSyncAutoStarts = false;
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
