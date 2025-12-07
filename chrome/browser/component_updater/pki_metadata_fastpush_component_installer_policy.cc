// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pki_metadata_fastpush_component_installer_policy.h"

#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the fastpush extension.
// The extension id is: oaanfgijljhkdknnacjidbpmmgnghhjj
constexpr uint8_t kPKIMetadataFastpushPublicKeySHA256[32] = {
    0xe0, 0x0d, 0x56, 0x89, 0xb9, 0x7a, 0x3a, 0xdd, 0x02, 0x98, 0x31,
    0xfc, 0xc6, 0xd6, 0x77, 0x99, 0x71, 0x4d, 0xf9, 0xc7, 0x7e, 0xa2,
    0x29, 0xcd, 0x41, 0x2b, 0x51, 0xee, 0x7d, 0xe8, 0x12, 0x5c};

}  // namespace

namespace component_updater {

PKIMetadataFastpushComponentInstallerPolicy::
    PKIMetadataFastpushComponentInstallerPolicy() = default;

PKIMetadataFastpushComponentInstallerPolicy::
    ~PKIMetadataFastpushComponentInstallerPolicy() = default;

bool PKIMetadataFastpushComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool PKIMetadataFastpushComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
PKIMetadataFastpushComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void PKIMetadataFastpushComponentInstallerPolicy::OnCustomUninstall() {}

void PKIMetadataFastpushComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict /* manifest */) {
  PKIMetadataComponentInstallerService::GetInstance()->OnFastpushComponentReady(
      install_dir);
}

// Called during startup and installation before ComponentReady().
bool PKIMetadataFastpushComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& install_dir) const {
  if (!base::PathExists(install_dir)) {
    return false;
  }

  return true;
}

base::FilePath
PKIMetadataFastpushComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("PKIMetadataFastpush"));
}

void PKIMetadataFastpushComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  *hash = base::ToVector(kPKIMetadataFastpushPublicKeySHA256);
}

std::string PKIMetadataFastpushComponentInstallerPolicy::GetName() const {
  return "PKI Metadata Fastpush";
}

update_client::InstallerAttributes
PKIMetadataFastpushComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

}  // namespace component_updater
