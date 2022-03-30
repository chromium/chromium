// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/component_updater/component_installer.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace component_updater {

// Component installer policy for the PKIMetadata component. This component
// includes any dynamically updateable needed for PKI policies enforcement.
// Initially this contains the Certificate Transparency log list.
class PKIMetadataComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  PKIMetadataComponentInstallerPolicy();
  PKIMetadataComponentInstallerPolicy(
      const PKIMetadataComponentInstallerPolicy&) = delete;
  PKIMetadataComponentInstallerPolicy operator=(
      const PKIMetadataComponentInstallerPolicy&) = delete;
  ~PKIMetadataComponentInstallerPolicy() override;

  // Sets the PKI metadata configuration on the current network service. This is
  // a no-op if the component is not ready.
  static void ReconfigureAfterNetworkRestart();

  // Converts a protobuf repeated bytes array to an array of uint8_t arrays.
  // Exposed for testing.
  static std::vector<std::vector<uint8_t>> BytesArrayFromProtoBytes(
      google::protobuf::RepeatedPtrField<std::string> proto_bytes);

  static void WriteComponentForTesting(const base::FilePath& path,
                                       std::string contents);

 private:
  // ComponentInstallerPolicy methods:
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

void MaybeRegisterPKIMetadataComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PKI_METADATA_COMPONENT_INSTALLER_H_
