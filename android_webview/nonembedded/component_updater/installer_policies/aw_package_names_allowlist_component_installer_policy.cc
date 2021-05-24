// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/installer_policies/aw_package_names_allowlist_component_installer_policy.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_delegate.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: aemllinfpjdgcldgaelcgakpjmaekbai
const uint8_t kWebViewAppsPackageNamesAllowlistPublicKeySHA256[32] = {
    0x04, 0xcb, 0xb8, 0xd5, 0xf9, 0x36, 0x2b, 0x36, 0x04, 0xb2, 0x60,
    0xaf, 0x9c, 0x04, 0xa1, 0x08, 0xa3, 0xe9, 0xdc, 0x92, 0x46, 0xe7,
    0xae, 0xc8, 0x3e, 0x32, 0x6f, 0x74, 0x43, 0x02, 0xf3, 0x7e};

const char kWebViewAppsPackageNamesAllowlistName[] =
    "WebViewAppsPackageNamesAllowlist";

}  // namespace

namespace android_webview {

AwPackageNamesAllowlistComponentInstallerPolicy::
    AwPackageNamesAllowlistComponentInstallerPolicy() {
  std::vector<uint8_t> hash;
  GetHash(&hash);
  delegate_ = std::make_unique<AwComponentInstallerPolicyDelegate>(hash);
}

AwPackageNamesAllowlistComponentInstallerPolicy::
    ~AwPackageNamesAllowlistComponentInstallerPolicy() = default;

update_client::CrxInstaller::Result
AwPackageNamesAllowlistComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  // Nothing custom here.
  return update_client::CrxInstaller::Result(/* error = */ 0);
}

void AwPackageNamesAllowlistComponentInstallerPolicy::OnCustomUninstall() {
  delegate_->OnCustomUninstall();
}

void AwPackageNamesAllowlistComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  delegate_->ComponentReady(version, install_dir, std::move(manifest));
}

void RegisterWebViewAppsPackageNamesAllowlistComponent(
    base::OnceCallback<bool(const update_client::CrxComponent&)>
        register_callback,
    base::OnceClosure registration_finished) {
  base::MakeRefCounted<component_updater::ComponentInstaller>(
      std::make_unique<AwPackageNamesAllowlistComponentInstallerPolicy>())
      ->Register(std::move(register_callback),
                 std::move(registration_finished));
}

bool AwPackageNamesAllowlistComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool AwPackageNamesAllowlistComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

// Called during startup and installation before ComponentReady().
bool AwPackageNamesAllowlistComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return manifest.HasKey(kWebViewAppsPackageNamesAllowlistName);
}

base::FilePath
AwPackageNamesAllowlistComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("WebViewAppsPackageNamesAllowlist"));
}

void AwPackageNamesAllowlistComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  DCHECK(hash);
  hash->assign(
      kWebViewAppsPackageNamesAllowlistPublicKeySHA256,
      kWebViewAppsPackageNamesAllowlistPublicKeySHA256 +
          base::size(kWebViewAppsPackageNamesAllowlistPublicKeySHA256));
}

std::string AwPackageNamesAllowlistComponentInstallerPolicy::GetName() const {
  return kWebViewAppsPackageNamesAllowlistName;
}

update_client::InstallerAttributes
AwPackageNamesAllowlistComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

}  // namespace android_webview
