// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/installer/installer_win.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/win/scoped_variant.h"
#include "chrome/browser/google/google_update_app_command.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/util/install_util.h"

namespace {

// Switch used to install platform_experience_helper
const char kPlatformExperienceHelperForceInstallSwitch[] = "force-install";
// Directory under which platform_experience_helper is installed
const wchar_t kPlatformExperienceHelperDir[] = L"PlatformExperienceHelper";
// Name of the platform_experience_helper executable
const wchar_t kPlatformExperienceHelperExe[] =
    L"platform_experience_helper.exe";

// This function might block.
// Returns true if the platform_experience_helper is installed.
// Returns true if it can't determine whether it's installed or not.
bool PlatformExperienceHelperMightBeInstalled() {
  base::FilePath peh_base_dir = base::PathService::CheckedGet(
      install_static::IsSystemInstall()
          ? static_cast<int>(base::DIR_EXE)
          : static_cast<int>(chrome::DIR_USER_DATA));

  base::FilePath peh_exe_path =
      peh_base_dir.Append(kPlatformExperienceHelperDir)
          .Append(kPlatformExperienceHelperExe);
  return base::PathExists(peh_exe_path);
}

}  // namespace

namespace platform_experience {

void MaybeInstallPlatformExperienceHelper() {
  if (PlatformExperienceHelperMightBeInstalled()) {
    return;
  }

  // TODO(crbug.com/422447800): Report metrics for number of installer launch
  // attempts (success vs. failure), split by user vs system installs.
  if (install_static::IsSystemInstall()) {
    auto command = GetUpdaterAppCommand(installer::kCmdInstallPEH);
    if (!command.has_value()) {
      return;
    }

    const VARIANT& var = base::win::ScopedVariant::kEmptyVariant;
    (*command)->execute(var, var, var, var, var, var, var, var, var);
    return;
  }

  base::FilePath peh_installer_path =
      base::PathService::CheckedGet(base::DIR_MODULE)
          .Append(FILE_PATH_LITERAL("os_update_handler.exe"));
  base::CommandLine install_cmd(peh_installer_path);
  install_cmd.AppendSwitch(kPlatformExperienceHelperForceInstallSwitch);
  InstallUtil::AppendModeAndChannelSwitches(&install_cmd);

  base::LaunchOptions launch_options;
  launch_options.feedback_cursor_off = true;
  launch_options.force_breakaway_from_job_ = true;
  ::SetLastError(ERROR_SUCCESS);
  base::Process process = base::LaunchProcess(install_cmd, launch_options);
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch \"" << install_cmd.GetCommandLineString()
                << "\"";
  }
}

}  // namespace platform_experience
