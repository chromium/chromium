// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_STH_SET_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_STH_SET_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/component_updater/component_installer.h"

namespace network {
namespace mojom {
class NetworkService;
}  // namespace mojom
}  // namespace network

namespace component_updater {

class ComponentUpdateService;

// Component for receiving Signed Tree Heads updates for Certificate
// Transparency logs recognized in Chrome.
// The STHs are in JSON format.
// To identify the log each STH belongs to, the name of the file is
// hex-encoded Log ID of the log that produced this STH.
//
// Notifications of each of the new STHs are sent to the
// certificate_transparency::STHObserver, on the same task runner that this
// object is created, so that it can take appropriate steps, including possible
// persistence.
class STHSetComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  STHSetComponentInstallerPolicy();
  ~STHSetComponentInstallerPolicy() override;

  // Update the STHs after a network process crash.
  static void ReconfigureAfterNetworkRestart();

 private:
  friend class STHSetComponentInstallerTest;
  void SetNetworkServiceForTesting(
      network::mojom::NetworkService* network_service);

  // ComponentInstallerPolicy implementation.
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

  static void ConfigureNetworkService();

  DISALLOW_COPY_AND_ASSIGN(STHSetComponentInstallerPolicy);
};

void RegisterSTHSetComponent(ComponentUpdateService* cus,
                             const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_STH_SET_COMPONENT_INSTALLER_H_
