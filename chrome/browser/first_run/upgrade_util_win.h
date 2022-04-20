// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_WIN_H_
#define CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_WIN_H_

namespace base {
class CommandLine;
}

namespace upgrade_util {

// If the new_chrome.exe exists (placed by the installer then is swapped
// to chrome.exe and the old chrome is renamed to old_chrome.exe. If there
// is no new_chrome.exe or the swap fails the return is false;
bool SwapNewChromeExeIfPresent();

// Returns true if the currently running browser is running from old_chrome.exe.
// This may mean that the running executable is out of date and has been renamed
// by the in-use update process. old_chrome.exe shouldn't continue on and run as
// the browser process since it may end up launching newer chrome.exes as child
// processes resulting in a version mismatch.
bool IsRunningOldChrome();

// Returns true if chrome.exe was swapped with new_chrome.exe, and an attempt
// was made to launch the new chrome.exe, false otherwise. Combines the two
// methods, RelaunchChromeBrowser and SwapNewChromeExeIfPresent, to perform the
// rename and relaunch of the browser. Note that relaunch does NOT exit the
// existing browser process. If this returns true before message loop is
// executed, simply exit the main function. If browser is already running, you
// will need to exit it.
bool DoUpgradeTasks(const base::CommandLine& command_line);

}  // namespace upgrade_util

#endif  // CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_WIN_H_
