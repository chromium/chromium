// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_HYPHENATION_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_HYPHENATION_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class HyphenationComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  HyphenationComponentInstallerPolicy();
  ~HyphenationComponentInstallerPolicy() override;

  HyphenationComponentInstallerPolicy(
      const HyphenationComponentInstallerPolicy&) = delete;
  HyphenationComponentInstallerPolicy operator=(
      const HyphenationComponentInstallerPolicy&) = delete;

  static void GetHyphenationDictionary(
      base::OnceCallback<void(const base::FilePath&)> callback);

 private:
  // The following methods override ComponentInstallerPolicy.
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
// the First-Party Sets component.
void RegisterHyphenationComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_HYPHENATION_COMPONENT_INSTALLER_H_
