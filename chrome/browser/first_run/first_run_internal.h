// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace base {
class FilePath;
}

namespace installer {
class InitialPreferences;
}

namespace first_run {

struct MasterPrefs;

namespace internal {

enum FirstRunState {
  FIRST_RUN_UNKNOWN,  // The state is not tested or set yet.
  FIRST_RUN_TRUE,
  FIRST_RUN_FALSE,
};

// Sets up initial preferences by preferences passed by installer.
void SetupInitialPrefsFromInstallPrefs(
    const installer::InitialPreferences& install_prefs,
    MasterPrefs* out_prefs);

// -- Platform-specific functions --

void DoPostImportPlatformSpecificTasks();

// This function has a common implementationin for all non-linux platforms, and
// a linux specific implementation.
bool IsOrganicFirstRun();

// Shows the EULA dialog if required. Returns true if the EULA is accepted,
// returns false if the EULA has not been accepted, in which case the browser
// should exit.
bool ShowPostInstallEULAIfNeeded(installer::InitialPreferences* install_prefs);

// Returns the path for the initial preferences file.
base::FilePath InitialPrefsPath();

// Helper for IsChromeFirstRun. Exposed for testing.
FirstRunState DetermineFirstRunState(bool has_sentinel,
                                     bool force_first_run,
                                     bool no_first_run);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// For testing, forces the first run dialog to either be shown or not. If not
// called, the decision to show the dialog or not will be made by Chrome based
// on a number of factors (such as install type, whether it's a Chrome-branded
// build, etc).
void ForceFirstRunDialogShownForTesting(bool shown);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace internal
}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_INTERNAL_H_
