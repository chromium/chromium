// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/installer/installer_win.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/win/scoped_variant.h"
#include "chrome/browser/google/google_update_app_command.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/util/install_util.h"

namespace {

// Switch used to install platform_experience_helper
constexpr char kPlatformExperienceHelperForceInstallSwitch[] = "force-install";
// Directory under which platform_experience_helper is installed
constexpr wchar_t kPlatformExperienceHelperDir[] = L"PlatformExperienceHelper";
// Name of the platform_experience_helper executable
constexpr wchar_t kPlatformExperienceHelperExe[] =
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

// Enum for tracking the launch status of the platform experience helper
// installer for system installs.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SystemInstallerLaunchStatus)
enum class SystemInstallerLaunchStatus {
  kSuccess = 0,
  kAppCommandNotFound = 1,
  kAppCommandExecutionFailed = 2,
  kMaxValue = kAppCommandExecutionFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/enums.xml:SystemInstallerLaunchStatus)

// Enum for tracking the launch status of the platform experience helper
// installer for user installs.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(UserInstallerLaunchStatus)
enum class UserInstallerLaunchStatus {
  kSuccess = 0,
  kFileNotFound = 1,
  kAccessDenied = 2,
  kOtherFailure = 3,
  kMaxValue = kOtherFailure,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/windows/enums.xml:UserInstallerLaunchStatus)

void ReportSystemInstallerLaunchStatus(SystemInstallerLaunchStatus status) {
  base::UmaHistogramEnumeration(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.System", status);
}

void ReportUserInstallLaunchStatusMetric(UserInstallerLaunchStatus status) {
  base::UmaHistogramEnumeration(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.User", status);
}

}  // namespace

namespace platform_experience {

void MaybeInstallPlatformExperienceHelper() {
  if (PlatformExperienceHelperMightBeInstalled()) {
    return;
  }

  if (install_static::IsSystemInstall()) {
    auto command = GetUpdaterAppCommand(installer::kCmdInstallPEH);
    if (!command.has_value()) {
      ReportSystemInstallerLaunchStatus(
          SystemInstallerLaunchStatus::kAppCommandNotFound);
      return;
    }

    const VARIANT& var = base::win::ScopedVariant::kEmptyVariant;
    if (FAILED(
            (*command)->execute(var, var, var, var, var, var, var, var, var))) {
      ReportSystemInstallerLaunchStatus(
          SystemInstallerLaunchStatus::kAppCommandExecutionFailed);
    } else {
      ReportSystemInstallerLaunchStatus(SystemInstallerLaunchStatus::kSuccess);
    }
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
  UserInstallerLaunchStatus status = UserInstallerLaunchStatus::kSuccess;
  if (!process.IsValid()) {
    switch (::GetLastError()) {
      case ERROR_FILE_NOT_FOUND:
        status = UserInstallerLaunchStatus::kFileNotFound;
        break;
      case ERROR_ACCESS_DENIED:
        status = UserInstallerLaunchStatus::kAccessDenied;
        break;
      default:
        status = UserInstallerLaunchStatus::kOtherFailure;
        break;
    }
  }
  ReportUserInstallLaunchStatusMetric(status);
}

}  // namespace platform_experience
