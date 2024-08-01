// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/component_updater/crowd_deny_component_installer.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "components/permissions/permission_uma_util.h"

namespace {

// The SHA-256 hash of the public key (in X.509 format, DER-encoded) used to
// sign the extension. The extension id is: ggkkehgbnfjpeggfpleeakpidbkibbmn.
constexpr uint8_t kCrowdDenyPublicKeySHA256[32] = {
    0x66, 0xaa, 0x47, 0x61, 0xd5, 0x9f, 0x46, 0x65, 0xfb, 0x44, 0x0a,
    0xf8, 0x31, 0xa8, 0x11, 0xcd, 0x5e, 0xea, 0x32, 0xe0, 0x29, 0x8b,
    0x0c, 0x3a, 0xb4, 0xc9, 0x5e, 0x9c, 0xa4, 0x2a, 0x6d, 0x90};

constexpr char kCrowdDenyHumanReadableName[] = "Crowd Deny";
constexpr char kCrowdDenyManifestPreloadDataFormatKey[] = "preload_data_format";
constexpr int kCrowdDenyManifestPreloadDataCurrentFormat = 1;

constexpr base::FilePath::CharType kCrowdDenyPreloadDataFilename[] =
    FILE_PATH_LITERAL("Preload Data");

base::FilePath GetPreloadDataFilePath(const base::FilePath& install_dir) {
  return install_dir.Append(kCrowdDenyPreloadDataFilename);
}

}  // namespace

namespace component_updater {

bool CrowdDenyComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool CrowdDenyComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // Just check that the file is there, detailed verification of the contents is
  // delegated to code in //chrome/browser/permissions.
  return base::PathExists(GetPreloadDataFilePath(install_dir));
}

bool CrowdDenyComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
CrowdDenyComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  // Nothing custom here.
  return update_client::CrxInstaller::Result(0);
}

void CrowdDenyComponentInstallerPolicy::OnCustomUninstall() {
  // Nothing custom here.
}

void CrowdDenyComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  DVLOG(1) << "Crowd Deny component ready, version " << version.GetString()
           << " in " << install_dir.value();

  std::optional<int> format =
      manifest.FindInt(kCrowdDenyManifestPreloadDataFormatKey);
  if (!format || *format != kCrowdDenyManifestPreloadDataCurrentFormat) {
    DVLOG(1) << "Crowd Deny component bailing out.";
    DVLOG_IF(1, format) << "Future data version: " << *format;
    return;
  }

  CrowdDenyPreloadData::GetInstance()->LoadFromDisk(
      GetPreloadDataFilePath(install_dir), version);
}

base::FilePath CrowdDenyComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("Crowd Deny"));
}

void CrowdDenyComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(
      kCrowdDenyPublicKeySHA256,
      kCrowdDenyPublicKeySHA256 + std::size(kCrowdDenyPublicKeySHA256));
}

std::string CrowdDenyComponentInstallerPolicy::GetName() const {
  return kCrowdDenyHumanReadableName;
}

update_client::InstallerAttributes
CrowdDenyComponentInstallerPolicy::GetInstallerAttributes() const {
  // No special update rules.
  return update_client::InstallerAttributes();
}

void RegisterCrowdDenyComponent(ComponentUpdateService* cus) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<CrowdDenyComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
