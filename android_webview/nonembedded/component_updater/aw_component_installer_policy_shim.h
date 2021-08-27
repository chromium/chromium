// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_SHIM_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_SHIM_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "components/component_updater/component_installer.h"

namespace base {
class DictionaryValue;
class FilePath;
class Version;
}  // namespace base

namespace android_webview {

class AwComponentInstallerPolicyDelegate;

// A shim class that transparently redirects all calls to the passed
// installer policy object except for the calls that require custom WebView
// handling, namely `ComponentReady` and `OnCustomUninstall`.
//
// This class is handy for installer policies shared between chrome and WebView,
// it can be used to wrap installer policies written for chrome to modify their
// behaviour to match the expected WebView behaviour.
class AwComponentInstallerPolicyShim
    : public component_updater::ComponentInstallerPolicy {
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
  void GetHash(std::vector<uint8_t>* hash) const override;

 private:
  std::unique_ptr<AwComponentInstallerPolicyDelegate> delegate_;
  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_INSTALLER_POLICY_SHIM_H_
