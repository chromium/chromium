// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_MEDIA_FOUNDATION_WIDEVINE_CDM_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_MEDIA_FOUNDATION_WIDEVINE_CDM_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

class ComponentUpdateService;

class MediaFoundationWidevineCdmComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  MediaFoundationWidevineCdmComponentInstallerPolicy() = default;
  MediaFoundationWidevineCdmComponentInstallerPolicy(
      MediaFoundationWidevineCdmComponentInstallerPolicy& other) = delete;
  MediaFoundationWidevineCdmComponentInstallerPolicy& operator=(
      const MediaFoundationWidevineCdmComponentInstallerPolicy& other) = delete;
  ~MediaFoundationWidevineCdmComponentInstallerPolicy() override = default;

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

void RegisterMediaFoundationWidevineCdmComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_MEDIA_FOUNDATION_WIDEVINE_CDM_COMPONENT_INSTALLER_H_
