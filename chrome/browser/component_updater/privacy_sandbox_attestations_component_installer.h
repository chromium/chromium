// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PRIVACY_SANDBOX_ATTESTATIONS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PRIVACY_SANDBOX_ATTESTATIONS_COMPONENT_INSTALLER_H_

#include <cstdint>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/update_client/update_client.h"

namespace base {
class File;
class FilePath;
class Version;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

class PrivacySandboxAttestationsComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  using AttestationsReadyRepeatingCallback =
      base::RepeatingCallback<void(base::Version, base::File)>;

  // Once the attestations file is ready, `ComponentReady` will be invoked on
  // the UI thread, which in turn invokes `on_attestations_ready_`.
  explicit PrivacySandboxAttestationsComponentInstallerPolicy(
      AttestationsReadyRepeatingCallback on_attestations_ready);
  ~PrivacySandboxAttestationsComponentInstallerPolicy() override;

  PrivacySandboxAttestationsComponentInstallerPolicy(
      const PrivacySandboxAttestationsComponentInstallerPolicy&) = delete;
  PrivacySandboxAttestationsComponentInstallerPolicy operator=(
      const PrivacySandboxAttestationsComponentInstallerPolicy&) = delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxAttestationsInstallerFeatureDisabledTest,
      DeleteExistingFilesIfFeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxAttestationsInstallerFeatureEnabledTest,
      LoadAttestationsFileOnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxAttestationsInstallerFeatureEnabledTest,
      NonexistentFileOnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxAttestationsInstallerFeatureEnabledTest,
      LoadsNewerVersionOnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(
      PrivacySandboxAttestationsInstallerFeatureEnabledTest,
      LoadNewAttestationsFileWhenUpdated);

  // The following methods override `ComponentInstallerPolicy`.
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

  static base::FilePath GetInstalledDirectory(const base::FilePath& base);
  static base::FilePath GetInstalledPath(const base::FilePath& base);

  // Upon newer version of attestations file becomes ready, the callback is
  // invoked to update the existing in-memory attestations map. So this is a
  // repeating callback.
  AttestationsReadyRepeatingCallback on_attestations_ready_;
};

// Call once during startup to make the component update service aware of
// the Privacy Sandbox Attestations component.
void RegisterPrivacySandboxAttestationsComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PRIVACY_SANDBOX_ATTESTATIONS_COMPONENT_INSTALLER_H_
