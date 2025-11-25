// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_FASTPUSH_COMPONENT_INSTALLER_POLICY_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_FASTPUSH_COMPONENT_INSTALLER_POLICY_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

// Component installer policy for the PKIMetadataFastpush component. This
// component goes along with PKIMetadata component, but includes data that
// needs lower update latency.
class PKIMetadataFastpushComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  PKIMetadataFastpushComponentInstallerPolicy();
  PKIMetadataFastpushComponentInstallerPolicy(
      const PKIMetadataFastpushComponentInstallerPolicy&) = delete;
  PKIMetadataFastpushComponentInstallerPolicy operator=(
      const PKIMetadataFastpushComponentInstallerPolicy&) = delete;
  ~PKIMetadataFastpushComponentInstallerPolicy() override;

 private:
  // ComponentInstallerPolicy methods:
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

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_FASTPUSH_COMPONENT_INSTALLER_POLICY_H_
