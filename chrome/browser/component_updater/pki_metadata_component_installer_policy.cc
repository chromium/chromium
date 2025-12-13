// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pki_metadata_component_installer_policy.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: efniojlnjndmcbiieegkicadnoecjjef
const uint8_t kPKIMetadataPublicKeySHA256[32] = {
    0x45, 0xd8, 0xe9, 0xbd, 0x9d, 0x3c, 0x21, 0x88, 0x44, 0x6a, 0x82,
    0x03, 0xde, 0x42, 0x99, 0x45, 0x66, 0x25, 0xfe, 0xb3, 0xd1, 0xf8,
    0x11, 0x65, 0xb4, 0x6f, 0xd3, 0x1b, 0x21, 0x89, 0xbe, 0x9c};

}  // namespace

namespace component_updater {

PKIMetadataComponentInstallerPolicy::PKIMetadataComponentInstallerPolicy() =
    default;

PKIMetadataComponentInstallerPolicy::~PKIMetadataComponentInstallerPolicy() =
    default;

bool PKIMetadataComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool PKIMetadataComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
PKIMetadataComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void PKIMetadataComponentInstallerPolicy::OnCustomUninstall() {}

void PKIMetadataComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict /* manifest */) {
  PKIMetadataComponentInstallerService::GetInstance()->OnComponentReady(
      install_dir);
}

// Called during startup and installation before ComponentReady().
bool PKIMetadataComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& install_dir) const {
  if (!base::PathExists(install_dir)) {
    return false;
  }

  return true;
}

base::FilePath PKIMetadataComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("PKIMetadata"));
}

void PKIMetadataComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPKIMetadataPublicKeySHA256),
               std::end(kPKIMetadataPublicKeySHA256));
}

std::string PKIMetadataComponentInstallerPolicy::GetName() const {
  return "PKI Metadata";
}

update_client::InstallerAttributes
PKIMetadataComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

}  // namespace component_updater
