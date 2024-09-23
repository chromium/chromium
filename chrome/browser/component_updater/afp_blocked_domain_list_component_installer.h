// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_AFP_BLOCKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_AFP_BLOCKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class AntiFingerprintingBlockedDomainListComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  static const char kManifestRulesetFormatKey[];
  static const int kCurrentRulesetFormat;

  AntiFingerprintingBlockedDomainListComponentInstallerPolicy();

  AntiFingerprintingBlockedDomainListComponentInstallerPolicy(
      const AntiFingerprintingBlockedDomainListComponentInstallerPolicy&) =
      delete;
  AntiFingerprintingBlockedDomainListComponentInstallerPolicy& operator=(
      const AntiFingerprintingBlockedDomainListComponentInstallerPolicy&) =
      delete;
  ~AntiFingerprintingBlockedDomainListComponentInstallerPolicy() override;

 private:
  friend class AntiFingerprintingBlockedDomainListComponentInstallerTest;
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

// Called once during startup to make the component update service aware of
// the Component, if it is enabled.
void RegisterAntiFingerprintingBlockedDomainListComponent(
    ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_AFP_BLOCKED_DOMAIN_LIST_COMPONENT_INSTALLER_H_
