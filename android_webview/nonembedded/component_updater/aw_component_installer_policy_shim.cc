// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_shim.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace android_webview {

AwComponentInstallerPolicyShim::AwComponentInstallerPolicyShim(
    std::unique_ptr<component_updater::ComponentInstallerPolicy> policy)
    : policy_(std::move(policy)) {
}

AwComponentInstallerPolicyShim::~AwComponentInstallerPolicyShim() = default;

update_client::CrxInstaller::Result
AwComponentInstallerPolicyShim::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return policy_->OnCustomInstall(manifest, install_dir);
}

bool AwComponentInstallerPolicyShim::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return policy_->SupportsGroupPolicyEnabledComponentUpdates();
}

bool AwComponentInstallerPolicyShim::RequiresNetworkEncryption() const {
  return policy_->RequiresNetworkEncryption();
}

bool AwComponentInstallerPolicyShim::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return policy_->VerifyInstallation(manifest, install_dir);
}

base::FilePath AwComponentInstallerPolicyShim::GetRelativeInstallDir() const {
  return policy_->GetRelativeInstallDir();
}

void AwComponentInstallerPolicyShim::GetHash(std::vector<uint8_t>* hash) const {
  policy_->GetHash(hash);
}

std::string AwComponentInstallerPolicyShim::GetName() const {
  return policy_->GetName();
}

update_client::InstallerAttributes
AwComponentInstallerPolicyShim::GetInstallerAttributes() const {
  return policy_->GetInstallerAttributes();
}

}  // namespace android_webview
