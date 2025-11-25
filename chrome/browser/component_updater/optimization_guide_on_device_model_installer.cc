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
#include "base/logging.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

namespace component_updater {
namespace {

using ::optimization_guide::OnDeviceModelComponentStateManager;

// Extension id is fklghjjljmnfjoepjmlobpekiapffcja.
constexpr char kManifestName[] = "Optimization Guide On Device Model";
constexpr base::FilePath::CharType kInstallationRelativePath[] =
    FILE_PATH_LITERAL("OptGuideOnDeviceModel");
constexpr uint8_t kPublicKeySHA256[32] = {
    0x5a, 0xb6, 0x79, 0x9b, 0x9c, 0xd5, 0x9e, 0x4f, 0x9c, 0xbe, 0x1f,
    0x4a, 0x80, 0xf5, 0x52, 0x90, 0x74, 0xea, 0x87, 0x3a, 0xf9, 0x91,
    0x00, 0x26, 0x43, 0x86, 0x03, 0x36, 0xa6, 0x38, 0x86, 0x63};
static_assert(std::size(kPublicKeySHA256) == crypto::kSHA256Length);

bool IsOnDeviceModelAlreadyInstalled(ComponentUpdateService* cus) {
  CrxUpdateItem update_item;
  bool success =
      cus->GetComponentDetails(OptimizationGuideOnDeviceModelInstallerPolicy::
                                   GetOnDeviceModelExtensionId(),
                               &update_item);
  return success && update_item.component.has_value() &&
         update_item.component->version.IsValid() &&
         update_item.component->version.CompareToWildcardString("0.0.0.0") > 0;
}

}  // namespace

OptimizationGuideOnDeviceModelInstallerPolicy::
    OptimizationGuideOnDeviceModelInstallerPolicy(
        base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
            state_manager,
        optimization_guide::OnDeviceModelRegistrationAttributes attributes)
    : state_manager_(state_manager), attributes_(std::move(attributes)) {}

OptimizationGuideOnDeviceModelInstallerPolicy::
    ~OptimizationGuideOnDeviceModelInstallerPolicy() = default;

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
                     state_manager_));
}

void OptimizationGuideOnDeviceModelInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  if (state_manager_) {
    state_manager_->SetReady(version, install_dir, manifest);
  }
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
  using Hint = optimization_guide::proto::OnDeviceModelPerformanceHint;
  base::flat_set<Hint> hints{attributes_.supported_hints};
  return {
      {"cpu_support", hints.contains(Hint::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU)
                          ? "yes"
                          : "no"},
      {"highest_quality_support",
       hints.contains(Hint::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY)
           ? "yes"
           : "no"},
      {"fastest_inference_support",
       hints.contains(Hint::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE)
           ? "yes"
           : "no"},
  };
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::AllowCachedCopies() const {
  return false;
}

bool OptimizationGuideOnDeviceModelInstallerPolicy::
    AllowUpdatesOnMeteredConnections() const {
  return false;
}

// static
const std::string
OptimizationGuideOnDeviceModelInstallerPolicy::GetOnDeviceModelExtensionId() {
  return crx_file::id_util::GenerateIdFromHash(kPublicKeySHA256);
}

// static
void OptimizationGuideOnDeviceModelInstallerPolicy::UpdateOnDemand() {
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      GetOnDeviceModelExtensionId(),
      component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS) {
          LOG(ERROR) << "Failed to update on-device model component with error "
                     << static_cast<int>(error);
        }
      }));
}

void RegisterOptimizationGuideOnDeviceModelComponent(
    ComponentUpdateService* cus,
    base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
    optimization_guide::OnDeviceModelRegistrationAttributes attributes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto register_callback = base::BindOnce(
      [](base::WeakPtr<OnDeviceModelComponentStateManager> state_manager,
         ComponentUpdateService* cus) {
        if (!IsOnDeviceModelAlreadyInstalled(cus)) {
          // If we don't have ANY usable model, trigger an on-demand update
          // so that we can get one more quickly.
          OptimizationGuideOnDeviceModelInstallerPolicy::UpdateOnDemand();
        }
        if (state_manager) {
          state_manager->InstallerRegistered();
        }
      },
      state_manager->GetWeakPtr(), cus);
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<OptimizationGuideOnDeviceModelInstallerPolicy>(
          state_manager, std::move(attributes)))
      ->Register(cus, std::move(register_callback));
}

void UninstallOptimizationGuideOnDeviceModelComponent(
    base::WeakPtr<OnDeviceModelComponentStateManager> state_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<OptimizationGuideOnDeviceModelInstallerPolicy>(
          state_manager,
          // Attributes don't matter for uninstall.
          optimization_guide::OnDeviceModelRegistrationAttributes({})))
      ->Uninstall();
}

}  // namespace component_updater
