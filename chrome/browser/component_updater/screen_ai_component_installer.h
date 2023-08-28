// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SCREEN_AI_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SCREEN_AI_COMPONENT_INSTALLER_H_

#include <string>

#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/update_client/update_client.h"

namespace component_updater {

class ScreenAIComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  ScreenAIComponentInstallerPolicy();
  ScreenAIComponentInstallerPolicy(const ScreenAIComponentInstallerPolicy&) =
      delete;
  ScreenAIComponentInstallerPolicy& operator=(
      const ScreenAIComponentInstallerPolicy&) = delete;
  ~ScreenAIComponentInstallerPolicy() override;

  static void DeleteComponent();

  static std::string GetOmahaId();

 private:
  // ComponentInstallerPolicy::
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
};

// Call once during startup to make the component update service aware of
// the ScreenAI component. Only registers the component if the component is
// expected to be used, otherwise removes it if it exists from before.
void ManageScreenAIComponentRegistration(ComponentUpdateService* cus,
                                         PrefService* local_state);

// Called if ScreenAI component should be installed based on a user trigger of
// a required functionality.
void RegisterScreenAIComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SCREEN_AI_COMPONENT_INSTALLER_H_
