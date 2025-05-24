// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_

#include "base/memory/weak_ptr.h"
#include "components/component_updater/component_installer.h"

namespace optimization_guide {
class OnDeviceModelComponentStateManager;
}  // namespace optimization_guide

namespace component_updater {

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
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  bool AllowCachedCopies() const override;
  bool AllowUpdatesOnMeteredConnections() const override;
  static const std::string GetOnDeviceModelExtensionId();
  static void UpdateOnDemand();

 private:
  // The on-device state manager should be accessed in the UI thread.
  base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
      state_manager_;
};

void RegisterOptimizationGuideOnDeviceModelComponent(
    ComponentUpdateService* cus,
    base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
        state_manager,
    bool is_already_installing);

void UninstallOptimizationGuideOnDeviceModelComponent(
    base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
        state_manager);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
