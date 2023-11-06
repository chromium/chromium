// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_PACKAGE_NAMES_ALLOWLIST_COMPONENT_INSTALLER_POLICY_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_PACKAGE_NAMES_ALLOWLIST_COMPONENT_INSTALLER_POLICY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy.h"
#include "base/functional/callback.h"
#include "base/values.h"

namespace base {
class FilePath;
}  // namespace base

namespace android_webview {

class AwPackageNamesAllowlistComponentInstallerPolicy
    : public AwComponentInstallerPolicy {
 public:
  AwPackageNamesAllowlistComponentInstallerPolicy();
  ~AwPackageNamesAllowlistComponentInstallerPolicy() override;
  AwPackageNamesAllowlistComponentInstallerPolicy(
      const AwPackageNamesAllowlistComponentInstallerPolicy&) = delete;
  AwPackageNamesAllowlistComponentInstallerPolicy& operator=(
      const AwPackageNamesAllowlistComponentInstallerPolicy&) = delete;

  void GetHash(std::vector<uint8_t>* hash) const override;

 protected:
  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  base::FilePath GetRelativeInstallDir() const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

// Call once during startup to make the component update service aware of
// the package name logging component.
void RegisterWebViewAppsPackageNamesAllowlistComponent(
    base::OnceCallback<bool(const component_updater::ComponentRegistration&)>
        register_callback,
    base::OnceClosure registration_finished);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_PACKAGE_NAMES_ALLOWLIST_COMPONENT_INSTALLER_POLICY_H_
