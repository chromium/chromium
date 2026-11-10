// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/install_product_component.h"

#include <windows.h>

#include <wrl/client.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_variant.h"
#include "chrome/browser/child_module/child_module_paths.h"
#include "chrome/browser/google/google_update_app_command.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"

namespace component_updater {

class ScopedAllowWaitForProductComponentInstallExit
    : public base::ScopedAllowBaseSyncPrimitives {};

base::expected<Microsoft::WRL::ComPtr<IAppCommandWeb>, HRESULT>
ProductComponentInstallerDelegate::GetAppCommand(
    const std::wstring& command_name) {
  return GetUpdaterAppCommand(command_name);
}

base::Process ProductComponentInstallerDelegate::LaunchProcess(
    const base::CommandLine& cmd,
    const base::LaunchOptions& options) {
  return base::LaunchProcess(cmd, options);
}

bool ProductComponentInstallOutcome::Succeeded() const {
  return step == Step::kInstallerExited &&
         (extra_code == installer::INSTALL_COMPONENT_SUCCESS ||
          extra_code == installer::INSTALL_COMPONENT_ALREADY_EXISTS);
}

namespace {

using Step = ProductComponentInstallOutcome::Step;

// Returns the name of `step` in the InstallCode histograms, or an empty string
// if the step has no code to record.
std::string_view GetStepHistogramName(Step step) {
  switch (step) {
    case Step::kInvalidInput:
    case Step::kNotInstalled:
      return {};
    case Step::kGetAppCommandFailed:
      return "GetAppCommandFailed";
    case Step::kComError:
      return "ComError";
    case Step::kLaunchFailed:
      // The App Command reports no code when it fails to launch setup.exe.
      return install_static::IsSystemInstall() ? std::string_view()
                                               : "LaunchFailed";
    case Step::kInstallerExited:
      return "InstallerExited";
    case Step::kWaitFailed:
      return "WaitFailed";
  }
  NOTREACHED();
}

// Returns true if `path` is on a network drive.
bool IsPathOnNetworkDrive(const base::FilePath& path) {
  if (path.empty()) {
    return false;
  }
  if (path.IsNetwork()) {
    return true;
  }
  base::FilePath root_path = path;
  while (!root_path.DirName().empty() && root_path.DirName() != root_path) {
    root_path = root_path.DirName();
  }
  return ::GetDriveType(root_path.value().c_str()) == DRIVE_REMOTE;
}

// Records the step at which installation of `inner_crx_path` for the component
// identified by `histogram_tag` ended, the code that gives the details of that
// step, how long installation took, and whether the inner CRX is on a network
// drive.
void RecordOutcome(std::string_view histogram_tag,
                   const base::FilePath& inner_crx_path,
                   const ProductComponentInstallOutcome& outcome,
                   base::TimeDelta duration) {
  const std::string_view install_level =
      install_static::IsSystemInstall() ? "SystemLevel" : "UserLevel";
  const std::string prefix =
      base::StrCat({"ComponentUpdater.ProductComponent.", histogram_tag, "."});
  base::UmaHistogramEnumeration(
      base::StrCat({prefix, "InstallStep.", install_level}), outcome.step);
  const std::string_view step_name = GetStepHistogramName(outcome.step);
  if (!step_name.empty()) {
    base::UmaHistogramSparse(
        base::StrCat({prefix, "InstallCode.", step_name, ".", install_level}),
        outcome.extra_code);
  }
  base::UmaHistogramMediumTimes(
      base::StrCat({prefix, "InstallDuration.", install_level}), duration);
  base::UmaHistogramBoolean(
      base::StrCat({prefix, "InnerCrxOnNetworkDrive.", install_level}),
      IsPathOnNetworkDrive(inner_crx_path));
}

// Installs the component for system-level Chrome via elevated Google Update
// AppCommand (IAppCommandWeb). The "install-component" command is used if
// `canonical_udd` is empty. Otherwise, the "install-component-for-user" command
// is used to install the component for `canonical_udd` on behalf of the calling
// user, whose SID the updater provides to setup.exe.
ProductComponentInstallOutcome InstallSystemLevel(
    const base::FilePath& inner_crx,
    const base::FilePath& canonical_udd,
    ProductComponentInstallerDelegate* delegate) {
  TRACE_EVENT("update_client", "InstallProductComponent::InstallSystemLevel");
  auto app_command_expected = delegate->GetAppCommand(
      canonical_udd.empty() ? installer::kCmdInstallComponent
                            : installer::kCmdInstallComponentForUser);
  if (!app_command_expected.has_value()) {
    return {.step = Step::kGetAppCommandFailed,
            .extra_code = static_cast<int>(app_command_expected.error())};
  }

  Microsoft::WRL::ComPtr<IAppCommandWeb> app_command =
      std::move(app_command_expected.value());
  base::win::ScopedVariant inner_crx_var(inner_crx.value().c_str());
  // The updater stops collecting substitutions at the first empty VARIANT, so
  // only %1 is provided to "install-component".
  base::win::ScopedVariant udd_var;
  if (!canonical_udd.empty()) {
    udd_var.Set(canonical_udd.value().c_str());
  }
  const VARIANT& empty = base::win::ScopedVariant::kEmptyVariant;
  HRESULT hr = app_command->execute(inner_crx_var, udd_var, empty, empty, empty,
                                    empty, empty, empty, empty);
  if (FAILED(hr)) {
    return {.step = Step::kComError, .extra_code = static_cast<int>(hr)};
  }

  UINT status = 0;
  while (true) {
    hr = app_command->get_status(&status);
    if (FAILED(hr)) {
      return {.step = Step::kComError, .extra_code = static_cast<int>(hr)};
    }
    if (status == COMMAND_STATUS_ERROR) {
      return {.step = Step::kLaunchFailed};
    }
    if (status == COMMAND_STATUS_COMPLETE) {
      break;
    }
    base::PlatformThread::Sleep(base::Seconds(1));
  }

  DWORD exit_code = 0;
  hr = app_command->get_exitCode(&exit_code);
  if (FAILED(hr)) {
    return {.step = Step::kComError, .extra_code = static_cast<int>(hr)};
  }

  return {.step = Step::kInstallerExited,
          .extra_code = static_cast<int>(exit_code)};
}

// Installs the component for per-user Chrome by directly executing child
// setup.exe.
ProductComponentInstallOutcome InstallUserLevel(
    const base::FilePath& inner_crx,
    const base::FilePath& canonical_udd,
    ProductComponentInstallerDelegate* delegate) {
  TRACE_EVENT("update_client", "InstallProductComponent::InstallUserLevel");
  installer::ProductState product_state;
  if (!product_state.Initialize(/*system_install=*/false)) {
    return {.step = Step::kNotInstalled};
  }
  base::FilePath setup_path = product_state.GetSetupPath();
  if (setup_path.empty()) {
    return {.step = Step::kNotInstalled};
  }

  base::CommandLine cmd(setup_path);
  cmd.AppendSwitchPath(installer::switches::kInstallComponent, inner_crx);
  if (!canonical_udd.empty()) {
    cmd.AppendSwitchPath(installer::switches::kUdd, canonical_udd);
  }
  cmd.AppendSwitch(installer::switches::kVerboseLogging);
  InstallUtil::AppendModeAndChannelSwitches(&cmd);

  base::LaunchOptions options;
  options.start_hidden = true;
  ::SetLastError(ERROR_SUCCESS);
  base::Process process = delegate->LaunchProcess(cmd, options);
  if (!process.IsValid()) {
    return {.step = Step::kLaunchFailed,
            .extra_code = static_cast<int>(::GetLastError())};
  }

  ScopedAllowWaitForProductComponentInstallExit allow_wait_for_exit;
  int exit_code = 0;
  if (!process.WaitForExit(&exit_code)) {
    return {.step = Step::kWaitFailed,
            .extra_code = static_cast<int>(::GetLastError())};
  }
  return {.step = Step::kInstallerExited, .extra_code = exit_code};
}

ProductComponentInstallOutcome InstallProductComponentImpl(
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir,
    ProductComponentInstallerDelegate* delegate) {
  if (!base::PathExists(inner_crx_path)) {
    return {.step = Step::kInvalidInput};
  }
  // Setup treats the User Data directory as an opaque string from which it
  // derives where to install the component, so it must be canonicalized here
  // exactly as the browser does when deriving where to find the component.
  base::FilePath canonical_udd;
  if (!user_data_dir.empty()) {
    canonical_udd = child_module::CanonicalizeUserDataDir(user_data_dir);
    if (canonical_udd.empty()) {
      return {.step = Step::kInvalidInput};
    }
  }
  return install_static::IsSystemInstall()
             ? InstallSystemLevel(inner_crx_path, canonical_udd, delegate)
             : InstallUserLevel(inner_crx_path, canonical_udd, delegate);
}

ProductComponentInstallOutcome InstallAndRecordOutcome(
    std::string_view histogram_tag,
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir,
    ProductComponentInstallerDelegate* delegate) {
  const base::ElapsedTimer timer;
  const ProductComponentInstallOutcome outcome =
      InstallProductComponentImpl(inner_crx_path, user_data_dir, delegate);
  RecordOutcome(histogram_tag, inner_crx_path, outcome, timer.Elapsed());
  return outcome;
}

}  // namespace

ProductComponentInstallOutcome InstallProductComponent(
    std::string_view histogram_tag,
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir) {
  ProductComponentInstallerDelegate default_delegate;
  return InstallAndRecordOutcome(histogram_tag, inner_crx_path, user_data_dir,
                                 &default_delegate);
}

ProductComponentInstallOutcome InstallProductComponentForTesting(
    std::string_view histogram_tag,
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir,
    ProductComponentInstallerDelegate* delegate) {
  CHECK(delegate);
  return InstallAndRecordOutcome(histogram_tag, inner_crx_path, user_data_dir,
                                 delegate);
}

}  // namespace component_updater
