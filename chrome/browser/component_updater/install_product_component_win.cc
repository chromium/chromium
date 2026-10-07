// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/install_product_component.h"

#include <windows.h>

#include <wrl/client.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_variant.h"
#include "chrome/browser/google/google_update_app_command.h"
#include "chrome/common/child_module/child_module_helper.h"
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

namespace {

ProductComponentInstallResult MapInstallerExitCode(DWORD exit_code) {
  switch (exit_code) {
    // LINT.IfChange(MapInstallerExitCode)
    case installer::INSTALL_COMPONENT_SUCCESS:
      return ProductComponentInstallResult::kSuccess;
    case installer::INSTALL_COMPONENT_ALREADY_EXISTS:
      return ProductComponentInstallResult::kAlreadyExists;
    case installer::INSTALL_COMPONENT_FAILED_INTERNAL:
      return ProductComponentInstallResult::kFailedInternal;
    case installer::INSTALL_COMPONENT_FAILED_SIGNATURE:
      return ProductComponentInstallResult::kSignatureVerificationFailed;
    case installer::INSTALL_COMPONENT_INVALID_INPUT:
      return ProductComponentInstallResult::kFailedInvalidInput;
    // LINT.ThenChange(/chrome/installer/util/util_constants.h:InstallComponent)
    default:
      return ProductComponentInstallResult::kOtherInstallerError;
  }
}

// Maps COM and RPC HRESULT errors returned during AppCommand execution to
// ProductComponentInstallResult.
ProductComponentInstallResult MapComError(HRESULT hr) {
  switch (hr) {
    case E_ACCESSDENIED:
      return ProductComponentInstallResult::kFailedComAccessDenied;
    case RPC_E_SERVER_DIED:
    case RPC_E_DISCONNECTED:
    case HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE):
      return ProductComponentInstallResult::kFailedComServerDied;
    case E_INVALIDARG:
      return ProductComponentInstallResult::kFailedComInvalidArg;
    case E_UNEXPECTED:
      return ProductComponentInstallResult::kFailedComUnexpected;
    default:
      return ProductComponentInstallResult::kFailedComOther;
  }
}

// Installs the component for system-level Chrome via elevated Google Update
// AppCommand (IAppCommandWeb). The "install-component" command is used if
// `canonical_udd` is empty. Otherwise, the "install-component-for-user" command
// is used to install the component for `canonical_udd` on behalf of the calling
// user, whose SID the updater provides to setup.exe.
ProductComponentInstallResult InstallSystemLevel(
    const base::FilePath& inner_crx,
    const base::FilePath& canonical_udd,
    ProductComponentInstallerDelegate* delegate) {
  TRACE_EVENT("update_client", "InstallProductComponent::InstallSystemLevel");
  auto app_command_expected = delegate->GetAppCommand(
      canonical_udd.empty() ? installer::kCmdInstallComponent
                            : installer::kCmdInstallComponentForUser);
  if (!app_command_expected.has_value()) {
    return ProductComponentInstallResult::kCommandNotFound;
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
    return MapComError(hr);
  }

  UINT status = 0;
  while (true) {
    hr = app_command->get_status(&status);
    if (FAILED(hr)) {
      return MapComError(hr);
    }
    if (status == COMMAND_STATUS_ERROR) {
      return ProductComponentInstallResult::kLaunchFailed;
    }
    if (status == COMMAND_STATUS_COMPLETE) {
      break;
    }
    base::PlatformThread::Sleep(base::Seconds(1));
  }

  DWORD exit_code = 0;
  hr = app_command->get_exitCode(&exit_code);
  if (FAILED(hr)) {
    return MapComError(hr);
  }

  return MapInstallerExitCode(exit_code);
}

// Installs the component for per-user Chrome by directly executing child
// setup.exe.
ProductComponentInstallResult InstallUserLevel(
    const base::FilePath& inner_crx,
    const base::FilePath& canonical_udd,
    ProductComponentInstallerDelegate* delegate) {
  TRACE_EVENT("update_client", "InstallProductComponent::InstallUserLevel");
  installer::ProductState product_state;
  if (!product_state.Initialize(/*system_install=*/false)) {
    return ProductComponentInstallResult::kNotInstalled;
  }
  base::FilePath setup_path = product_state.GetSetupPath();
  if (setup_path.empty()) {
    return ProductComponentInstallResult::kCommandNotFound;
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
    const DWORD error_code = ::GetLastError();
    if (error_code == ERROR_FILE_NOT_FOUND ||
        error_code == ERROR_PATH_NOT_FOUND) {
      return ProductComponentInstallResult::kCommandNotFound;
    }
    return ProductComponentInstallResult::kLaunchFailed;
  }

  ScopedAllowWaitForProductComponentInstallExit allow_wait_for_exit;
  int exit_code = 0;
  if (!process.WaitForExit(&exit_code)) {
    return ProductComponentInstallResult::kLaunchFailed;
  }
  return MapInstallerExitCode(static_cast<DWORD>(exit_code));
}

ProductComponentInstallResult InstallProductComponentImpl(
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir,
    ProductComponentInstallerDelegate* delegate) {
  if (!base::PathExists(inner_crx_path)) {
    return ProductComponentInstallResult::kFailedInvalidInput;
  }
  // Setup treats the User Data directory as an opaque string from which it
  // derives where to install the component, so it must be canonicalized here
  // exactly as the browser does when deriving where to find the component.
  base::FilePath canonical_udd;
  if (!user_data_dir.empty()) {
    canonical_udd = child_module::CanonicalizeUserDataDir(user_data_dir);
    if (canonical_udd.empty()) {
      return ProductComponentInstallResult::kFailedInvalidInput;
    }
  }
  return install_static::IsSystemInstall()
             ? InstallSystemLevel(inner_crx_path, canonical_udd, delegate)
             : InstallUserLevel(inner_crx_path, canonical_udd, delegate);
}

}  // namespace

ProductComponentInstallResult InstallProductComponent(
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir) {
  ProductComponentInstallerDelegate default_delegate;
  return InstallProductComponentImpl(inner_crx_path, user_data_dir,
                                     &default_delegate);
}

ProductComponentInstallResult InstallProductComponentForTesting(
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir,
    ProductComponentInstallerDelegate* delegate) {
  CHECK(delegate);
  return InstallProductComponentImpl(inner_crx_path, user_data_dir, delegate);
}

}  // namespace component_updater
