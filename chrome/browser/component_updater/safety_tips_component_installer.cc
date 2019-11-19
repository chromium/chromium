// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/safety_tips_component_installer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/reputation/safety_tips.pb.h"
#include "chrome/browser/reputation/safety_tips_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using component_updater::ComponentUpdateService;

namespace {

const base::FilePath::CharType kSafetyTipsConfigBinaryPbFileName[] =
    FILE_PATH_LITERAL("safety_tips.pb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: jflookgnkcckhobaglndicnbbgbonegd
const uint8_t kSafetyTipsPublicKeySHA256[32] = {
    0x95, 0xbe, 0xea, 0x6d, 0xa2, 0x2a, 0x7e, 0x10, 0x6b, 0xd3, 0x82,
    0xd1, 0x16, 0x1e, 0xd4, 0x63, 0x21, 0xfe, 0x79, 0x5d, 0x02, 0x30,
    0xc2, 0xcf, 0x4a, 0x9c, 0x8a, 0x39, 0xcc, 0x4a, 0x00, 0xce};

std::unique_ptr<chrome_browser_safety_tips::SafetyTipsConfig>
LoadSafetyTipsProtoFromDisk(const base::FilePath& pb_path) {
  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    DVLOG(1) << "Failed reading from " << pb_path.value();
    return nullptr;
  }
  auto proto = std::make_unique<chrome_browser_safety_tips::SafetyTipsConfig>();
  if (!proto->ParseFromString(binary_pb)) {
    DVLOG(1) << "Failed parsing proto " << pb_path.value();
    return nullptr;
  }
  return proto;
}

}  // namespace

namespace component_updater {

SafetyTipsComponentInstallerPolicy::SafetyTipsComponentInstallerPolicy() =
    default;

SafetyTipsComponentInstallerPolicy::~SafetyTipsComponentInstallerPolicy() =
    default;

bool SafetyTipsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool SafetyTipsComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
SafetyTipsComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void SafetyTipsComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath SafetyTipsComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kSafetyTipsConfigBinaryPbFileName);
}

void SafetyTipsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> /* manifest */) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "Component ready, version " << version.GetString() << " in "
           << install_dir.value();

  const base::FilePath pb_path = GetInstalledPath(install_dir);
  if (pb_path.empty())
    return;

  // The default proto will always be a placeholder since the updated versions
  // are not checked in to the repo. Simply load whatever the component updater
  // gave us without checking the default proto from the resource bundle.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadSafetyTipsProtoFromDisk, pb_path),
      base::BindOnce(&SetSafetyTipsRemoteConfigProto));
}

// Called during startup and installation before ComponentReady().
bool SafetyTipsComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& /* manifest */,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in PopulateFromDynamicUpdate().
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath SafetyTipsComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("SafetyTips"));
}

void SafetyTipsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(
      kSafetyTipsPublicKeySHA256,
      kSafetyTipsPublicKeySHA256 + base::size(kSafetyTipsPublicKeySHA256));
}

std::string SafetyTipsComponentInstallerPolicy::GetName() const {
  return "Safety Tips";
}

update_client::InstallerAttributes
SafetyTipsComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> SafetyTipsComponentInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void RegisterSafetyTipsComponent(ComponentUpdateService* cus,
                                 const base::FilePath& /* user_data_dir */) {
  DVLOG(1) << "Registering Safety Tips component.";

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<SafetyTipsComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
