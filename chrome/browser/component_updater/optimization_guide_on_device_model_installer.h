// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

namespace component_updater {

// Base class for on-device model installer policies.
class OptimizationGuideOnDeviceModelInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  // Overrides for ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const final;
  bool RequiresNetworkEncryption() const final;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) final;
  bool AllowCachedCopies() const final;
  bool AllowUpdatesOnMeteredConnections() const final;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static void UpdateOnDemand(const std::string& id,
                             OnDemandUpdater::Priority priority);
};

enum class OnDeviceModelType {
  kBaseModel,
  kClassifierModel,
};

// Returns the extension ID for the optimization guide on-device models.
std::string GetOptimizationGuideOnDeviceModelExtensionId(
    OnDeviceModelType type);

std::unique_ptr<
    optimization_guide::OnDeviceModelComponentStateManager::Delegate>
CreateOptimizationGuideOnDeviceModelComponentDelegate(OnDeviceModelType type);

// Creates a generic delegate for Manifest Component.
std::unique_ptr<optimization_guide::ManifestAssetManager::Delegate>
CreateManifestAssetManagerDelegate();

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
