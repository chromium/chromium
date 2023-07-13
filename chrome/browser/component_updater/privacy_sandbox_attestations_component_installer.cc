// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_installer.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/update_client/update_client.h"

namespace {

constexpr base::FilePath::CharType kPrivacySandboxAttestationsFileName[] =
    FILE_PATH_LITERAL("privacy-sandbox-attestations.dat");

constexpr base::FilePath::CharType
    kPrivacySandboxAttestationsRelativeInstallDir[] =
        FILE_PATH_LITERAL("PrivacySandboxAttestationsPreloaded");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: niikhdgajlphfehepabhhblakbdgeefj
constexpr uint8_t kPrivacySandboxAttestationsPublicKeySHA256[32] = {
    0xd8, 0x8a, 0x73, 0x60, 0x9b, 0xf7, 0x54, 0x74, 0xf0, 0x17, 0x71,
    0xb0, 0xa1, 0x36, 0x44, 0x59, 0xf6, 0x22, 0x38, 0xa8, 0x7c, 0xc9,
    0x7b, 0x7a, 0x43, 0x2c, 0x72, 0xee, 0x04, 0x01, 0xae, 0xc0};

const char kPrivacySandboxAttestationsManifestName[] =
    "Privacy Sandbox Attestations";

}  // namespace

namespace component_updater {

PrivacySandboxAttestationsComponentInstallerPolicy::
    PrivacySandboxAttestationsComponentInstallerPolicy(
        AttestationsReadyRepeatingCallback on_attestations_ready)
    : on_attestations_ready_(std::move(on_attestations_ready)) {}

PrivacySandboxAttestationsComponentInstallerPolicy::
    ~PrivacySandboxAttestationsComponentInstallerPolicy() = default;

bool PrivacySandboxAttestationsComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetInstalledPath(install_dir));
}

bool PrivacySandboxAttestationsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool PrivacySandboxAttestationsComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  // Privacy sandbox attestations are identical for all users.
  return false;
}

update_client::CrxInstaller::Result
PrivacySandboxAttestationsComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  // No custom install for privacy sandbox attestations.
  return update_client::CrxInstaller::Result(0);
}

void PrivacySandboxAttestationsComponentInstallerPolicy::OnCustomUninstall() {}

void PrivacySandboxAttestationsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kEnforcePrivacySandboxAttestations)) {
    // Privacy Sandbox Enrollment is not enforced if the feature is disabled.
    return;
  }

  if (on_attestations_ready_.is_null()) {
    return;
  }

  if (install_dir.empty() || !version.IsValid()) {
    return;
  }

  VLOG(1) << "Privacy Sandbox Attestations Component ready, version "
          << version.GetString() << " in " << install_dir.value();

  on_attestations_ready_.Run(std::move(version), std::move(install_dir));
}

base::FilePath
PrivacySandboxAttestationsComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kPrivacySandboxAttestationsRelativeInstallDir);
}

void PrivacySandboxAttestationsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPrivacySandboxAttestationsPublicKeySHA256),
               std::end(kPrivacySandboxAttestationsPublicKeySHA256));
}

std::string PrivacySandboxAttestationsComponentInstallerPolicy::GetName()
    const {
  return kPrivacySandboxAttestationsManifestName;
}

update_client::InstallerAttributes
PrivacySandboxAttestationsComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

base::FilePath
PrivacySandboxAttestationsComponentInstallerPolicy::GetInstalledDirectory(
    const base::FilePath& base) {
  return base.Append(kPrivacySandboxAttestationsRelativeInstallDir);
}

base::FilePath
PrivacySandboxAttestationsComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kPrivacySandboxAttestationsFileName);
}

void RegisterPrivacySandboxAttestationsComponent(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kEnforcePrivacySandboxAttestations)) {
    // Privacy sandbox enrollment is not enforced if the feature is disabled.
    // Any existing version of this component is deleted.
    base::FilePath user_path;
    if (base::PathService::Get(chrome::DIR_USER_DATA, &user_path)) {
      base::ThreadPool::PostTask(
          FROM_HERE, {base::TaskPriority::LOWEST, base::MayBlock()},
          base::GetDeletePathRecursivelyCallback(
              user_path.Append(kPrivacySandboxAttestationsRelativeInstallDir)));
    }
    return;
  }

  VLOG(1) << "Registering Privacy Sandbox Attestations component";

  auto policy =
      std::make_unique<PrivacySandboxAttestationsComponentInstallerPolicy>(
          /*on_attestations_ready=*/base::BindRepeating(
              [](base::Version version, base::FilePath install_dir) {
                VLOG(1) << "Received privacy sandbox attestations file";
                privacy_sandbox::PrivacySandboxAttestations::GetInstance()
                    ->LoadAttestations(std::move(version),
                                       std::move(install_dir));
              }));

  base::MakeRefCounted<ComponentInstaller>(std::move(policy))
      ->Register(cus, base::OnceClosure(), base::TaskPriority::BEST_EFFORT);
}

}  // namespace component_updater
