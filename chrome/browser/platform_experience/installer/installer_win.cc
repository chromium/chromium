// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/installer/installer_win.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/enterprise_util.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "chrome/browser/google/google_update_app_command.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/updater/branded_constants.h"

namespace {

platform_experience::InstallerLauncherDelegate*
    g_installer_delegate_for_testing = nullptr;

// Switch used to install platform_experience_helper
constexpr char kPlatformExperienceHelperForceInstallSwitch[] = "force-install";

using InstallState = platform_experience::PlatformExperienceHelperInstallState;
using OtherScopeRegistration =
    platform_experience::PlatformExperienceHelperOtherScopeRegistration;

struct OmahaRegistration {
  bool is_registered = false;
  base::Version pv_version;
};

bool IsValidNonZeroVersion(const base::Version& version) {
  if (!version.IsValid()) {
    return false;
  }
  return std::ranges::any_of(version.components(),
                             [](uint32_t component) { return component != 0; });
}

OmahaRegistration ReadOmahaRegistration(HKEY root) {
  const std::wstring clients_key_path = base::StrCat(
      {L"Software\\Google\\Update\\Clients\\",
       base::ASCIIToWide(updater::kPlatformExperienceHelperAppId)});
  base::win::RegKey key;
  std::wstring pv;
  if (key.Open(root, clients_key_path.c_str(), KEY_READ | KEY_WOW64_32KEY) !=
          ERROR_SUCCESS ||
      key.ReadValue(L"pv", &pv) != ERROR_SUCCESS || pv.empty()) {
    return {};
  }

  OmahaRegistration registration;
  registration.is_registered = true;
  if (base::IsStringASCII(pv)) {
    registration.pv_version = base::Version(base::WideToASCII(pv));
  }
  return registration;
}

InstallState ClassifyStateWithBinary(const OmahaRegistration& registration,
                                     const base::Version& binary_version) {
  if (!registration.is_registered) {
    return InstallState::kBinaryNoRegistration;
  }
  if (!IsValidNonZeroVersion(registration.pv_version)) {
    return InstallState::kBinaryRegisteredZeroVersion;
  }
  if (IsValidNonZeroVersion(binary_version) &&
      registration.pv_version == binary_version) {
    return InstallState::kHealthy;
  }
  return InstallState::kVersionMismatch;
}

InstallState ClassifyStateWithoutBinary(const OmahaRegistration& registration) {
  if (!registration.is_registered) {
    return InstallState::kNotInstalled;
  }
  if (!IsValidNonZeroVersion(registration.pv_version)) {
    return InstallState::kRegisteredZeroVersion;
  }
  return InstallState::kRegisteredNoBinary;
}

InstallState ProbeCurrentScopeInstallState(
    bool is_system_install,
    platform_experience::PehLauncher& peh_launcher,
    const base::FilePath& binary_path) {
  const OmahaRegistration registration = ReadOmahaRegistration(
      is_system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER);
  if (!binary_path.empty()) {
    return ClassifyStateWithBinary(registration,
                                   peh_launcher.GetBinaryVersion(binary_path));
  }
  return ClassifyStateWithoutBinary(registration);
}

OtherScopeRegistration ProbeOtherScopeRegistration(bool is_system_install) {
  const OmahaRegistration registration = ReadOmahaRegistration(
      is_system_install ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE);
  if (!registration.is_registered) {
    return OtherScopeRegistration::kNotRegistered;
  }
  return IsValidNonZeroVersion(registration.pv_version)
             ? OtherScopeRegistration::kRegisteredWithVersion
             : OtherScopeRegistration::kRegisteredZeroVersion;
}

void RecordInstallStateMetrics(bool is_system_install,
                               platform_experience::PehLauncher& peh_launcher,
                               const base::FilePath& binary_path) {
  const std::string_view scope_suffix = is_system_install ? "System" : "User";
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Windows.PlatformExperienceHelper.InstallState.", scope_suffix}),
      ProbeCurrentScopeInstallState(is_system_install, peh_launcher,
                                    binary_path));
  base::UmaHistogramEnumeration(
      base::StrCat({"Windows.PlatformExperienceHelper.OtherScopeRegistration.",
                    scope_suffix}),
      ProbeOtherScopeRegistration(is_system_install));
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
  if (base::win::GetVersion() < base::win::Version::WIN10 ||
      base::IsManagedOrEnterpriseDevice()) {
    return;
  }

  const bool is_system_install = install_static::IsSystemInstall();
  PehLauncher peh_launcher;
  const base::FilePath peh_binary_path = peh_launcher.GetBinaryPath();
  RecordInstallStateMetrics(is_system_install, peh_launcher, peh_binary_path);

  if (!peh_binary_path.empty()) {
    return;
  }

  InstallerLauncherDelegate* delegate = GetDelegate();

  if (is_system_install) {
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
