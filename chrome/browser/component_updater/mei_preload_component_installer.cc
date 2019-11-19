// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/mei_preload_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "chrome/browser/media/media_engagement_preloaded_list.h"
#include "components/component_updater/component_updater_paths.h"
#include "media/base/media_switches.h"

using component_updater::ComponentUpdateService;

namespace {

constexpr base::FilePath::CharType kMediaEngagementPreloadBinaryPbFileName[] =
    FILE_PATH_LITERAL("preloaded_data.pb");

// The extension id is: aemomkdncapdnfajjbbcbdebjljbpmpj
constexpr uint8_t kMeiPreloadPublicKeySHA256[32] = {
    0x04, 0xce, 0xca, 0x3d, 0x20, 0xf3, 0xd5, 0x09, 0x91, 0x12, 0x13,
    0x41, 0x9b, 0x91, 0xfc, 0xf9, 0x19, 0xc4, 0x94, 0x6a, 0xb9, 0x9a,
    0xe1, 0xaf, 0x3b, 0x9a, 0x95, 0x85, 0x5b, 0x9e, 0x99, 0xed};

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
  return false;
}

bool MediaEngagementPreloadComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
MediaEngagementPreloadComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
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
    std::unique_ptr<base::DictionaryValue> manifest) {
  base::TaskTraits task_traits = {
      base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  base::OnceClosure task =
      base::BindOnce(&LoadPreloadedDataFromDisk, GetInstalledPath(install_dir));

  if (!on_load_closure_) {
    base::PostTask(FROM_HERE, task_traits, std::move(task));
  } else {
    base::PostTaskAndReply(FROM_HERE, task_traits, std::move(task),
                           std::move(on_load_closure_));
  }
}

// Called during startup and installation before ComponentReady().
bool MediaEngagementPreloadComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
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
      kMeiPreloadPublicKeySHA256 + base::size(kMeiPreloadPublicKeySHA256));
}

std::string MediaEngagementPreloadComponentInstallerPolicy::GetName() const {
  return kMediaEngagementPreloadManifestName;
}

update_client::InstallerAttributes
MediaEngagementPreloadComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string>
MediaEngagementPreloadComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterMediaEngagementPreloadComponent(ComponentUpdateService* cus,
                                             base::OnceClosure on_load) {
  if (!base::FeatureList::IsEnabled(media::kPreloadMediaEngagementData))
    return;

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<MediaEngagementPreloadComponentInstallerPolicy>(
          std::move(on_load)));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
