// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
#define CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

class Browser;

namespace chrome {

// Starts a user initiated exit process. Called from Browser::Exit.
// On platforms other than ChromeOS, this is equivalent to
// CloseAllBrowsers() On ChromeOS, this tells session manager
// that chrome is signing out, which lets session manager send
// SIGTERM to start actual exit process.
void AttemptUserExit();

// Starts a user initiated restart process. On platforms other than
// chromeos, this sets a restart bit in the preference so that
// chrome will be restarted at the end of shutdown process. On
// ChromeOS, this simply exits the chrome, which lets sesssion
// manager re-launch the browser with restore last session flag.
void AttemptRestart();

// Starts a user initiated relaunch process. On platforms other than Chrome OS,
// this is equivalent to AttemptRestart. On Chrome OS, this relaunches the
// entire OS, instead of just relaunching the browser.
void AttemptRelaunch();

// Attempt to exit by closing all browsers.  This is equivalent to
// CloseAllBrowsers() on platforms where the application exits
// when no more windows are remaining. On other platforms (the Mac),
// this will additionally exit the application if all browsers are
// successfully closed.
//  Note that the exit process may be interrupted by download or
// unload handler, and the browser may or may not exit.
void AttemptExit();

// Shutdown chrome cleanly without blocking. This always sets
// exit-cleanly bit and exits the browser, even if there is
// ongoing downloads or a page with onbeforeunload handler.
//
// If you need to exit or restart in your code on ChromeOS,
// use AttemptExit or AttemptRestart respectively.
void ExitIgnoreUnloadHandlers();

}  // namespace chrome

#endif  // CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
