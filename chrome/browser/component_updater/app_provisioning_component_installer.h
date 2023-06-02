// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_APP_PROVISIONING_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_APP_PROVISIONING_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class AppProvisioningComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  AppProvisioningComponentInstallerPolicy() = default;
  AppProvisioningComponentInstallerPolicy(
      const AppProvisioningComponentInstallerPolicy&) = delete;
  AppProvisioningComponentInstallerPolicy& operator=(
      const AppProvisioningComponentInstallerPolicy&) = delete;
  ~AppProvisioningComponentInstallerPolicy() override = default;

 private:
  // The following methods override ComponentInstallerPolicy.
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static base::FilePath GetAppWithLocaleInstalledPath(
      const base::FilePath& base);
};

// Call once during startup to make the component update service aware of
// the App Provisioning Policies component.
void RegisterAppProvisioningComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_APP_PROVISIONING_COMPONENT_INSTALLER_H_
