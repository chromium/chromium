// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_EXTENSION_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_EXTENSION_INSTALLER_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

class ComponentUpdateService;

struct ComponentExtensionInstallerConfig {
  std::string name;
  std::string extension_id;
  std::array<uint8_t, 32> public_key_sha256;
  base::FilePath relative_install_dir;
  int bundled_manifest_resource_id;
};

class ComponentExtensionInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit ComponentExtensionInstallerPolicy(
      const ComponentExtensionInstallerConfig& config);
  ~ComponentExtensionInstallerPolicy() override;

  ComponentExtensionInstallerPolicy(const ComponentExtensionInstallerPolicy&) =
      delete;
  ComponentExtensionInstallerPolicy& operator=(
      const ComponentExtensionInstallerPolicy&) = delete;

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

 private:
  const ComponentExtensionInstallerConfig config_;
};

// Manages registration and uninstallation of the components.
void ManageComponentExtensionRegistrations(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_EXTENSION_INSTALLER_H_
