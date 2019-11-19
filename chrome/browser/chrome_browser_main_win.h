// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are win32-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_WIN_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_WIN_H_

#include <memory>

#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "chrome/browser/chrome_browser_main.h"

class ModuleWatcher;

namespace base {
class CommandLine;
}

// Handle uninstallation when given the appropriate the command-line switch.
// If |chrome_still_running| is true a modal dialog will be shown asking the
// user to close the other chrome instance.
int DoUninstallTasks(bool chrome_still_running);

class ChromeBrowserMainPartsWin : public ChromeBrowserMainParts {
 public:
  ChromeBrowserMainPartsWin(const content::MainFunctionParams& parameters,
                            StartupData* startup_data);

  ~ChromeBrowserMainPartsWin() override;

  // BrowserParts overrides.
  void ToolkitInitialized() override;
  void PreMainMessageLoopStart() override;
  int PreCreateThreads() override;

  // ChromeBrowserMainParts overrides.
  void ShowMissingLocaleMessageBox() override;
  void PostProfileInit() override;
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

 private:
  // Watches module load events and forwards them to the ModuleDatabase.
  std::unique_ptr<ModuleWatcher> module_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsWin);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_WIN_H_
