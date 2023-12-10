// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/afp_blocked_domain_list_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/common/chrome_features.h"
#include "components/component_updater/component_updater_paths.h"

using component_updater::ComponentUpdateService;

namespace {

using ListReadyRepeatingCallback = component_updater::
    AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
        ListReadyRepeatingCallback;

const base::FilePath::CharType kAfpBlockedDomainListBinaryPbFileName[] =
    FILE_PATH_LITERAL("rules.pb");

const base::FilePath::CharType kAfpBlockedDomainListRelativeInstallDirName[] =
    FILE_PATH_LITERAL("AfpBlockedDomainList");

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The CRX ID is: kgdbnmlfakkebekbaceapiaenjgmlhan.
const uint8_t kAfpBlockedDomainListPublicKeySHA256[32] = {
    0xa6, 0x31, 0xdc, 0xb5, 0x0a, 0xa4, 0x14, 0xa1, 0x02, 0x40, 0xf8,
    0x04, 0xd9, 0x6c, 0xb7, 0x0d, 0x7b, 0xbd, 0x63, 0xf9, 0xc8, 0x65,
    0x6e, 0x9b, 0x83, 0x7a, 0x3a, 0xfd, 0xd1, 0xc8, 0x40, 0xe3};

const char kAfpBlockedDomainListManifestName[] =
    "Anti-Fingerprinting Blocked Domain List";

// Runs on a thread pool and reads the component file from disk to a string.
absl::optional<std::string> ReadComponentFromDisk(
    const base::FilePath& file_path) {
  std::string contents;
  if (!base::ReadFileToString(file_path, &contents)) {
    VLOG(1) << "Failed reading from " << file_path.value();
    return absl::nullopt;
  }
  return contents;
}

base::TaskPriority GetTaskPriority() {
  return base::FeatureList::IsEnabled(
             features::kEnableNetworkServiceResourceBlockList)
             ? base::TaskPriority::USER_BLOCKING
             : base::TaskPriority::BEST_EFFORT;
}

}  // namespace

namespace component_updater {

AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    AntiFingerprintingBlockedDomainListComponentInstallerPolicy(
        ListReadyRepeatingCallback on_list_ready)
    : on_list_ready_(on_list_ready) {
  CHECK(on_list_ready);
}

AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    ~AntiFingerprintingBlockedDomainListComponentInstallerPolicy() = default;

bool AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  // No encryption required since the Blocklist will be public and identical for
  // all users.
  return false;
}

update_client::CrxInstaller::Result
AntiFingerprintingBlockedDomainListComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    OnCustomUninstall() {}

base::FilePath
AntiFingerprintingBlockedDomainListComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kAfpBlockedDomainListBinaryPbFileName);
}

void AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    ComponentReady(const base::Version& version,
                   const base::FilePath& install_dir,
                   base::Value::Dict manifest) {
  VLOG(1) << "Anti-Fingerprinting Blocked Domain List Component ready, version "
          << version.GetString() << " in " << install_dir.value();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), GetTaskPriority()},
      base::BindOnce(&ReadComponentFromDisk, GetInstalledPath(install_dir)),
      base::BindOnce(
          [](ListReadyRepeatingCallback on_list_ready,
             const absl::optional<std::string>& maybe_contents) {
            if (maybe_contents.has_value()) {
              on_list_ready.Run(maybe_contents.value());
            }
          },
          on_list_ready_));
}

// Called during startup and installation before ComponentReady().
bool AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    VerifyInstallation(const base::Value::Dict& manifest,
                       const base::FilePath& install_dir) const {
  if (!base::PathExists(GetInstalledPath(install_dir))) {
    return false;
  }

  std::string contents;
  if (!base::ReadFileToString(GetInstalledPath(install_dir), &contents)) {
    return false;
  }

  // TODO(thesalsa): Perform more validation of the proto file where it gets
  // deserialized for use.

  return true;
}

base::FilePath AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    GetRelativeInstallDir() const {
  return base::FilePath(kAfpBlockedDomainListRelativeInstallDirName);
}

void AntiFingerprintingBlockedDomainListComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kAfpBlockedDomainListPublicKeySHA256),
               std::end(kAfpBlockedDomainListPublicKeySHA256));
}

std::string
AntiFingerprintingBlockedDomainListComponentInstallerPolicy::GetName() const {
  return kAfpBlockedDomainListManifestName;
}

update_client::InstallerAttributes
AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterAntiFingerprintingBlockedDomainListComponent(
    ComponentUpdateService* cus) {
  VLOG(1) << "Registering Anti-Fingerprinting Blocked Domain List Component.";
  auto policy = std::make_unique<
      AntiFingerprintingBlockedDomainListComponentInstallerPolicy>(
      /*on_list_ready=*/base::DoNothing());

  base::MakeRefCounted<ComponentInstaller>(std::move(policy))
      ->Register(cus, base::OnceClosure(), GetTaskPriority());
}

// Deletes the install directory for the Anti-Fingerprinting Blocklist. Used to
// clean up any existing versions if the component is disabled.
void DeleteAntiFingerprintingBlockedDomainListComponent(
    const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          base::IgnoreResult(&base::DeletePathRecursively),
          user_data_dir.Append(kAfpBlockedDomainListRelativeInstallDirName)));
}

}  // namespace component_updater
