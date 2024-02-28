// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/app_provisioning_component_installer.h"

#include <stdint.h>

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/browser_thread.h"

namespace {

constexpr base::FilePath::CharType kAppWithLocaleBinaryPbFileName[] =
    FILE_PATH_LITERAL("app_data.textproto");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: fellaebeeieagcalnmmpapfioejgihci
constexpr uint8_t kAppProvisioningPublicKeySHA256[32] = {
    0x54, 0xbb, 0x04, 0x14, 0x48, 0x40, 0x62, 0x0b, 0xdc, 0xcf, 0x0f,
    0x58, 0xe4, 0x96, 0x87, 0x28, 0x52, 0x36, 0x7f, 0x5f, 0x5c, 0xcf,
    0xc5, 0x4c, 0xf5, 0xb9, 0x77, 0x25, 0x74, 0xce, 0xa1, 0xb3};

constexpr char kAppProvisioningManifestName[] = "App Provisioning";
}  // namespace

namespace component_updater {

// Called during startup and installation before ComponentReady().
bool AppProvisioningComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in `PopulateFromDynamicUpdate()`.
  return base::PathExists(GetAppWithLocaleInstalledPath(install_dir));
}

bool AppProvisioningComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool AppProvisioningComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
AppProvisioningComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void AppProvisioningComponentInstallerPolicy::OnCustomUninstall() {}

void AppProvisioningComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();
}

base::FilePath AppProvisioningComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("AppProvisioning"));
}

void AppProvisioningComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kAppProvisioningPublicKeySHA256),
               std::end(kAppProvisioningPublicKeySHA256));
}

std::string AppProvisioningComponentInstallerPolicy::GetName() const {
  return kAppProvisioningManifestName;
}

update_client::InstallerAttributes
AppProvisioningComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

base::FilePath
AppProvisioningComponentInstallerPolicy::GetAppWithLocaleInstalledPath(
    const base::FilePath& base) {
  return base.Append(kAppWithLocaleBinaryPbFileName);
}

void RegisterAppProvisioningComponent(
    component_updater::ComponentUpdateService* cus) {
  if (chromeos::features::IsCloudGamingDeviceEnabled()) {
    VLOG(1) << "Registering App Provisioning component.";
    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<AppProvisioningComponentInstallerPolicy>());
    installer->Register(cus, base::OnceClosure());
  }
}

}  // namespace component_updater
