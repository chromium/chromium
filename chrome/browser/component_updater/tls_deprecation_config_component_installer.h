// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TLS_DEPRECATION_CONFIG_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TLS_DEPRECATION_CONFIG_COMPONENT_INSTALLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class TLSDeprecationConfigComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  TLSDeprecationConfigComponentInstallerPolicy();
  ~TLSDeprecationConfigComponentInstallerPolicy() override;

  // Queues a task to reconfigure the network service after the Network Service
  // instance has changed (i.e., as signaled by
  // content::ContentBrowserClient::OnNetworkServiceCreated).
  static void ReconfigureAfterNetworkRestart();

 private:
  // ComponentInstallerPolicy methods:
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
  std::vector<std::string> GetMimeTypes() const override;

  DISALLOW_COPY_AND_ASSIGN(TLSDeprecationConfigComponentInstallerPolicy);
};

void RegisterTLSDeprecationConfigComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TLS_DEPRECATION_CONFIG_COMPONENT_INSTALLER_H_
