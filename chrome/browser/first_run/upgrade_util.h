// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_
#define CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"

static_assert(!BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS),
              "Not used on Android or ChromeOS");

namespace base {
class CommandLine;
}

namespace browser_shutdown {
enum class RestartMode;
}

namespace upgrade_util {

// Returns a new command line for relaunching Chrome according to
// |restart_mode|. Strips transient flags, normalizes switches, and handles
// arguments according to the restart mode.
base::CommandLine GetRelaunchCommandLine(
    const base::CommandLine& current_command_line,
    browser_shutdown::RestartMode restart_mode);

// Launches Chrome again simulating a "user" launch. If Chrome could not be
// launched, returns false. On Windows, if `force_breakaway_from_job` is true,
// the launched process breaks away from the current process's Job Object.
// `wait_for_parent` specifies whether the launched process should wait for
// this parent process to terminate via kWaitForParentHandle.
bool RelaunchChromeBrowser(const base::CommandLine& command_line,
                           bool force_breakaway_from_job = false,
                           bool wait_for_parent = false);

#if !BUILDFLAG(IS_MAC)
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
