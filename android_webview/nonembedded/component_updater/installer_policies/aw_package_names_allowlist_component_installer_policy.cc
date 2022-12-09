// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/installer_policies/aw_package_names_allowlist_component_installer_policy.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/common/components/aw_apps_package_names_allowlist_component_utils.h"
#include "android_webview/nonembedded/component_updater/aw_component_installer_policy.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"

namespace {

const char kWebViewAppsPackageNamesAllowlistName[] =
    "WebViewAppsPackageNamesAllowlist";

}  // namespace

namespace android_webview {

AwPackageNamesAllowlistComponentInstallerPolicy::
    AwPackageNamesAllowlistComponentInstallerPolicy() = default;

AwPackageNamesAllowlistComponentInstallerPolicy::
    ~AwPackageNamesAllowlistComponentInstallerPolicy() = default;

update_client::CrxInstaller::Result
AwPackageNamesAllowlistComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  // Nothing custom here.
  return update_client::CrxInstaller::Result(/* error = */ 0);
}

void RegisterWebViewAppsPackageNamesAllowlistComponent(
    base::OnceCallback<bool(const component_updater::ComponentRegistration&)>
        register_callback,
    base::OnceClosure registration_finished) {
  base::MakeRefCounted<component_updater::ComponentInstaller>(
      std::make_unique<AwPackageNamesAllowlistComponentInstallerPolicy>())
      ->Register(std::move(register_callback),
                 std::move(registration_finished));
}

bool AwPackageNamesAllowlistComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool AwPackageNamesAllowlistComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

bool AwPackageNamesAllowlistComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath
AwPackageNamesAllowlistComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("WebViewAppsPackageNamesAllowlist"));
}

void AwPackageNamesAllowlistComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetWebViewAppsPackageNamesAllowlistPublicKeyHash(hash);
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
