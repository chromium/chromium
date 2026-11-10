// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_INSTALL_PRODUCT_COMPONENT_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_INSTALL_PRODUCT_COMPONENT_H_

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <wrl/client.h>

#include "base/process/process.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"

struct IAppCommandWeb;

namespace base {
class CommandLine;
struct LaunchOptions;
}  // namespace base
#endif  // BUILDFLAG(IS_WIN)

namespace component_updater {

// The outcome of installing a product component payload: the step at which
// installation ended, and a code that gives the details of that step.
struct ProductComponentInstallOutcome {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. 0 is not used so that a step that
  // is reported as an update_client error code is never mistaken for success.
  // LINT.IfChange(ProductComponentInstallStep)
  enum class Step {
    // The inner CRX does not exist or the User Data directory could not be
    // canonicalized. `extra_code` is 0.
    kInvalidInput = 1,
    // The running browser is not properly installed on the device: its
    // installation state could not be read or does not name setup.exe
    // (per-user installs only). `extra_code` is 0.
    kNotInstalled = 2,
    // Google Update's App Command could not be retrieved, e.g., because the
    // updater's COM registration is broken or missing or its service is
    // disabled (per-machine installs only). `extra_code` is the HRESULT.
    kGetAppCommandFailed = 3,
    // A call to the App Command failed (per-machine installs only).
    // `extra_code` is the HRESULT.
    kComError = 4,
    // setup.exe could not be launched. `extra_code` is the Windows error code
    // (per-user), or 0 if the App Command reported an error (per-machine), in
    // which case no code is recorded.
    kLaunchFailed = 5,
    // setup.exe ran to completion. `extra_code` is its exit code: an
    // installer::InstallStatus value, or an NTSTATUS if it crashed or was
    // terminated.
    kInstallerExited = 6,
    // Waiting for setup.exe to exit failed (per-user installs only).
    // `extra_code` is the Windows error code.
    kWaitFailed = 7,
    kMaxValue = kWaitFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:ProductComponentInstallStep)

  friend bool operator==(const ProductComponentInstallOutcome&,
                         const ProductComponentInstallOutcome&) = default;

  // Returns true if setup.exe installed the component or reported that it is
  // already installed.
  bool Succeeded() const;

  Step step = Step::kInvalidInput;
  int extra_code = 0;
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

  // Blocks the calling thread for `delay` before installation is retried
  // because another setup.exe operation was in progress.
  virtual void WaitBeforeRetry(base::TimeDelta delay);
};
#endif  // BUILDFLAG(IS_WIN)

// Synchronously installs a product component payload (.crx3) into the Chrome
// installation directory. If `user_data_dir` is not empty, the component is
// installed on behalf of the current user and `user_data_dir` (e.g., a dynamic
// patch). `user_data_dir` is canonicalized via
// `child_module::CanonicalizeUserDataDir()`; installation ends at kInvalidInput
// if this fails.
//
// - Windows (per-machine): Invokes Google Update's elevated IAppCommandWeb
//   ("install-component", or "install-component-for-user" if `user_data_dir`
//   is not empty), passing the inner CRX path and canonicalized UDD path, and
//   polls until the command completes.
// - Windows (per-user): Invokes setup.exe directly with the inner CRX path and
//   canonicalized UDD path and waits for it to exit.
// - Non-Windows: Ends at kLaunchFailed until platform-specific helpers are
//   supported.
//
// On Windows, if setup.exe cannot acquire exclusive access to the installation
// because another setup.exe operation (e.g., a browser update or another
// component installation) is in progress, installation is retried a few times
// after increasing delays before giving up.
//
// The outcome, the duration of the installation, and whether the inner CRX is
// on a network drive are recorded in the
// ComponentUpdater.ProductComponent.<histogram_tag>.* histograms, where
// `histogram_tag` names the component (e.g., "PlatformRuntime") so that the
// outcomes of different components are recorded separately.
//
// Blocks the calling thread until execution completes (base::MayBlock()).
ProductComponentInstallOutcome InstallProductComponent(
    std::string_view histogram_tag,
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir = {});

#if BUILDFLAG(IS_WIN)
// Test-only entry point allowing custom delegate injection.
ProductComponentInstallOutcome InstallProductComponentForTesting(
    std::string_view histogram_tag,
    const base::FilePath& inner_crx_path,
    const base::FilePath& user_data_dir,
    ProductComponentInstallerDelegate* delegate);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_INSTALL_PRODUCT_COMPONENT_H_
