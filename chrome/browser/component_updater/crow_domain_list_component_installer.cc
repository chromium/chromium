// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/crow_domain_list_component_installer.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/share/core/crow/crow_configuration.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/feed/feed_feature_list.h"

using component_updater::ComponentUpdateService;

namespace {

constexpr base::FilePath::CharType kCrowDomainListBinaryPbFileName[] =
    FILE_PATH_LITERAL("creator_domains_list.binarypb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: cdoopinbipdmaefofkedmagbfmdcjnaa
constexpr uint8_t kCrowDomainListPublicKeySHA256[32] = {
    0x23, 0xee, 0xf8, 0xd1, 0x8f, 0x3c, 0x04, 0x5e, 0x5a, 0x43, 0xc0,
    0x61, 0x5c, 0x32, 0x9d, 0x00, 0x6b, 0x48, 0xc2, 0x61, 0xe5, 0x93,
    0xda, 0x15, 0xd5, 0x35, 0x53, 0xd4, 0x45, 0x6c, 0x31, 0x27};

constexpr char kCrowDomainListManifestName[] = "Creator Chip Configuration";

void LoadCrowConfigFromDisk(const base::FilePath& pb_path) {
  if (pb_path.empty())
    return;

  VLOG(1) << "Reading Creator Chip config from file: " << pb_path.value();
  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb)) {
    // ComponentReady will only be called when there is some installation of the
    // component ready, so it would be correct to consider this an error.
    VLOG(1) << "Failed reading from " << pb_path.value();
    return;
  }

  crow::CrowConfiguration::GetInstance()->PopulateFromBinaryPb(binary_pb);
}

}  // namespace

namespace component_updater {

bool CrowDomainListComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool CrowDomainListComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
CrowDomainListComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // No custom steps.
}

void CrowDomainListComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath CrowDomainListComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kCrowDomainListBinaryPbFileName);
}

void CrowDomainListComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadCrowConfigFromDisk, GetInstalledPath(install_dir)));
}

// Called during startup and installation before ComponentReady().
bool CrowDomainListComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // Checking the component downloaded successfully is sufficient here;
  // the config parser will validate later.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath CrowDomainListComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("CreatorChipConfig"));
}

void CrowDomainListComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kCrowDomainListPublicKeySHA256),
               std::end(kCrowDomainListPublicKeySHA256));
}

std::string CrowDomainListComponentInstallerPolicy::GetName() const {
  return kCrowDomainListManifestName;
}

update_client::InstallerAttributes
CrowDomainListComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterCrowDomainListComponent(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(feed::kShareCrowButton)) {
    return;
  }

  VLOG(1) << "Registering component CrowDomainListComponentInstallerPolicy";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<CrowDomainListComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
