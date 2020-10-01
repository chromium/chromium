// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_FLOC_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_FLOC_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "components/component_updater/component_installer.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace federated_learning {
class FlocBlocklistService;
class FlocSortingLshClustersService;
}  // namespace federated_learning

namespace component_updater {

class ComponentUpdateService;

// Component for receiving FLoC files, e.g. blocklist, sorting-lsh, etc.
class FlocComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit FlocComponentInstallerPolicy(
      federated_learning::FlocBlocklistService* floc_blocklist_service,
      federated_learning::FlocSortingLshClustersService*
          floc_sorting_lsh_clusters_service);
  ~FlocComponentInstallerPolicy() override;

  FlocComponentInstallerPolicy(const FlocComponentInstallerPolicy&) = delete;
  FlocComponentInstallerPolicy& operator=(const FlocComponentInstallerPolicy&) =
      delete;

 private:
  friend class FlocComponentInstallerTest;

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

  federated_learning::FlocBlocklistService* floc_blocklist_service_;
  federated_learning::FlocSortingLshClustersService*
      floc_sorting_lsh_clusters_service_;
};

void RegisterFlocComponent(
    ComponentUpdateService* cus,
    federated_learning::FlocBlocklistService* floc_blocklist_service,
    federated_learning::FlocSortingLshClustersService*
        floc_sorting_lsh_clusters_service);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_FLOC_COMPONENT_INSTALLER_H_
