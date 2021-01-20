// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/floc_component_installer.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/federated_learning/floc_constants.h"
#include "components/federated_learning/floc_sorting_lsh_clusters_service.h"

namespace component_updater {

// The extension id is: cmahhnpholdijhjokonmfdjbfmklppij
constexpr uint8_t kFlocComponentPublicKeySHA256[32] = {
    0x2c, 0x07, 0x7d, 0xf7, 0xeb, 0x38, 0x97, 0x9e, 0xae, 0xdc, 0x53,
    0x91, 0x5c, 0xab, 0xff, 0x89, 0xbc, 0xf0, 0xd9, 0x30, 0xd2, 0x2e,
    0x8f, 0x68, 0x3a, 0xf9, 0x21, 0x91, 0x9f, 0xc1, 0x84, 0xa1};

constexpr char kFlocComponentFetcherManifestName[] =
    "Federated Learning of Cohorts";

FlocComponentInstallerPolicy::FlocComponentInstallerPolicy(
    federated_learning::FlocSortingLshClustersService*
        floc_sorting_lsh_clusters_service)
    : floc_sorting_lsh_clusters_service_(floc_sorting_lsh_clusters_service) {}

FlocComponentInstallerPolicy::~FlocComponentInstallerPolicy() = default;

bool FlocComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return false;
}

// Public data is delivered via this component, no need for encryption.
bool FlocComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
FlocComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void FlocComponentInstallerPolicy::OnCustomUninstall() {}

void FlocComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK(!install_dir.empty());

  floc_sorting_lsh_clusters_service_->OnSortingLshClustersFileReady(
      install_dir.Append(federated_learning::kSortingLshClustersFileName),
      version);
}

// Called during startup and installation before ComponentReady().
bool FlocComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  if (!base::PathExists(install_dir))
    return false;

  int floc_component_format = 0;
  if (!manifest.GetInteger(federated_learning::kManifestFlocComponentFormatKey,
                           &floc_component_format) ||
      floc_component_format !=
          federated_learning::kCurrentFlocComponentFormatVersion) {
    return false;
  }

  return true;
}

base::FilePath FlocComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(federated_learning::kTopLevelDirectoryName);
}

void FlocComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kFlocComponentPublicKeySHA256),
               std::end(kFlocComponentPublicKeySHA256));
}

std::string FlocComponentInstallerPolicy::GetName() const {
  return kFlocComponentFetcherManifestName;
}

update_client::InstallerAttributes
FlocComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterFlocComponent(
    ComponentUpdateService* cus,
    federated_learning::FlocSortingLshClustersService*
        floc_sorting_lsh_clusters_service) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<FlocComponentInstallerPolicy>(
          floc_sorting_lsh_clusters_service));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
