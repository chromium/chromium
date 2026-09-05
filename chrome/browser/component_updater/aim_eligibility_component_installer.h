// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_AIM_ELIGIBILITY_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_AIM_ELIGIBILITY_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

class PrefRegistrySimple;

namespace component_updater {

class ComponentUpdateService;

class AimEligibilityComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  AimEligibilityComponentInstallerPolicy();
  ~AimEligibilityComponentInstallerPolicy() override;

  AimEligibilityComponentInstallerPolicy(
      const AimEligibilityComponentInstallerPolicy&) = delete;
  AimEligibilityComponentInstallerPolicy& operator=(
      const AimEligibilityComponentInstallerPolicy&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // ComponentInstallerPolicy:
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

// Manages registration and uninstallation of the component.
void ManageAimEligibilityComponentRegistration(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_AIM_ELIGIBILITY_COMPONENT_INSTALLER_H_
