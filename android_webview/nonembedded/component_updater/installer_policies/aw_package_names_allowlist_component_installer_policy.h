// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_PACKAGE_NAMES_ALLOWLIST_COMPONENT_INSTALLER_POLICY_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_PACKAGE_NAMES_ALLOWLIST_COMPONENT_INSTALLER_POLICY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_delegate.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

namespace base {
class DictionaryValue;
class FilePath;
class Version;
}  // namespace base

namespace android_webview {

class AwPackageNamesAllowlistComponentInstallerPolicy
    : public component_updater::ComponentInstallerPolicy {
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
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

 private:
  std::unique_ptr<AwComponentInstallerPolicyDelegate> delegate_;
};

// Call once during startup to make the component update service aware of
// the package name logging component.
void RegisterWebViewAppsPackageNamesAllowlistComponent(
    base::OnceCallback<bool(const update_client::CrxComponent&)>
        register_callback,
    base::OnceClosure registration_finished);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_INSTALLER_POLICIES_AW_PACKAGE_NAMES_ALLOWLIST_COMPONENT_INSTALLER_POLICY_H_
