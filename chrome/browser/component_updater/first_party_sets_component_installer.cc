// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/first_party_sets_component_installer.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"

using component_updater::ComponentUpdateService;

namespace {

constexpr base::FilePath::CharType kFirstPartySetsSetsFileName[] =
    FILE_PATH_LITERAL("sets.json");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: gonpemdgkjcecdgbnaabipppbmgfggbe
constexpr uint8_t kFirstPartySetsPublicKeySHA256[32] = {
    0x6e, 0xdf, 0x4c, 0x36, 0xa9, 0x24, 0x23, 0x61, 0xd0, 0x01, 0x8f,
    0xff, 0x1c, 0x65, 0x66, 0x14, 0xa8, 0x46, 0x37, 0xe6, 0xeb, 0x80,
    0x8b, 0x8f, 0xb0, 0xb6, 0x18, 0xa7, 0xcd, 0x3d, 0xbb, 0xfb};

constexpr char kFirstPartySetsManifestName[] = "First-Party Sets";

constexpr base::FilePath::CharType kFirstPartySetsRelativeInstallDir[] =
    FILE_PATH_LITERAL("FirstPartySetsPreloaded");

// Reads the sets as raw JSON from their storage file, returning the raw sets on
// success and nullopt on failure.
base::Optional<std::string> LoadSetsFromDisk(const base::FilePath& pb_path) {
  if (pb_path.empty())
    return base::nullopt;

  VLOG(1) << "Reading First-Party Sets from file: " << pb_path.value();
  std::string result;
  if (!base::ReadFileToString(pb_path, &result)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    VLOG(1) << "Failed reading from " << pb_path.value();
    return base::nullopt;
  }
  return result;
}

}  // namespace

namespace component_updater {

FirstPartySetsComponentInstallerPolicy::FirstPartySetsComponentInstallerPolicy(
    base::RepeatingCallback<void(const std::string&)> on_sets_ready)
    : on_sets_ready_(std::move(on_sets_ready)) {}

FirstPartySetsComponentInstallerPolicy::
    ~FirstPartySetsComponentInstallerPolicy() = default;

bool FirstPartySetsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  // False since this is a data, non-binary component.
  return false;
}

bool FirstPartySetsComponentInstallerPolicy::RequiresNetworkEncryption() const {
  // Update checks and pings associated with this component do not require
  // confidentiality, since the component is identical for all users.
  return false;
}

update_client::CrxInstaller::Result
FirstPartySetsComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void FirstPartySetsComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kFirstPartySetsSetsFileName);
}

void FirstPartySetsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "First-Party Sets Component ready, version " << version.GetString()
          << " in " << install_dir.value();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadSetsFromDisk, GetInstalledPath(install_dir)),
      base::BindOnce(
          [](base::RepeatingCallback<void(const std::string&)> on_sets_ready,
             base::Optional<std::string> raw_sets) {
            if (raw_sets.has_value())
              on_sets_ready.Run(*raw_sets);
          },
          on_sets_ready_));
}

// Called during startup and installation before ComponentReady().
bool FirstPartySetsComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the sets here, since we'll do the validation
  // in the Network Service.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath FirstPartySetsComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kFirstPartySetsRelativeInstallDir);
}

void FirstPartySetsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kFirstPartySetsPublicKeySHA256,
               kFirstPartySetsPublicKeySHA256 +
                   base::size(kFirstPartySetsPublicKeySHA256));
}

std::string FirstPartySetsComponentInstallerPolicy::GetName() const {
  return kFirstPartySetsManifestName;
}

update_client::InstallerAttributes
FirstPartySetsComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> FirstPartySetsComponentInstallerPolicy::GetMimeTypes()
    const {
  return {};
}

void RegisterFirstPartySetsComponent(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(network::features::kFirstPartySets))
    return;
  VLOG(1) << "Registering First-Party Sets component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<FirstPartySetsComponentInstallerPolicy>(
          /*on_sets_ready=*/base::BindRepeating(
              [](const std ::string& raw_sets) {
                VLOG(1) << "Received Sets: \"" << raw_sets << "\"";
                content::GetNetworkService()->SetPreloadedFirstPartySets(
                    raw_sets);
              })));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
