// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CHROME_APPS_DEPRECATION_ALLOWLIST_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CHROME_APPS_DEPRECATION_ALLOWLIST_COMPONENT_INSTALLER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

class ChromeAppsDeprecationAllowlistComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  using ComponentReadyCallback =
      base::RepeatingCallback<void(const base::Version&,
                                   const base::FilePath& installed_file_path)>;

  explicit ChromeAppsDeprecationAllowlistComponentInstallerPolicy(
      ComponentReadyCallback component_ready_callback);
  ~ChromeAppsDeprecationAllowlistComponentInstallerPolicy() override;
  ChromeAppsDeprecationAllowlistComponentInstallerPolicy(
      const ChromeAppsDeprecationAllowlistComponentInstallerPolicy&) = delete;
  ChromeAppsDeprecationAllowlistComponentInstallerPolicy operator=(
      const ChromeAppsDeprecationAllowlistComponentInstallerPolicy&) = delete;

 private:
  // ComponentInstallerPolicy overrides.
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

  // Repeatedly called from `ComponentReady()`.
  ComponentReadyCallback on_component_ready_;
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CHROME_APPS_DEPRECATION_ALLOWLIST_COMPONENT_INSTALLER_H_
