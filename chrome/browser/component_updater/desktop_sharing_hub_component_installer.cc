// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/desktop_sharing_hub_component_installer.h"

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
#include "chrome/browser/share/core/share_targets.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/browser_thread.h"

using component_updater::ComponentUpdateService;

namespace {

const base::FilePath::CharType kDesktopSharingHubBinaryPbFileName[] =
    FILE_PATH_LITERAL("desktop_sharing_hub.pb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: dhlpobdgcjafebgbbhjdnapejmpkgiie
const uint8_t kDesktopSharingHubPublicKeySHA256[32] = {
    0x37, 0xbf, 0xe1, 0x36, 0x29, 0x05, 0x41, 0x61, 0x17, 0x93, 0xd0,
    0xf4, 0x9c, 0xfa, 0x68, 0x84, 0xa7, 0x6c, 0x79, 0x27, 0x43, 0x97,
    0x26, 0xd6, 0xa1, 0xdb, 0xa1, 0x4c, 0x03, 0xa3, 0x05, 0x27};

const char kDesktopSharingHubManifestName[] = "Desktop Sharing Hub";

absl::optional<std::string> LoadShareTargetsFromDisk(
    const base::FilePath& pb_path) {
  if (pb_path.empty())
    return absl::nullopt;

  VLOG(1) << "Reading Download File Types from file: " << pb_path.value();
  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb)) {
    // ComponentReady will only be called when there is some installation of the
    // component ready, so it would be correct to consider this an error.
    LOG(ERROR) << "Failed reading from " << pb_path.value();
    return absl::nullopt;
  }

  return binary_pb;
}

void OnLoadShareTargetsFromDiskDone(absl::optional<std::string> binary_pb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (binary_pb)
    sharing::ShareTargets::GetInstance()->PopulateFromDynamicUpdate(*binary_pb);
}

}  // namespace

namespace component_updater {

bool DesktopSharingHubComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool DesktopSharingHubComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
DesktopSharingHubComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void DesktopSharingHubComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath DesktopSharingHubComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kDesktopSharingHubBinaryPbFileName);
}

void DesktopSharingHubComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  // The actual disk read is done on a worker thread, but as soon as the read is
  // done the binary protobuf should be posted back to the main thread to
  // maintain the invariant that ShareTargets is only used from the main thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadShareTargetsFromDisk, GetInstalledPath(install_dir)),
      base::BindOnce(&OnLoadShareTargetsFromDiskDone));
}

// Called during startup and installation before ComponentReady().
bool DesktopSharingHubComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in PopulateFromDynamicUpdate().
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
DesktopSharingHubComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("DesktopSharingHub"));
}

void DesktopSharingHubComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kDesktopSharingHubPublicKeySHA256,
               kDesktopSharingHubPublicKeySHA256 +
                   std::size(kDesktopSharingHubPublicKeySHA256));
}

std::string DesktopSharingHubComponentInstallerPolicy::GetName() const {
  return kDesktopSharingHubManifestName;
}

update_client::InstallerAttributes
DesktopSharingHubComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterDesktopSharingHubComponent(ComponentUpdateService* cus) {
  VLOG(1) << "Registering Desktop Sharing Hub component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<DesktopSharingHubComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
