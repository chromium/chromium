// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PAYLOAD_TEST_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PAYLOAD_TEST_COMPONENT_INSTALLER_H_

#include "components/component_updater/component_installer.h"

namespace component_updater {

class ComponentUpdateService;

class PayloadTestComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  PayloadTestComponentInstallerPolicy() = default;
  ~PayloadTestComponentInstallerPolicy() override = default;

  // Overrides for ComponentInstallerPolicy.
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
  bool AllowCachedCopies() const override;
};

void RegisterPayloadTestComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PAYLOAD_TEST_COMPONENT_INSTALLER_H_
