// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/component_updater/mei_preload_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/media/media_engagement_preloaded_list.h"
#include "components/component_updater/component_updater_paths.h"
#include "media/base/media_switches.h"

using component_updater::ComponentUpdateService;

namespace {

constexpr base::FilePath::CharType kMediaEngagementPreloadBinaryPbFileName[] =
    FILE_PATH_LITERAL("preloaded_data.pb");

// The extension id is: laoigpblnllgcgjnjnllmfolckpjlhki
constexpr uint8_t kMeiPreloadPublicKeySHA256[32] = {
    0xb0, 0xe8, 0x6f, 0x1b, 0xdb, 0xb6, 0x26, 0x9d, 0x9d, 0xbb, 0xc5,
    0xeb, 0x2a, 0xf9, 0xb7, 0xa8, 0x50, 0x35, 0x43, 0x88, 0xc2, 0x09,
    0x04, 0x02, 0xc1, 0xfb, 0x3a, 0xca, 0x7b, 0x11, 0xf9, 0xa9};

constexpr char kMediaEngagementPreloadManifestName[] = "MEI Preload";

void LoadPreloadedDataFromDisk(const base::FilePath& pb_path) {
  DCHECK(!pb_path.empty());
  MediaEngagementPreloadedList::GetInstance()->LoadFromFile(pb_path);
}

}  // namespace

namespace component_updater {

MediaEngagementPreloadComponentInstallerPolicy::
    MediaEngagementPreloadComponentInstallerPolicy(
        base::OnceClosure on_load_closure)
    : on_load_closure_(std::move(on_load_closure)) {}

MediaEngagementPreloadComponentInstallerPolicy::
    ~MediaEngagementPreloadComponentInstallerPolicy() = default;

bool MediaEngagementPreloadComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool MediaEngagementPreloadComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
MediaEngagementPreloadComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void MediaEngagementPreloadComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath MediaEngagementPreloadComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kMediaEngagementPreloadBinaryPbFileName);
}

void MediaEngagementPreloadComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  constexpr base::TaskTraits kTaskTraits = {
      base::MayBlock(), base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  base::OnceClosure task =
      base::BindOnce(&LoadPreloadedDataFromDisk, GetInstalledPath(install_dir));

  if (!on_load_closure_) {
    base::ThreadPool::PostTask(FROM_HERE, kTaskTraits, std::move(task));
  } else {
    base::ThreadPool::PostTaskAndReply(FROM_HERE, kTaskTraits, std::move(task),
                                       std::move(on_load_closure_));
  }
}

// Called during startup and installation before ComponentReady().
bool MediaEngagementPreloadComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in LoadFromFile().
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
MediaEngagementPreloadComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("MEIPreload"));
}

void MediaEngagementPreloadComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(
      kMeiPreloadPublicKeySHA256,
      kMeiPreloadPublicKeySHA256 + std::size(kMeiPreloadPublicKeySHA256));
}

std::string MediaEngagementPreloadComponentInstallerPolicy::GetName() const {
  return kMediaEngagementPreloadManifestName;
}

update_client::InstallerAttributes
MediaEngagementPreloadComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterMediaEngagementPreloadComponent(ComponentUpdateService* cus,
                                             base::OnceClosure on_load) {
  if (!base::FeatureList::IsEnabled(media::kPreloadMediaEngagementData)) {
    return;
  }

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<MediaEngagementPreloadComponentInstallerPolicy>(
          std::move(on_load)));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
