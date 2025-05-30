// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_apps_deprecation_allowlist_component_installer.h"

#include "base/files/file_util.h"
#include "chrome/browser/apps/app_service/publishers/chrome_app_deprecation.h"
#include "chrome/browser/apps/app_service/publishers/proto/chrome_app_deprecation.pb.h"

namespace {
// The SHA256 of the public key used to sign the extension.
// The extension id is imneddfbdipbgilndpnagjdolfgbbkdn.
constexpr std::array<uint8_t, 32>
    kChromeAppsDeprecationAllowlistsPublicKeySHA256 = {
        0x8c, 0xd4, 0x33, 0x51, 0x38, 0xf1, 0x68, 0xbd, 0x3f, 0xd0, 0x69,
        0x3e, 0xb5, 0x61, 0x1a, 0x3d, 0x89, 0xd5, 0x5c, 0xd6, 0x6b, 0xab,
        0x13, 0x9f, 0x70, 0x77, 0xdb, 0x15, 0xe6, 0x23, 0x9e, 0x9f};

constexpr base::FilePath::CharType kDataFileName[] =
    FILE_PATH_LITERAL("chrome-app-deprecation-allowlists.pb");

}  // namespace

namespace component_updater {

ChromeAppsDeprecationAllowlistComponentInstallerPolicy::
    ChromeAppsDeprecationAllowlistComponentInstallerPolicy(
        ComponentReadyCallback on_component_ready)
    : on_component_ready_(std::move(on_component_ready)) {}

ChromeAppsDeprecationAllowlistComponentInstallerPolicy::
    ~ChromeAppsDeprecationAllowlistComponentInstallerPolicy() = default;

bool ChromeAppsDeprecationAllowlistComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kDataFileName));
}

bool ChromeAppsDeprecationAllowlistComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool ChromeAppsDeprecationAllowlistComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
ChromeAppsDeprecationAllowlistComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  // No custom install.
  return update_client::CrxInstaller::Result(0);
}

void ChromeAppsDeprecationAllowlistComponentInstallerPolicy::
    OnCustomUninstall() {}

void ChromeAppsDeprecationAllowlistComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  if (install_dir.empty() || !version.IsValid()) {
    return;
  }

  on_component_ready_.Run(version, install_dir.Append(kDataFileName));
}

base::FilePath
ChromeAppsDeprecationAllowlistComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("ChromeAppDeprecationAllowlist"));
}

void ChromeAppsDeprecationAllowlistComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kChromeAppsDeprecationAllowlistsPublicKeySHA256),
               std::end(kChromeAppsDeprecationAllowlistsPublicKeySHA256));
}

std::string ChromeAppsDeprecationAllowlistComponentInstallerPolicy::GetName()
    const {
  return "Chrome App Deprecation Allowlist";
}

update_client::InstallerAttributes
ChromeAppsDeprecationAllowlistComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

}  // namespace component_updater
