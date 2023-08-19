// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_BROWSER_SHUTDOWN_H_
#define CHROME_BROWSER_LIFETIME_BROWSER_SHUTDOWN_H_

#include <stdint.h>

#include "base/auto_reset.h"
#include "build/build_config.h"

class PrefRegistrySimple;

namespace browser_shutdown {

#if !BUILDFLAG(IS_ANDROID)

// The type of restart to perform during shutdown; see ShutdownPostThreadsStop.
enum class RestartMode {
  // Do not restart the browser.
  kNoRestart,

  // Restart the browser. This is typically used in conjunction with the
  // prefs::kWasRestarted Local State preference to restore the user's browsing
  // session. Regardless of whether or not prefs::kWasRestarted is used,
  // single-use switches (e.g., --app) and any URLs are stripped from the
  // command line.
  kRestartLastSession,

  // Restart the browser into the background. The browser will appear in the
  // system tray without opening any browser windows. This has no effect if
  // |RESTART| is not also set.
  kRestartInBackground,

  // Restart the browser using the original command line. This is useful in
  // cases where startup was interrupted and will continue in the new process.
  // Restart loops are prevented by adding switches::kRelaunched to the command
  // line of the new process.
  kRestartThisSession,
};

#endif  // !BUILDFLAG(IS_ANDROID)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ShutdownType {
  // An uninitialized value.
  kNotValid = 0,
  // The last browser window was closed.
  kWindowClose = 1,
  // The user clicked on the Exit menu item.
  kBrowserExit = 2,
  // User logoff or system shutdown.
  kEndSession = 3,
  // Exit without onbeforeunload or in-progress download prompts.
  kSilentExit = 4,
  // The browser process is exiting but not by a user action. These exit paths
  // can happen with early exit paths where the browser main is not executed.
  kOtherExit = 5,
  kMaxValue = kOtherExit
};

void RegisterPrefs(PrefRegistrySimple* registry);

// Called when the browser starts shutting down so that we can measure shutdown
// time.
void OnShutdownStarting(ShutdownType type);

// Returns true if OnShutdownStarting has been called to note that shutdown has
// started.
// TODO (crbug/1155597): Make this work in sync with
// BrowserProcessImpl::IsShuttingDown.
bool HasShutdownStarted();

// Returns true if OnShutdownStarting has been called and unload handlers (e.g.,
// an in-progress download or a page's beforeunload handler) should be ignored.
// This is true for kEndSession and kSilentExit shutdown types.
bool ShouldIgnoreUnloadHandlers();

// Get the current shutdown type.
ShutdownType GetShutdownType();

#if !BUILDFLAG(IS_ANDROID)
// Performs the shutdown tasks that need to be done before
// BrowserProcess and the various threads go away.
//
// Returns true if the session should be restarted.
bool ShutdownPreThreadsStop();

// Records the shutdown retrics.
void RecordShutdownMetrics();

// Records the shutdown related prefs, and returns true if the browser should be
// restarted on exit.
bool RecordShutdownInfoPrefs();

// Performs the remaining shutdown tasks after all threads but the
// main thread have been stopped.  This includes deleting g_browser_process.
void ShutdownPostThreadsStop(RestartMode restart_mode);
#endif

// There are various situations where the browser process should continue to
// run after the last browser window has closed - the Mac always continues
// running until the user explicitly quits, and on Windows/Linux the application
// should not shutdown when the last browser window closes if there are any
// BackgroundContents running.
// When the user explicitly chooses to shutdown the app (via the "Exit" or
// "Quit" menu items) BrowserList will call SetTryingToQuit() to tell itself to
// initiate a shutdown when the last window closes.
// If the quit is aborted, then the flag should be reset.

// This is a low-level mutator; in general, don't call SetTryingToQuit(true),
// except from appropriate places in BrowserList. To quit, use usual means,
// e.g., using |chrome_browser_application_mac::Terminate()| on the Mac, or
// |BrowserList::CloseAllWindowsAndExit()| on other platforms. To stop quitting,
// use |chrome_browser_application_mac::CancelTerminate()| on the Mac; other
// platforms can call SetTryingToQuit(false) directly.
void SetTryingToQuit(bool quitting);

// General accessor.
bool IsTryingToQuit();

// Allows setting a fake shutdown type for testing purposes.
base::AutoReset<ShutdownType> SetShutdownTypeForTesting(
    ShutdownType shutdown_type);

// Allows resetting the shutdown globals for testing purposes.
void ResetShutdownGlobalsForTesting();

}  // namespace browser_shutdown

#endif  // CHROME_BROWSER_LIFETIME_BROWSER_SHUTDOWN_H_
