// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
#define CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/installer/util/initial_preferences.h"

class GURL;
class Profile;

namespace base {
class CommandLine;
class FilePath;
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

enum ProcessInitialPreferencesResult {
  FIRST_RUN_PROCEED = 0,  // Proceed with first run.
  EULA_EXIT_NOW,          // Should immediately exit due to EULA flow.
};

// See ProcessInitialPreferences for more info about this structure.
struct MasterPrefs {
  MasterPrefs();
  ~MasterPrefs();

  // TODO(macourteau): as part of the initial preferences refactoring effort,
  // remove items from here which are being stored temporarily only to be later
  // dumped into local_state. Also see related TODO in chrome_browser_main.cc.

  bool make_chrome_default_for_user = false;
  std::vector<GURL> new_tabs;
  std::vector<GURL> bookmarks;
  std::string import_bookmarks_path;
  std::string suppress_default_browser_prompt_for_version;
#if BUILDFLAG(IS_MAC)
  bool confirm_to_quit;
#endif
};

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Returns true if Chrome should behave as if this is the first time Chrome is
// run for this user.
bool IsChromeFirstRun();

#if BUILDFLAG(IS_MAC)
// Returns true if |command_line|'s switches explicitly specify that first run
// should be suppressed in the current run.
bool IsFirstRunSuppressed(const base::CommandLine& command_line);
#endif

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

// Automatically imports items requested by |profile|'s configuration (sum of
// policies and initial prefs). Also imports bookmarks from file if
// |import_bookmarks_path| is not empty.
void AutoImport(Profile* profile,
                const std::string& import_bookmarks_path);

// Does remaining first run tasks. This can pop the first run consent dialog on
// linux. |make_chrome_default_for_user| is the value of
// kMakeChromeDefaultForUser in master_preferences which contributes to the
// decision of making chrome default browser in post import tasks.
void DoPostImportTasks(bool make_chrome_default_for_user);

// Returns the current state of AutoImport as recorded in a bitfield formed from
// values in AutoImportState.
uint16_t auto_import_state();

// Set a initial preferences file path that overrides platform defaults.
void SetInitialPrefsPathForTesting(const base::FilePath& master_prefs);

// Loads initial preferences from the initial preference file into the installer
// initial preferences. Returns the pointer to installer::InitialPreferences
// object if successful; otherwise, returns nullptr.
std::unique_ptr<installer::InitialPreferences> LoadInitialPrefs();

// The master_preferences is a JSON file with the same entries as the
// 'Default\Preferences' file. This function locates this file from a standard
// location, processes it, and uses its content to initialize the preferences
// for the profile pointed to by |user_data_dir|. After processing the file,
// this function returns a value from the ProcessInitialPreferencesResult enum,
// indicating whether the first run flow should be shown, skipped, or whether
// the browser should exit.
//
// This function overwrites any existing Preferences file and is only meant to
// be invoked on first run.
//
// See chrome/installer/util/initial_preferences.h for a description of
// 'master_preferences' file.
ProcessInitialPreferencesResult ProcessInitialPreferences(
    const base::FilePath& user_data_dir,
    std::unique_ptr<installer::InitialPreferences> initial_prefs,
    MasterPrefs* out_prefs);

}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_FIRST_RUN_H_
