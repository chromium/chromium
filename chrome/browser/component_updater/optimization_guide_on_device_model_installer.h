// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"

namespace optimization_guide {
class OnDeviceModelComponentStateManager;
}  // namespace optimization_guide

namespace component_updater {

// Base class for on-device model installer policies.
class OptimizationGuideOnDeviceModelInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  // `state_manager` has the lifetime till all profiles are closed. It could
  // slightly vary from lifetime of `this` which runs in separate task runner,
  // and could get destroyed slightly later than `state_manager`.
  explicit OptimizationGuideOnDeviceModelInstallerPolicy(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager);
  ~OptimizationGuideOnDeviceModelInstallerPolicy() override;

  // Overrides for ComponentInstallerPolicy.
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override;
  bool AllowCachedCopies() const override;
  bool AllowUpdatesOnMeteredConnections() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static void UpdateOnDemand(const std::string& id,
                             OnDemandUpdater::Priority priority);

 protected:
  // The on-device state manager should be accessed in the UI thread.
  base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
      state_manager_;
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

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
