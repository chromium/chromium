// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_VR_ASSETS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_VR_ASSETS_COMPONENT_INSTALLER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class VrAssetsComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  VrAssetsComponentInstallerPolicy() = default;
  ~VrAssetsComponentInstallerPolicy() override = default;

 private:
  static bool ShouldRegisterVrAssetsComponentOnStartup();
  static void RegisterComponent(ComponentUpdateService* cus);
  static void UpdateComponent(ComponentUpdateService* cus);
  static void OnRegisteredComponent(ComponentUpdateService* cus);

  // ComponentInstallerPolicy:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static bool registered_component_;
  static bool registration_pending_;
  static bool ondemand_update_pending_;

  friend bool ShouldRegisterVrAssetsComponentOnStartup();
  friend void RegisterVrAssetsComponent(ComponentUpdateService* cus);
  friend void UpdateVrAssetsComponent(ComponentUpdateService* cus);

  DISALLOW_COPY_AND_ASSIGN(VrAssetsComponentInstallerPolicy);
};

// Returns true if the assets component should be registered at startup.
bool ShouldRegisterVrAssetsComponentOnStartup();

// Call once to make the component update service aware of the VR Assets
// component.
void RegisterVrAssetsComponent(ComponentUpdateService* cus);

// Update VR assets component immediately. The component must be registered
// before calling this function.
void UpdateVrAssetsComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_VR_ASSETS_COMPONENT_INSTALLER_H_
