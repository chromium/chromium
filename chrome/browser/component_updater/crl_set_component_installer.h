// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CRL_SET_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CRL_SET_COMPONENT_INSTALLER_H_

#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class CRLSetPolicy : public ComponentInstallerPolicy {
 public:
  CRLSetPolicy();
  CRLSetPolicy(const CRLSetPolicy&) = delete;
  CRLSetPolicy& operator=(const CRLSetPolicy&) = delete;
  ~CRLSetPolicy() override;

 private:
  friend class CRLSetComponentInstallerTest;

  // ComponentInstallerPolicy implementation.
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
};

// Registers a CRLSet component with |cus|. On a new CRLSet update, the default
// Network Service, returned by content::GetNetworkService(), will be updated
// with the new CRLSet.
void RegisterCRLSetComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CRL_SET_COMPONENT_INSTALLER_H_
