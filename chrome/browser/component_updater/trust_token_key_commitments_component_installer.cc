// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/trust_token_key_commitments_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"

using component_updater::ComponentUpdateService;

namespace {

// This file name must be in sync with the server-side configuration, or updates
// will fail.
const base::FilePath::CharType kTrustTokenKeyCommitmentsFileName[] =
    FILE_PATH_LITERAL("keys.json");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: kiabhabjdbkjdpjbpigfodbdjmbglcoo
const uint8_t kTrustTokenKeyCommitmentsPublicKeySHA256[32] = {
    0xa8, 0x01, 0x70, 0x19, 0x31, 0xa9, 0x3f, 0x91, 0xf8, 0x65, 0xe3,
    0x13, 0x9c, 0x16, 0xb2, 0xee, 0xb4, 0xc7, 0xc2, 0x8e, 0xdb, 0x04,
    0xd3, 0xaf, 0xeb, 0x07, 0x18, 0x15, 0x89, 0x23, 0x81, 0xad};

const char kTrustTokenKeyCommitmentsManifestName[] =
    "Trust Token Key Commitments";

// Attempts to load key commitments as raw JSON from their storage file,
// returning the loaded commitments on success and nullopt on failure.
base::Optional<std::string> LoadKeyCommitmentsFromDisk(
    const base::FilePath& path) {
  if (path.empty())
    return base::nullopt;

  VLOG(1) << "Reading trust token key commitments from file: " << path.value();

  std::string ret;
  if (!base::ReadFileToString(path, &ret)) {
    VLOG(1) << "Failed reading from " << path.value();
    return base::nullopt;
  }

  return ret;
}

}  // namespace

namespace component_updater {

TrustTokenKeyCommitmentsComponentInstallerPolicy::
    TrustTokenKeyCommitmentsComponentInstallerPolicy(
        base::RepeatingCallback<void(const std::string&)> on_commitments_ready)
    : on_commitments_ready_(std::move(on_commitments_ready)) {}

TrustTokenKeyCommitmentsComponentInstallerPolicy::
    ~TrustTokenKeyCommitmentsComponentInstallerPolicy() = default;

bool TrustTokenKeyCommitmentsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool TrustTokenKeyCommitmentsComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  // A network-path adversary being able to change the key commitments would
  // nullify the Trust Tokens protocol's privacy properties---but the component
  // updater guarantees integrity even if we return false here, and we don't
  // need confidentiality since this component's value is public and identical
  // for all users.
  return false;
}

update_client::CrxInstaller::Result
TrustTokenKeyCommitmentsComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void TrustTokenKeyCommitmentsComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath
TrustTokenKeyCommitmentsComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kTrustTokenKeyCommitmentsFileName);
}

void TrustTokenKeyCommitmentsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadKeyCommitmentsFromDisk,
                     GetInstalledPath(install_dir)),
      base::BindOnce(
          // Only bother sending commitments to the network service if we loaded
          // them successfully.
          [](base::RepeatingCallback<void(const std::string&)>
                 on_commitments_ready,
             base::Optional<std::string> loaded_commitments) {
            if (loaded_commitments.has_value()) {
              on_commitments_ready.Run(*loaded_commitments);
            }
          },
          on_commitments_ready_));
}

// Called during startup and installation before ComponentReady().
bool TrustTokenKeyCommitmentsComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the commitments here, since we'll do the
  // checking in NetworkService::SetTrustTokenKeyCommitments.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
TrustTokenKeyCommitmentsComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("TrustTokenKeyCommitments"));
}

void TrustTokenKeyCommitmentsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kTrustTokenKeyCommitmentsPublicKeySHA256,
               kTrustTokenKeyCommitmentsPublicKeySHA256 +
                   base::size(kTrustTokenKeyCommitmentsPublicKeySHA256));
}

std::string TrustTokenKeyCommitmentsComponentInstallerPolicy::GetName() const {
  return kTrustTokenKeyCommitmentsManifestName;
}

update_client::InstallerAttributes
TrustTokenKeyCommitmentsComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

std::vector<std::string>
TrustTokenKeyCommitmentsComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterTrustTokenKeyCommitmentsComponentIfTrustTokensEnabled(
    ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(network::features::kTrustTokens))
    return;

  VLOG(1) << "Registering Trust Token Key Commitments component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TrustTokenKeyCommitmentsComponentInstallerPolicy>(
          /*on_commitments_ready=*/base::BindRepeating(
              [](const std::string& raw_commitments) {
                content::GetNetworkService()->SetTrustTokenKeyCommitments(
                    raw_commitments, /*callback=*/base::DoNothing());
              })));

  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
