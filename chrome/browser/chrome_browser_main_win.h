// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are win32-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_WIN_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_WIN_H_

#include <memory>

#include "chrome/browser/chrome_browser_main.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#include "chrome/install_static/buildflags.h"

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
#include <optional>

#include "chrome/browser/google/did_run_updater_win.h"
#endif

class PlatformAuthPolicyObserver;

namespace base {
class CommandLine;
}

// Handle uninstallation when given the appropriate the command-line switch.
// If |chrome_still_running| is true a modal dialog will be shown asking the
// user to close the other chrome instance.
int DoUninstallTasks(bool chrome_still_running);

class ChromeBrowserMainPartsWin : public ChromeBrowserMainParts {
 public:
  ChromeBrowserMainPartsWin(bool is_integration_test,
                            StartupData* startup_data);
  ChromeBrowserMainPartsWin(const ChromeBrowserMainPartsWin&) = delete;
  ChromeBrowserMainPartsWin& operator=(const ChromeBrowserMainPartsWin&) =
      delete;
  ~ChromeBrowserMainPartsWin() override;

  // BrowserParts overrides.
  void ToolkitInitialized() override;
  void PreCreateMainMessageLoop() override;
  int PreCreateThreads() override;
  void PostMainMessageLoopRun() override;

  // ChromeBrowserMainParts overrides.
  void PostEarlyInitialization() override;
  void ShowMissingLocaleMessageBox() override;
  void PreProfileInit() override;
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
  void PostBrowserStart() override;

  // Prepares the localized strings that are going to be displayed to
  // the user if the browser process dies. These strings are stored in the
  // environment block so they are accessible in the early stages of the
  // chrome executable's lifetime.
  static void PrepareRestartOnCrashEnviroment(
      const base::CommandLine& parsed_command_line);

  // Registers Chrome with the Windows Restart Manager, which will restore the
  // Chrome session when the computer is restarted after a system update.
  static void RegisterApplicationRestart(
      const base::CommandLine& parsed_command_line);

  // This method handles the --hide-icons and --show-icons command line options
  // for chrome that get triggered by Windows from registry entries
  // HideIconsCommand & ShowIconsCommand. Chrome doesn't support hide icons
  // functionality so we just ask the users if they want to uninstall Chrome.
  static int HandleIconsCommands(const base::CommandLine& parsed_command_line);

  // Checks if there is any machine level Chrome installed on the current
  // machine. If yes and the current Chrome process is user level, uninstalls
  // the user-level Chrome and susbsequently auto-launches the system-level
  // Chrome. Returns true if the uninstall was kicked off and this process
  // should exit.
  static bool CheckMachineLevelInstall();

  // Sets the TranslationDelegate which provides localized strings to
  // installer_util.
  static void SetupInstallerUtilStrings();

  // Return a |command_line| copy modified to restore the session after Windows
  // updates. Removes URL args, unnecessary switches, and the program name.
  static base::CommandLine GetRestartCommandLine(
      const base::CommandLine& command_line);

 private:
  void OnModuleEvent(const ModuleWatcher::ModuleEvent& event);
  void SetupModuleDatabase(std::unique_ptr<ModuleWatcher>* module_watcher);

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  // Updates Chrome's "did run" state periodically when the process is in use.
  std::optional<DidRunUpdater> did_run_updater_;
#endif

  // Watches module load events and forwards them to the ModuleDatabase.
  std::unique_ptr<ModuleWatcher> module_watcher_;

  // Applies enterprise policies for platform auth SSO.
  std::unique_ptr<PlatformAuthPolicyObserver> platform_auth_policy_observer_;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_WIN_H_
