// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CRL_SET_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CRL_SET_COMPONENT_INSTALLER_H_

#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace network {
namespace mojom {
class NetworkService;
}  // namespace mojom
}  // namespace network

namespace component_updater {

class ComponentUpdateService;

class CRLSetPolicy : public ComponentInstallerPolicy {
 public:
  CRLSetPolicy();
  CRLSetPolicy(const CRLSetPolicy&) = delete;
  CRLSetPolicy& operator=(const CRLSetPolicy&) = delete;
  ~CRLSetPolicy() override;

  // Queues a task to reconfigure the network service returned by
  // content::GetNetworkService() (or configured by
  // SetNetworkServiceForTesting) after the Network Service instance has
  // changed (i.e. as signaled by
  // content::ContentBrowserClient::OnNetworkServiceCreated).
  static void ReconfigureAfterNetworkRestart();

 private:
  friend class CRLSetComponentInstallerTest;

  // Configures the CRLSet component to send updates to |network_service|
  // instead of the network service provided by |content::GetNetworkService()|.
  static void SetNetworkServiceForTesting(
      network::mojom::NetworkService* network_service);

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
