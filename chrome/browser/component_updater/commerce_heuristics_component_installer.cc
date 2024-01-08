// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/commerce_heuristics_component_installer.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "components/commerce/core/commerce_heuristics_data.h"
#include "components/component_updater/component_updater_paths.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "components/search/ntp_features.h"
#else
#include "components/commerce/core/commerce_feature_list.h"
#endif

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: cocncanleafgejenidihemfflagifjic
const uint8_t kCommerceHeuristicsPublicKeySHA256[32] = {
    0x2e, 0x2d, 0x20, 0xdb, 0x40, 0x56, 0x49, 0x4d, 0x83, 0x87, 0x4c,
    0x55, 0xb0, 0x68, 0x59, 0x82, 0xb6, 0x0d, 0xad, 0xaf, 0xcd, 0xa8,
    0x49, 0xb8, 0x61, 0x7a, 0x32, 0x38, 0xe0, 0x72, 0x2a, 0x10};

constexpr char kCommerceHeuristicsManifestName[] = "Commerce Heuristics";
constexpr base::FilePath::CharType kCommerceHintHeuristicsFileName[] =
    FILE_PATH_LITERAL("commerce_hint_heuristics.json");
constexpr base::FilePath::CharType kCommerceGlobalHeuristicsFileName[] =
    FILE_PATH_LITERAL("commerce_global_heuristics.json");
constexpr base::FilePath::CharType kCommerceProductIDHeuristicsFileName[] =
    FILE_PATH_LITERAL("commerce_product_id_heuristics.json");
constexpr base::FilePath::CharType kCommerceCartExtractionScriptFileName[] =
    FILE_PATH_LITERAL("cart_product_extraction.js");

base::FilePath GetCommerceHintHeuristicsInstalledPath(
    const base::FilePath& base) {
  return base.Append(kCommerceHintHeuristicsFileName);
}

base::FilePath GetCommerceGlobalHeuristicsInstalledPath(
    const base::FilePath& base) {
  return base.Append(kCommerceGlobalHeuristicsFileName);
}

base::FilePath GetCommerceProductIDHeuristicsInstalledPath(
    const base::FilePath& base) {
  return base.Append(kCommerceProductIDHeuristicsFileName);
}

base::FilePath GetCommerceCartExtractionScriptInstalledPath(
    const base::FilePath& base) {
  return base.Append(kCommerceCartExtractionScriptFileName);
}

void LoadHeuristicFilesFromDisk(const base::Version& version,
                                const base::FilePath& install_dir) {
  if (install_dir.empty()) {
    return;
  }

  const base::FilePath& commerce_hint_file_path =
      GetCommerceHintHeuristicsInstalledPath(install_dir);
  const base::FilePath& commerce_global_file_path =
      GetCommerceGlobalHeuristicsInstalledPath(install_dir);
  const base::FilePath& commerce_product_id_file_path =
      GetCommerceProductIDHeuristicsInstalledPath(install_dir);
  const base::FilePath& cart_extraction_file_path =
      GetCommerceCartExtractionScriptInstalledPath(install_dir);
  if (commerce_global_file_path.empty() || commerce_global_file_path.empty() ||
      commerce_product_id_file_path.empty() ||
      cart_extraction_file_path.empty()) {
    return;
  }

  std::string commerce_hint_json_data;
  std::string commerce_global_json_data;
  std::string commerce_product_id_json_data;
  std::string cart_extraction_script_data;

  if (!base::ReadFileToString(commerce_hint_file_path,
                              &commerce_hint_json_data)) {
    LOG(WARNING) << "Failed reading from " << commerce_hint_file_path.value();
    return;
  }
  if (!base::ReadFileToString(commerce_global_file_path,
                              &commerce_global_json_data)) {
    LOG(WARNING) << "Failed reading from " << commerce_global_file_path.value();
    return;
  }
  if (!base::ReadFileToString(commerce_product_id_file_path,
                              &commerce_product_id_json_data)) {
    LOG(WARNING) << "Failed reading from "
                 << commerce_product_id_file_path.value();
    return;
  }
  if (!base::ReadFileToString(cart_extraction_file_path,
                              &cart_extraction_script_data)) {
    LOG(WARNING) << "Failed reading from " << cart_extraction_file_path.value();
    return;
  }

  if (!commerce_heuristics::CommerceHeuristicsData::GetInstance()
           .PopulateDataFromComponent(std::move(commerce_hint_json_data),
                                      std::move(commerce_global_json_data),
                                      std::move(commerce_product_id_json_data),
                                      std::move(cart_extraction_script_data))) {
    LOG(WARNING) << "Failed populating data.";
    return;
  }
  commerce_heuristics::CommerceHeuristicsData::GetInstance().UpdateVersion(
      version);
}

}  // namespace

namespace component_updater {

bool CommerceHeuristicsInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool CommerceHeuristicsInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
CommerceHeuristicsInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void CommerceHeuristicsInstallerPolicy::OnCustomUninstall() {}

void CommerceHeuristicsInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadHeuristicFilesFromDisk, version, install_dir));
}

// Called during startup and installation before ComponentReady().
bool CommerceHeuristicsInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(
             GetCommerceGlobalHeuristicsInstalledPath(install_dir)) &&
         base::PathExists(GetCommerceHintHeuristicsInstalledPath(install_dir));
}

base::FilePath CommerceHeuristicsInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath::FromUTF8Unsafe("CommerceHeuristics");
}

void CommerceHeuristicsInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kCommerceHeuristicsPublicKeySHA256),
               std::end(kCommerceHeuristicsPublicKeySHA256));
}

std::string CommerceHeuristicsInstallerPolicy::GetName() const {
  return kCommerceHeuristicsManifestName;
}

update_client::InstallerAttributes
CommerceHeuristicsInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterCommerceHeuristicsComponent(
    component_updater::ComponentUpdateService* cus) {
#if !BUILDFLAG(IS_ANDROID)
  if (IsCartModuleEnabled())
#endif
  {
    VLOG(1) << "Registering Commerce Heuristics component.";
    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<CommerceHeuristicsInstallerPolicy>());
    installer->Register(cus, base::OnceClosure());
  }
}

}  // namespace component_updater
