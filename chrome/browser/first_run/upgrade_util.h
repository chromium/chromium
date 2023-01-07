// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_
#define CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
#error Not used on Android or ChromeOS
#endif

namespace base {
class CommandLine;
}

namespace upgrade_util {

// Launches Chrome again simulating a "user" launch. If Chrome could not be
// launched, returns false.
bool RelaunchChromeBrowser(const base::CommandLine& command_line);

#if !BUILDFLAG(IS_MAC)

// Sets a command line to be used to relaunch the browser upon exit.
void SetNewCommandLine(std::unique_ptr<base::CommandLine> new_command_line);

// Launches a new instance of the browser using a command line previously
// provided to SetNewCommandLine. This is typically used to finalize an in-use
// update that was detected while the browser was in persistent mode.
void RelaunchChromeBrowserWithNewCommandLineIfNeeded();

// Windows:
//  Checks if chrome_new.exe is present in the current instance's install.
// Linux:
//  Checks if the last modified time of chrome is newer than that of the current
//  running instance.
bool IsUpdatePendingRestart();

#endif  // !BUILDFLAG(IS_MAC)

using RelaunchChromeBrowserCallback =
    base::RepeatingCallback<bool(const base::CommandLine&)>;

// Sets |callback| to be run to process a RelaunchChromeBrowser request. This
// is a test seam for whole-browser tests. See
// ScopedRelaunchChromeBrowserOverride for convenience.
RelaunchChromeBrowserCallback SetRelaunchChromeBrowserCallbackForTesting(
    RelaunchChromeBrowserCallback callback);

}  // namespace upgrade_util

#endif  // CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_
