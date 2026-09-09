// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PLATFORM_RUNTIME_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PLATFORM_RUNTIME_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"

#if BUILDFLAG(IS_WIN)
#include <wrl/client.h>

#include "base/process/process.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"

struct IAppCommandWeb;
#endif

class PrefRegistrySimple;
class PrefService;

namespace base {
class CommandLine;
struct LaunchOptions;
class Version;
}  // namespace base

namespace component_updater {

// Local state preference storing the release/build timestamp of the currently
// installed Platform Runtime component.
inline constexpr char kPlatformRuntimeLastReleaseTime[] =
    "platform_runtime.last_release_time";
inline constexpr char kPlatformRuntimeLastInstalledVersion[] =
    "platform_runtime.last_installed_version";

// These values are persisted to UMA logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PlatformRuntimeInstallTrigger {
  kMissing = 0,
  kStale = 1,
  kBackground = 2,
  kMaxValue = kBackground,
};

// Persisted to UMA logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class PlatformRuntimeInstallationResult {
  kSuccess = 0,
  kAlreadyExists = 1,
  kInnerCrxNotFound = 2,
  kCommandNotFound = 3,
  kLaunchFailed = 4,
  kFailedInternal = 5,
  kFailedSignature = 6,
  kFailedInvalidInput = 7,
  kFailedOther = 8,
  kFailedInternalManagedDevice = 9,
  kFailedInternalNetworkPath = 10,
  kFailedInternalManagedDeviceAndNetworkPath = 11,
  kFailedComAccessDenied = 12,
  kFailedComServerDied = 13,
  kFailedComInvalidArg = 14,
  kFailedComUnexpected = 15,
  kFailedComOther = 16,
  kNotInstalled = 17,
  kMaxValue = kNotInstalled,
};

BASE_DECLARE_FEATURE(kEnablePlatformRuntimeComponent);

#if BUILDFLAG(IS_WIN)
// Delegate interface to abstract external Windows installer mechanisms
// Allows unit tests to mock external system interactions without touching the
// live system or launching real processes.
class PlatformRuntimeInstallerDelegate {
 public:
  virtual ~PlatformRuntimeInstallerDelegate() = default;

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

class PlatformRuntimeComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  PlatformRuntimeComponentInstallerPolicy();
  ~PlatformRuntimeComponentInstallerPolicy() override;
#if BUILDFLAG(IS_WIN)
  explicit PlatformRuntimeComponentInstallerPolicy(
      std::unique_ptr<PlatformRuntimeInstallerDelegate> delegate);
#endif
  PlatformRuntimeComponentInstallerPolicy(
      const PlatformRuntimeComponentInstallerPolicy&) = delete;
  PlatformRuntimeComponentInstallerPolicy& operator=(
      const PlatformRuntimeComponentInstallerPolicy&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  static void UpdateOnDemand(ComponentUpdateService* cus,
                             const std::string& id,
                             OnDemandUpdater::Priority priority);

  bool ShouldTriggerInstallOrUpdate(ComponentUpdateService* cus,
                                    PrefService* local_state,
                                    const std::string& crx_id);

  void ComponentReadyForTesting(const base::Version& version,
                                const base::FilePath& install_dir,
                                base::DictValue manifest) {
    ComponentReady(version, install_dir, std::move(manifest));
  }

  update_client::CrxInstaller::Result OnCustomInstallForTesting(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) {
    return OnCustomInstall(manifest, install_dir);
  }

  bool VerifyInstallationForTesting(const base::DictValue& manifest,
                                    const base::FilePath& install_dir) const {
    return VerifyInstallation(manifest, install_dir);
  }

  void SetInstallTrigger(PlatformRuntimeInstallTrigger trigger) {
    install_trigger_ = trigger;
  }

  // ComponentInstallerPolicy overrides:
  void GetHash(std::vector<uint8_t>* hash) const override;

#if BUILDFLAG(IS_WIN)
  // Friend and derived class of ScopedAllowBaseSyncPrimitives which allows
  // InstallUserLevel() to wait on setup.exe.
  class [[maybe_unused, nodiscard]] ScopedAllowWaitForExit
      : public base::ScopedAllowBaseSyncPrimitives {};
#endif

 private:
  // ComponentInstallerPolicy overrides:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  PlatformRuntimeInstallTrigger install_trigger_ =
      PlatformRuntimeInstallTrigger::kBackground;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<PlatformRuntimeInstallerDelegate> installer_delegate_;
#endif
};

void MaybeRegisterPlatformRuntimeComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PLATFORM_RUNTIME_COMPONENT_INSTALLER_H_
