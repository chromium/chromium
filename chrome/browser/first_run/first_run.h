// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/installer/util/master_preferences.h"

class GURL;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This namespace contains the chrome first-run installation actions needed to
// fully test the custom installer. It also contains the opposite actions to
// execute during uninstall. When the first run UI is ready we won't
// do the actions unconditionally. Currently the only action is to create a
// desktop shortcut.
//
// The way we detect first-run is by looking at a 'sentinel' file.
// If it does not exist we understand that we need to do the first time
// install work for this user. After that the sentinel file is created.
namespace first_run {

enum AutoImportState {
  AUTO_IMPORT_NONE = 0,
  AUTO_IMPORT_CALLED = 1 << 0,
  AUTO_IMPORT_PROFILE_IMPORTED = 1 << 1,
  AUTO_IMPORT_BOOKMARKS_FILE_IMPORTED = 1 << 2,
};

enum ProcessMasterPreferencesResult {
  FIRST_RUN_PROCEED = 0,  // Proceed with first run.
  EULA_EXIT_NOW,          // Should immediately exit due to EULA flow.
};

// See ProcessMasterPreferences for more info about this structure.
struct MasterPrefs {
  MasterPrefs();
  ~MasterPrefs();

  // TODO(macourteau): as part of the master preferences refactoring effort,
  // remove items from here which are being stored temporarily only to be later
  // dumped into local_state. Also see related TODO in chrome_browser_main.cc.

  bool make_chrome_default_for_user = false;
  std::vector<GURL> new_tabs;
  std::vector<GURL> bookmarks;
  std::string import_bookmarks_path;
  std::string suppress_default_browser_prompt_for_version;
};

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Returns true if Chrome should behave as if this is the first time Chrome is
// run for this user.
bool IsChromeFirstRun();

#if defined(OS_MACOSX)
// Returns true if |command_line|'s switches explicitly specify that first run
// should be suppressed in the current run.
bool IsFirstRunSuppressed(const base::CommandLine& command_line);
#endif

// Returns whether metrics reporting is currently opt-in. This is used to
// determine if the enable metrics reporting checkbox on first-run should be
// initially checked. Opt-in means it is not initially checked, opt-out means it
// is. This is not guaranteed to be correct outside of the first-run situation,
// as the default may change over time. For that, use
// GetMetricsReportingDefaultState in
// chrome/browser/metrics/metrics_reporting_state.h, which gives a value that
// was stored during first-run.
bool IsMetricsReportingOptIn();

// Creates the first run sentinel if needed. This should only be called after
// the process singleton has been grabbed by the current process
// (http://crbug.com/264694).
void CreateSentinelIfNeeded();

// Returns the first run sentinel creation time. This only requires I/O
// permission on the sequence it is first called on.
base::Time GetFirstRunSentinelCreationTime();

// Resets the first run status and cached first run sentinel creation time.
// This is needed for unit tests which are runned in the same process.
void ResetCachedSentinelDataForTesting();

// Sets a flag that will cause ShouldShowWelcomePage to return true
// exactly once, so that the browser loads the welcome tab once the
// message loop gets going.
void SetShouldShowWelcomePage();

// Returns true if the welcome page should be shown.
//
// This will return true only once: The first time it is called after
// SetShouldShowWelcomePage() is called.
bool ShouldShowWelcomePage();

// Returns true if |contents| hosts one of the welcome pages.
bool IsOnWelcomePage(content::WebContents* contents);

// Iterates over the given tabs, replacing "magic words" designated for
// use in Master Preferences files with corresponding URLs.
std::vector<GURL> ProcessMasterPrefsTabs(const std::vector<GURL>& tabs);

// Sets a flag that will cause ShouldDoPersonalDataManagerFirstRun()
// to return true exactly once, so that the browser loads
// PersonalDataManager once the main message loop gets going.
void SetShouldDoPersonalDataManagerFirstRun();

// Returns true if the autofill personal data manager first-run action
// should be taken.
//
// This will return true only once, the first time it is called after
// SetShouldDoPersonalDataManagerFirstRun() is called.
bool ShouldDoPersonalDataManagerFirstRun();

// Automatically imports items requested by |profile|'s configuration (sum of
// policies and master prefs). Also imports bookmarks from file if
// |import_bookmarks_path| is not empty.
void AutoImport(Profile* profile,
                const std::string& import_bookmarks_path);

// Does remaining first run tasks. This can pop the first run consent dialog on
// linux. |make_chrome_default_for_user| is the value of
// kMakeChromeDefaultForUser in master_preferences which contributes to the
// decision of making chrome default browser in post import tasks.
void DoPostImportTasks(Profile* profile, bool make_chrome_default_for_user);

// Returns the current state of AutoImport as recorded in a bitfield formed from
// values in AutoImportState.
uint16_t auto_import_state();

// Set a master preferences file path that overrides platform defaults.
void SetMasterPrefsPathForTesting(const base::FilePath& master_prefs);

// Loads master preferences from the master preference file into the installer
// master preferences. Returns the pointer to installer::MasterPreferences
// object if successful; otherwise, returns nullptr.
std::unique_ptr<installer::MasterPreferences> LoadMasterPrefs();

// The master_preferences is a JSON file with the same entries as the
// 'Default\Preferences' file. This function locates this file from a standard
// location, processes it, and uses its content to initialize the preferences
// for the profile pointed to by |user_data_dir|. After processing the file,
// this function returns a value from the ProcessMasterPreferencesResult enum,
// indicating whether the first run flow should be shown, skipped, or whether
// the browser should exit.
//
// This function overwrites any existing Preferences file and is only meant to
// be invoked on first run.
//
// See chrome/installer/util/master_preferences.h for a description of
// 'master_preferences' file.
ProcessMasterPreferencesResult ProcessMasterPreferences(
    const base::FilePath& user_data_dir,
    std::unique_ptr<installer::MasterPreferences> install_prefs,
    MasterPrefs* out_prefs);

}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
