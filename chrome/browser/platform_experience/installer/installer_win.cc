// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/installer/installer_win.h"

#include "base/command_line.h"
#include "base/enterprise_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "chrome/browser/google/google_update_app_command.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/util/install_util.h"

namespace {

platform_experience::InstallerLauncherDelegate*
    g_installer_delegate_for_testing = nullptr;

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

// This function might block.
// Returns whether we should attempt installing the platform experience helper.
bool ShouldInstallPlatformExperienceHelper() {
  if (base::win::GetVersion() < base::win::Version::WIN10) {
    return false;
  }
  if (base::IsManagedOrEnterpriseDevice()) {
    return false;
  }
  return !PlatformExperienceHelperMightBeInstalled();
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
  kOtherFailure = 3,  // This is a catch-all for all other failures.
  kInvalidParameter = 4,
  kElevationRequired = 5,
  kMaxValue = kElevationRequired,
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

class DefaultInstallerLauncherDelegate
    : public platform_experience::InstallerLauncherDelegate {
 public:
  DefaultInstallerLauncherDelegate() = default;
  ~DefaultInstallerLauncherDelegate() override = default;

  Microsoft::WRL::ComPtr<IAppCommandWeb> GetUpdaterAppCommand(
      const std::wstring& command_name) override {
    return ::GetUpdaterAppCommand(command_name).value_or(nullptr);
  }

  base::Process LaunchProcess(const base::CommandLine& cmd_line,
                              const base::LaunchOptions& options) override {
    return base::LaunchProcess(cmd_line, options);
  }
};

platform_experience::InstallerLauncherDelegate* GetDelegate() {
  if (g_installer_delegate_for_testing) {
    return g_installer_delegate_for_testing;
  }
  static DefaultInstallerLauncherDelegate default_delegate;
  return &default_delegate;
}

}  // namespace

namespace platform_experience {

void SetInstallerLauncherDelegateForTesting(  // IN-TEST
    InstallerLauncherDelegate* delegate) {
  g_installer_delegate_for_testing = delegate;
}

void MaybeInstallPlatformExperienceHelper() {
  if (!ShouldInstallPlatformExperienceHelper()) {
    return;
  }

  InstallerLauncherDelegate* delegate = GetDelegate();

  if (install_static::IsSystemInstall()) {
    auto command = delegate->GetUpdaterAppCommand(installer::kCmdInstallPEH);
    if (!command) {
      ReportSystemInstallerLaunchStatus(
          SystemInstallerLaunchStatus::kAppCommandNotFound);
      return;
    }

    const VARIANT& var = base::win::ScopedVariant::kEmptyVariant;
    if (FAILED(command->execute(var, var, var, var, var, var, var, var, var))) {
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
  base::Process process = delegate->LaunchProcess(install_cmd, launch_options);
  UserInstallerLaunchStatus status = UserInstallerLaunchStatus::kSuccess;
  if (!process.IsValid()) {
    switch (::GetLastError()) {
      case ERROR_FILE_NOT_FOUND:
      case ERROR_PATH_NOT_FOUND:
        status = UserInstallerLaunchStatus::kFileNotFound;
        break;
      case ERROR_ACCESS_DENIED:
        status = UserInstallerLaunchStatus::kAccessDenied;
        break;
      case ERROR_INVALID_PARAMETER:
        status = UserInstallerLaunchStatus::kInvalidParameter;
        break;
      case ERROR_ELEVATION_REQUIRED:
        status = UserInstallerLaunchStatus::kElevationRequired;
        break;
      default:
        status = UserInstallerLaunchStatus::kOtherFailure;
        break;
    }
  }
  ReportUserInstallLaunchStatusMetric(status);
}

}  // namespace platform_experience
