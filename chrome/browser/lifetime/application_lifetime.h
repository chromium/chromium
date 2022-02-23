// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
#define CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_

#include "base/callback.h"
#include "base/callback_list.h"
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

#if !BUILDFLAG(IS_ANDROID)
// Starts an administrator-initiated relaunch process. On platforms other than
// Chrome OS, this relaunches the browser and restores the user's session. On
// Chrome OS, this restarts the entire OS. This differs from AttemptRelaunch in
// that all user prompts (e.g., beforeunload handlers and confirmation to abort
// in-progress downloads) are bypassed.
void RelaunchIgnoreUnloadHandlers();
#endif

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns true if any of the above Attempt calls have been called.
bool IsAttemptingShutdown();
#endif

#if !BUILDFLAG(IS_ANDROID)
// Closes all browsers and if successful, quits.
void CloseAllBrowsersAndQuit();

// Closes all browsers. If the session is ending the windows are closed
// directly. Otherwise the windows are closed by way of posting a WM_CLOSE
// message. This will quit the application if there is nothing other than
// browser windows keeping it alive or the application is quitting.
void CloseAllBrowsers();

// If there are no browsers open and we aren't already shutting down,
// initiate a shutdown.
void ShutdownIfNeeded();

// Begins shutdown of the application when the desktop session is ending.
void SessionEnding();

// Called once the application is exiting.
void OnAppExiting();

// Called once the application is exiting to do any platform specific
// processing required.
void HandleAppExitingForPlatform();

// Called when the process of closing all browsers starts or is cancelled.
void OnClosingAllBrowsers(bool closing);

// Registers a callback that will be invoked with true when all browsers start
// closing, and false if and when that process is cancelled.
base::CallbackListSubscription AddClosingAllBrowsersCallback(
    base::RepeatingCallback<void(bool)> closing_all_browsers_callback);
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace chrome

#endif  // CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
