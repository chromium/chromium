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
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Version;
}  // namespace base

namespace component_updater {

class PlatformRuntimeComponentInstallerTest;

inline constexpr char kPlatformRuntimeLastInstallTime[] =
    "platform_runtime.last_install_time";
inline constexpr char kPlatformRuntimeLastInstalledVersion[] =
    "platform_runtime.last_installed_version";

BASE_DECLARE_FEATURE(kEnablePlatformRuntimeComponent);

class PlatformRuntimeComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  PlatformRuntimeComponentInstallerPolicy() = default;
  PlatformRuntimeComponentInstallerPolicy(
      const PlatformRuntimeComponentInstallerPolicy&) = delete;
  PlatformRuntimeComponentInstallerPolicy& operator=(
      const PlatformRuntimeComponentInstallerPolicy&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  static void UpdateOnDemand(ComponentUpdateService* cus,
                             const std::string& id,
                             OnDemandUpdater::Priority priority);

  static bool ShouldTriggerInstallOrUpdate(ComponentUpdateService* cus,
                                           PrefService* local_state,
                                           const std::string& crx_id);

  void ComponentReadyForTesting(const base::Version& version,
                                const base::FilePath& install_dir,
                                base::DictValue manifest) {
    ComponentReady(version, install_dir, std::move(manifest));
  }

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
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

void MaybeRegisterPlatformRuntimeComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PLATFORM_RUNTIME_COMPONENT_INSTALLER_H_
