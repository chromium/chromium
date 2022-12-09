// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_THIRD_PARTY_MODULE_LIST_COMPONENT_INSTALLER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_THIRD_PARTY_MODULE_LIST_COMPONENT_INSTALLER_WIN_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

class ComponentUpdateService;

// Component for receiving Third Party Module Lists. The lists are in proto
// format, corresponding to the proto definition in
// chrome/browser/win/conflicts/proto/module_list.proto
//
// Notifications of a new version of the module list are sent to the
// ThirdPartyConflictsManager instance in the ModuleDatabase, if it exists.
class ThirdPartyModuleListComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  ThirdPartyModuleListComponentInstallerPolicy();

  ThirdPartyModuleListComponentInstallerPolicy(
      const ThirdPartyModuleListComponentInstallerPolicy&) = delete;
  ThirdPartyModuleListComponentInstallerPolicy& operator=(
      const ThirdPartyModuleListComponentInstallerPolicy&) = delete;

  ~ThirdPartyModuleListComponentInstallerPolicy() override;

 private:
  // ComponentInstallerPolicy implementation.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  // Returns the path to the proto file for the given |install_dir|.
  static base::FilePath GetModuleListPath(const base::FilePath& install_dir);
};

void RegisterThirdPartyModuleListComponent(
    ComponentUpdateService* component_update_service);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_THIRD_PARTY_MODULE_LIST_COMPONENT_INSTALLER_WIN_H_
