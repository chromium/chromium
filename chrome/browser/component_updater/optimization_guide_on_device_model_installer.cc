// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

using ::optimization_guide::OnDeviceModelComponentStateManager;

namespace component_updater {

namespace {

// Extension id is fklghjjljmnfjoepjmlobpekiapffcja.
constexpr char kManifestName[] = "Optimization Guide On Device Model";
constexpr base::FilePath::CharType kInstallationRelativePath[] =
    FILE_PATH_LITERAL("OptGuideOnDeviceModel");
constexpr uint8_t kPublicKeySHA256[32] = {
    0x5a, 0xb6, 0x79, 0x9b, 0x9c, 0xd5, 0x9e, 0x4f, 0x9c, 0xbe, 0x1f,
    0x4a, 0x80, 0xf5, 0x52, 0x90, 0x74, 0xea, 0x87, 0x3a, 0xf9, 0x91,
    0x00, 0x26, 0x43, 0x86, 0x03, 0x36, 0xa6, 0x38, 0x86, 0x63};
static_assert(std::size(kPublicKeySHA256) == crypto::kSHA256Length);

}  // namespace

OptimizationGuideOnDeviceModelInstallerPolicy::
    OptimizationGuideOnDeviceModelInstallerPolicy(
        scoped_refptr<optimization_guide::OnDeviceModelComponentStateManager>
            state_manager)
    : state_manager_(state_manager) {}

OptimizationGuideOnDeviceModelInstallerPolicy::
    ~OptimizationGuideOnDeviceModelInstallerPolicy() {
  content::GetUIThreadTaskRunner()->ReleaseSoon(FROM_HERE,
                                                std::move(state_manager_));
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return OnDeviceModelComponentStateManager::VerifyInstallation(install_dir,
                                                                manifest);
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::RequiresNetworkEncryption()
    const {
  // This installer is only registered for users who use certain features, and
  // we do not want to expose that they are users of those features.
  return true;
}

update_client::CrxInstaller::Result
OptimizationGuideOnDeviceModelInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void OptimizationGuideOnDeviceModelInstallerPolicy::OnCustomUninstall() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceModelComponentStateManager::UninstallComplete,
                     state_manager_->GetWeakPtr()));
}

void OptimizationGuideOnDeviceModelInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  state_manager_->SetReady(version, install_dir, manifest);
}

base::FilePath
OptimizationGuideOnDeviceModelInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(kInstallationRelativePath);
}

void OptimizationGuideOnDeviceModelInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPublicKeySHA256), std::end(kPublicKeySHA256));
}

std::string OptimizationGuideOnDeviceModelInstallerPolicy::GetName() const {
  return kManifestName;
}

update_client::InstallerAttributes
OptimizationGuideOnDeviceModelInstallerPolicy::GetInstallerAttributes() const {
  return {
      // TODO(b/310740288): Decide on attributes for model variant.
  };
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::AllowCachedCopies() const {
  return false;
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::
    AllowUpdatesOnMeteredConnections() const {
  return false;
}

void RegisterOptimizationGuideOnDeviceModelComponent(
    ComponentUpdateService* cus,
    scoped_refptr<OnDeviceModelComponentStateManager> state_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<OptimizationGuideOnDeviceModelInstallerPolicy>(
          state_manager))
      ->Register(cus,
                 base::BindOnce(
                     &OnDeviceModelComponentStateManager::InstallerRegistered,
                     state_manager->GetWeakPtr()));
}

void UninstallOptimizationGuideOnDeviceModelComponent(
    scoped_refptr<OnDeviceModelComponentStateManager> state_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<OptimizationGuideOnDeviceModelInstallerPolicy>(
          state_manager))
      ->Uninstall();
}

}  // namespace component_updater
