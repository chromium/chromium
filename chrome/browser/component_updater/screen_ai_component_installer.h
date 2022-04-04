// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SCREEN_AI_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SCREEN_AI_COMPONENT_INSTALLER_H_

#include <string>

#include "components/component_updater/component_installer.h"
#include "components/prefs/pref_service.h"
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

  static void DeleteLibraryOrScheduleDeletionIfNeeded(
      PrefService* global_prefs);

 private:
  // ComponentInstallerPolicy::
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

// Call once during startup to make the component update service aware of
// the ScreenAI component.
void RegisterScreenAIComponent(ComponentUpdateService* cus,
                               PrefService* global_prefs);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SCREEN_AI_COMPONENT_INSTALLER_H_
