// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_INSTALL_PRODUCT_COMPONENT_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_INSTALL_PRODUCT_COMPONENT_H_

#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <wrl/client.h>

#include "base/process/process.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"

struct IAppCommandWeb;

namespace base {
class CommandLine;
struct LaunchOptions;
}  // namespace base
#endif  // BUILDFLAG(IS_WIN)

namespace component_updater {

// Result of installing a product component payload.
enum class ProductComponentInstallResult {
  kSuccess = 0,
  kAlreadyExists = 1,
  kLaunchFailed = 2,
  kSignatureVerificationFailed = 3,
  kFailedInvalidInput = 4,
  kIncompatibleBaseVersion = 5,
  kOtherInstallerError = 6,
  kCommandNotFound = 7,
  kNotInstalled = 8,
  kFailedInternal = 9,
  kFailedComAccessDenied = 10,
  kFailedComServerDied = 11,
  kFailedComInvalidArg = 12,
  kFailedComUnexpected = 13,
  kFailedComOther = 14,
};

#if BUILDFLAG(IS_WIN)
// Delegate interface to abstract external Windows installer mechanisms.
// Allows unit tests to mock external system interactions without touching the
// live system or launching real processes.
class ProductComponentInstallerDelegate {
 public:
  virtual ~ProductComponentInstallerDelegate() = default;

  // Retrieves the Google Update AppCommand COM interface (IAppCommandWeb) for
  // the given `command_name`. Used for elevated system-level installations.
  virtual base::expected<Microsoft::WRL::ComPtr<IAppCommandWeb>, HRESULT>
  GetAppCommand(const std::wstring& command_name);

  // Launches an external child process with the given `cmd` and `options`.
  // Used for per-user setup.exe component installation.
  virtual base::Process LaunchProcess(const base::CommandLine& cmd,
                                      const base::LaunchOptions& options);
};
#endif  // BUILDFLAG(IS_WIN)

// Synchronously installs a product component payload (.crx3) into the Chrome
// installation directory. If `user_data_dir` is not empty, the component is
// installed on behalf of the current user and `user_data_dir` (e.g., a dynamic
// patch). `user_data_dir` is canonicalized via
// `child_module::CanonicalizeUserDataDir()`; kFailedInvalidInput is returned if
// this fails.
//
// - Windows (per-machine): Invokes Google Update's elevated IAppCommandWeb
//   ("install-component", or "install-component-for-user" if `user_data_dir`
//   is not empty), passing the inner CRX path and canonicalized UDD path, and
//   polls until the command completes.
// - Windows (per-user): Invokes setup.exe directly with the inner CRX path and
//   canonicalized UDD path and waits for it to exit.
// - Non-Windows: Returns kLaunchFailed until platform-specific helpers are
//   supported.
//
// Blocks the calling thread until execution completes (base::MayBlock()).
ProductComponentInstallResult InstallProductComponent(
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir = {});

#if BUILDFLAG(IS_WIN)
// Test-only entry point allowing custom delegate injection.
ProductComponentInstallResult InstallProductComponentForTesting(
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir,
    ProductComponentInstallerDelegate* delegate);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_INSTALL_PRODUCT_COMPONENT_H_
