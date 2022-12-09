// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_SHIM_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_SHIM_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy.h"
#include "base/values.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {
class ComponentInstallerPolicy;
}  // namespace component_updater

namespace android_webview {

// A shim class that transparently redirects all calls to the passed
// installer policy object except for the calls that require custom WebView
// implementation, namely `ComponentReady` and `OnCustomUninstall`.
//
// This class is handy for installer policies shared between chrome and WebView,
// it can be used to wrap installer policies written for chrome to modify their
// behaviour to match the expected WebView behaviour.
class AwComponentInstallerPolicyShim : public AwComponentInstallerPolicy {
 public:
  explicit AwComponentInstallerPolicyShim(
      std::unique_ptr<component_updater::ComponentInstallerPolicy> policy);
  ~AwComponentInstallerPolicyShim() override;
  AwComponentInstallerPolicyShim(const AwComponentInstallerPolicyShim&) =
      delete;
  AwComponentInstallerPolicyShim& operator=(
      const AwComponentInstallerPolicyShim&) = delete;

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
  void GetHash(std::vector<uint8_t>* hash) const override;

 private:
  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_SHIM_H_
