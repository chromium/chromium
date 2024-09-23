// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include <windows.h>

#include <shellapi.h>
#include <stdint.h>

#include <string_view>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/strings/grit/components_locale_settings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/shell.h"

namespace {

// Launches the setup exe with the given parameter/value on the command-line.
// For non-metro Windows, it waits for its termination, returns its exit code
// in |*ret_code|, and returns true if the exit code is valid.
// For metro Windows, it launches setup via ShellExecuteEx and returns in order
// to bounce the user back to the desktop, then returns immediately.
bool LaunchSetupForEula(const base::FilePath::StringType& value,
                        int* ret_code) {
  base::FilePath exe_dir;
  if (!base::PathService::Get(base::DIR_MODULE, &exe_dir))
    return false;
  exe_dir = exe_dir.Append(installer::kInstallerDir);
  base::FilePath exe_path = exe_dir.Append(installer::kSetupExe);

  base::CommandLine cl(base::CommandLine::NO_PROGRAM);
  cl.AppendSwitchNative(installer::switches::kShowEula, value);

  base::CommandLine setup_path(exe_path);
  setup_path.AppendArguments(cl, false);

  base::Process process =
      base::LaunchProcess(setup_path, base::LaunchOptions());
  int exit_code = 0;
  if (!process.IsValid() || !process.WaitForExit(&exit_code))
    return false;

  *ret_code = exit_code;
  return true;
}

// Returns true if the EULA is required but has not been accepted by this user.
// The EULA is considered having been accepted if the user has gotten past
// first run in the "other" environment (desktop or metro).
bool IsEULANotAccepted(installer::InitialPreferences* install_prefs) {
  bool value = false;
  if (install_prefs->GetBool(installer::initial_preferences::kRequireEula,
                             &value) &&
      value) {
    base::FilePath eula_sentinel;
    // Be conservative and show the EULA if the path to the sentinel can't be
    // determined.
    if (!InstallUtil::GetEulaSentinelFilePath(&eula_sentinel) ||
        !base::PathExists(eula_sentinel)) {
      return true;
    }
  }
  return false;
}

// Writes the EULA to a temporary file, returned in |*eula_path|, and returns
// true if successful.
bool WriteEULAtoTempFile(base::FilePath* eula_path) {
  std::string terms =
      ui::ResourceBundle::GetSharedInstance().LoadLocalizedResourceString(
          IDS_TERMS_HTML);
  return (!terms.empty() && base::CreateTemporaryFile(eula_path) &&
          base::WriteFile(*eula_path, terms));
}

// Creates the sentinel indicating that the EULA was required and has been
// accepted.
bool CreateEULASentinel() {
  base::FilePath eula_sentinel;
  return InstallUtil::GetEulaSentinelFilePath(&eula_sentinel) &&
         base::CreateDirectory(eula_sentinel.DirName()) &&
         base::WriteFile(eula_sentinel, std::string_view());
}

}  // namespace

namespace first_run {
namespace internal {

void DoPostImportPlatformSpecificTasks() {
  // Trigger the Active Setup command for system-level Chromes to finish
  // configuring this user's install (e.g. per-user shortcuts).
  if (!InstallUtil::IsPerUserInstall()) {
    content::BrowserThread::PostBestEffortTask(
        FROM_HERE,
        base::ThreadPool::CreateTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
        base::BindOnce(&InstallUtil::TriggerActiveSetupCommand));
  }
}

bool ShowPostInstallEULAIfNeeded(installer::InitialPreferences* install_prefs) {
  if (IsEULANotAccepted(install_prefs)) {
    // Show the post-installation EULA. This is done by setup.exe and the
    // result determines if we continue or not. We wait here until the user
    // dismisses the dialog.

    // The actual eula text is in a resource in chrome. We extract it to
    // a text file so setup.exe can use it as an inner frame.
    base::FilePath inner_html;
    if (WriteEULAtoTempFile(&inner_html)) {
      int retcode = 0;
      if (!LaunchSetupForEula(inner_html.value(), &retcode) ||
          (retcode != installer::EULA_ACCEPTED &&
           retcode != installer::EULA_ACCEPTED_OPT_IN)) {
        LOG(WARNING) << "EULA flow requires fast exit.";
        return false;
      }
      CreateEULASentinel();

      if (retcode == installer::EULA_ACCEPTED) {
        DVLOG(1) << "EULA : no collection";
        GoogleUpdateSettings::SetCollectStatsConsent(false);
      } else if (retcode == installer::EULA_ACCEPTED_OPT_IN) {
        DVLOG(1) << "EULA : collection consent";
        GoogleUpdateSettings::SetCollectStatsConsent(true);
      }
    }
  }
  return true;
}

base::FilePath InitialPrefsPath() {
  // The standard location of the initial prefs is next to the chrome binary.
  base::FilePath dir_exe;
  if (!base::PathService::Get(base::DIR_EXE, &dir_exe))
    return base::FilePath();

  return installer::InitialPreferences::Path(dir_exe);
}

}  // namespace internal
}  // namespace first_run
