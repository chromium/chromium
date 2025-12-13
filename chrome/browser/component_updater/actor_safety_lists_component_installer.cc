// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/actor_safety_lists_component_installer.h"

#include <stdint.h>

#include <optional>
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
#include "chrome/browser/actor/safety_list_manager.h"
#include "components/component_updater/component_updater_paths.h"

namespace {

const base::FilePath::CharType kActorSafetyListsComponentFileName[] =
    FILE_PATH_LITERAL("listdata.json");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: ninodabcejpeglfjbkhdplaoglpcbffj
const uint8_t kActorSafetyListsPublicKeySHA256[32] = {
    0xd8, 0xde, 0x30, 0x12, 0x49, 0xf4, 0x6b, 0x59, 0x1a, 0x73, 0xfb,
    0x0e, 0x6b, 0xf2, 0x15, 0x59, 0x82, 0x1b, 0x44, 0xa6, 0xa6, 0x71,
    0x3b, 0x51, 0x57, 0xd0, 0xab, 0xa7, 0x61, 0xb9, 0x6a, 0x20};

const char kActorSafetyListsManifestName[] = "Actor Safety Lists";

constexpr base::FilePath::CharType kRelInstallDir[] =
    FILE_PATH_LITERAL("ActorSafetyLists");

// Runs on a thread pool.
std::optional<std::string> ReadComponentFromDisk(
    const base::FilePath& file_path) {
  VLOG(1) << "Reading Actor Safety Lists data from file: " << file_path.value();
  std::string contents;
  if (!base::ReadFileToString(file_path, &contents)) {
    VLOG(1) << "Failed reading from " << file_path.value();
    return std::nullopt;
  }
  return contents;
}

}  // namespace

namespace component_updater {

ActorSafetyListsComponentInstallerPolicy::
    ActorSafetyListsComponentInstallerPolicy(
        OnActorSafetyListsComponentReadyCallback on_component_ready_cb)
    : on_component_ready_cb_(on_component_ready_cb) {}

ActorSafetyListsComponentInstallerPolicy::
    ActorSafetyListsComponentInstallerPolicy() = default;

ActorSafetyListsComponentInstallerPolicy::
    ~ActorSafetyListsComponentInstallerPolicy() = default;

bool ActorSafetyListsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool ActorSafetyListsComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
ActorSafetyListsComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void ActorSafetyListsComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath ActorSafetyListsComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kActorSafetyListsComponentFileName);
}

void ActorSafetyListsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Actor Safety Lists ready, version " << version.GetString()
          << " in " << install_dir.value();

  // Given `BEST_EFFORT` since we don't need to be USER_BLOCKING.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadComponentFromDisk, GetInstalledPath(install_dir)),
      base::BindOnce(on_component_ready_cb_));
}

// Called during startup and installation before ComponentReady().
bool ActorSafetyListsComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the json here, since we'll do the checking
  // in the parsing component
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath ActorSafetyListsComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kRelInstallDir);
}

void ActorSafetyListsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kActorSafetyListsPublicKeySHA256),
               std::end(kActorSafetyListsPublicKeySHA256));
}

std::string ActorSafetyListsComponentInstallerPolicy::GetName() const {
  return kActorSafetyListsManifestName;
}

update_client::InstallerAttributes
ActorSafetyListsComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterActorSafetyListsComponent(ComponentUpdateService* cus,
                                       base::OnceClosure callback) {
  VLOG(1) << "Registering Actor Safety Lists Component.";
  auto policy = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<ActorSafetyListsComponentInstallerPolicy>(
          base::BindRepeating(
              [](const std::optional<std::string>& raw_metadata) {
                if (raw_metadata.has_value()) {
                  actor::SafetyListManager::GetInstance()->ParseSafetyLists(
                      *raw_metadata);
                }
              })));
  policy->Register(cus, std::move(callback));
}

// static
base::FilePath
ActorSafetyListsComponentInstallerPolicy::GetInstalledPathForTesting(
    const base::FilePath& base) {
  return GetInstalledPath(base);
}

}  // namespace component_updater
