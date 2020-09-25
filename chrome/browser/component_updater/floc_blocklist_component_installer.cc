// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/floc_blocklist_component_installer.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/federated_learning/floc_blocklist_service.h"
#include "components/federated_learning/floc_constants.h"

namespace component_updater {

// The extension id is: cmahhnpholdijhjokonmfdjbfmklppij
constexpr uint8_t kFlocBlocklistPublicKeySHA256[32] = {
    0x2c, 0x07, 0x7d, 0xf7, 0xeb, 0x38, 0x97, 0x9e, 0xae, 0xdc, 0x53,
    0x91, 0x5c, 0xab, 0xff, 0x89, 0xbc, 0xf0, 0xd9, 0x30, 0xd2, 0x2e,
    0x8f, 0x68, 0x3a, 0xf9, 0x21, 0x91, 0x9f, 0xc1, 0x84, 0xa1};

constexpr char kFlocBlocklistFetcherManifestName[] = "Floc Blocklist";

FlocBlocklistComponentInstallerPolicy::FlocBlocklistComponentInstallerPolicy(
    federated_learning::FlocBlocklistService* floc_blocklist_service)
    : floc_blocklist_service_(floc_blocklist_service) {}

FlocBlocklistComponentInstallerPolicy::
    ~FlocBlocklistComponentInstallerPolicy() = default;

bool FlocBlocklistComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

// Public data is delivered via this component, no need for encryption.
bool FlocBlocklistComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
FlocBlocklistComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void FlocBlocklistComponentInstallerPolicy::OnCustomUninstall() {}

void FlocBlocklistComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK(!install_dir.empty());

  floc_blocklist_service_->OnBlocklistFileReady(
      install_dir.Append(federated_learning::kBlocklistFileName));
}

// Called during startup and installation before ComponentReady().
bool FlocBlocklistComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  if (!base::PathExists(install_dir))
    return false;

  int blocklist_format = 0;
  if (!manifest.GetInteger(federated_learning::kManifestBlocklistFormatKey,
                           &blocklist_format) ||
      blocklist_format != federated_learning::kCurrentBlocklistFormatVersion) {
    return false;
  }

  return true;
}

base::FilePath FlocBlocklistComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(federated_learning::kTopLevelDirectoryName)
      .Append(federated_learning::kBlocklistBaseDirectoryName);
}

void FlocBlocklistComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kFlocBlocklistPublicKeySHA256),
               std::end(kFlocBlocklistPublicKeySHA256));
}

std::string FlocBlocklistComponentInstallerPolicy::GetName() const {
  return kFlocBlocklistFetcherManifestName;
}

update_client::InstallerAttributes
FlocBlocklistComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> FlocBlocklistComponentInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void RegisterFlocBlocklistComponent(
    ComponentUpdateService* cus,
    federated_learning::FlocBlocklistService* floc_blocklist_service) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<FlocBlocklistComponentInstallerPolicy>(
          floc_blocklist_service));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
