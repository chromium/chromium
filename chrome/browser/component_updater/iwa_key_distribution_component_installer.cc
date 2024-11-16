// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_installer.h"
#include "components/update_client/update_client.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: iebhnlpddlcpcfpfalldikcoeakpeoah
constexpr std::array<uint8_t, 32> kIwaKeyDistributionPublicKeySHA256 = {
    0x84, 0x17, 0xdb, 0xf3, 0x3b, 0x2f, 0x25, 0xf5, 0x0b, 0xb3, 0x8a,
    0x2e, 0x40, 0xaf, 0x4e, 0x07, 0x18, 0xfa, 0xae, 0x6e, 0x0e, 0xdb,
    0x46, 0xfc, 0xc9, 0x36, 0x50, 0xcf, 0x38, 0xfa, 0xf9, 0xab};

}  // namespace

namespace component_updater {

BASE_FEATURE(kIwaKeyDistributionComponent,
             "IwaKeyDistributionComponent",
             base::FEATURE_DISABLED_BY_DEFAULT);

IwaKeyDistributionComponentInstallerPolicy::
    IwaKeyDistributionComponentInstallerPolicy(
        ComponentReadyCallback on_component_ready)
    : on_component_ready_(std::move(on_component_ready)) {}

IwaKeyDistributionComponentInstallerPolicy::
    ~IwaKeyDistributionComponentInstallerPolicy() = default;

bool IwaKeyDistributionComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kDataFileName));
}

bool IwaKeyDistributionComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool IwaKeyDistributionComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
IwaKeyDistributionComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  // No custom install.
  return update_client::CrxInstaller::Result(0);
}

void IwaKeyDistributionComponentInstallerPolicy::OnCustomUninstall() {}

void IwaKeyDistributionComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  if (install_dir.empty() || !version.IsValid()) {
    return;
  }
  VLOG(1) << "Iwa Key Distribution Component ready, version " << version
          << " in " << install_dir;
  on_component_ready_.Run(version, install_dir.Append(kDataFileName));
}

base::FilePath
IwaKeyDistributionComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("IwaKeyDistribution"));
}

void IwaKeyDistributionComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kIwaKeyDistributionPublicKeySHA256),
               std::end(kIwaKeyDistributionPublicKeySHA256));
}

std::string IwaKeyDistributionComponentInstallerPolicy::GetName() const {
  return kManifestName;
}

update_client::InstallerAttributes
IwaKeyDistributionComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterIwaKeyDistributionComponent(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(kIwaKeyDistributionComponent)) {
    return;
  }

  using Handler = web_app::IwaKeyDistributionInfoProvider;

  // `base::Unretained()` is safe here as IwaKeyDistributionInfoProvider is a
  // singleton that never goes away.
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<IwaKeyDistributionComponentInstallerPolicy>(
          base::BindRepeating(&Handler::LoadKeyDistributionData,
                              base::Unretained(Handler::GetInstance()))),
      /*action_handler=*/nullptr, base::TaskPriority::BEST_EFFORT)
      ->Register(cus, base::DoNothing());
}

}  // namespace component_updater
