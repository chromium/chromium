// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SUBRESOURCE_FILTER_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SUBRESOURCE_FILTER_COMPONENT_INSTALLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

// Component for receiving Safe Browsing Subresource filtering rules.
class SubresourceFilterComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  static const char kManifestRulesetFormatKey[];
  static const int kCurrentRulesetFormat;

  SubresourceFilterComponentInstallerPolicy();

  SubresourceFilterComponentInstallerPolicy(
      const SubresourceFilterComponentInstallerPolicy&) = delete;
  SubresourceFilterComponentInstallerPolicy& operator=(
      const SubresourceFilterComponentInstallerPolicy&) = delete;

  ~SubresourceFilterComponentInstallerPolicy() override;

 private:
  friend class SubresourceFilterComponentInstallerTest;
  FRIEND_TEST_ALL_PREFIXES(SubresourceFilterComponentInstallerTest,
                           InstallerTag);

  static std::string GetInstallerTag();

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
};

void RegisterSubresourceFilterComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SUBRESOURCE_FILTER_COMPONENT_INSTALLER_H_
