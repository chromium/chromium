// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_

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

// Installer policy for the On-Device Base Model.
class OptimizationGuideOnDeviceBaseModelInstallerPolicy
    : public OptimizationGuideOnDeviceModelInstallerPolicy {
 public:
  explicit OptimizationGuideOnDeviceBaseModelInstallerPolicy(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager,
      optimization_guide::OnDeviceModelRegistrationAttributes attributes);
  ~OptimizationGuideOnDeviceBaseModelInstallerPolicy() override;

  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static const std::string GetOnDeviceModelExtensionId();
  static void UpdateOnDemand(OnDemandUpdater::Priority priority);

 private:
  const optimization_guide::OnDeviceModelRegistrationAttributes attributes_;
};

// Installer policy for the On-Device Classifier Model.
class OptimizationGuideOnDeviceClassifierModelInstallerPolicy
    : public OptimizationGuideOnDeviceModelInstallerPolicy {
 public:
  explicit OptimizationGuideOnDeviceClassifierModelInstallerPolicy(
      base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
          state_manager);
  ~OptimizationGuideOnDeviceClassifierModelInstallerPolicy() override;

  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;

  static const std::string GetExtensionId();
  static void UpdateOnDemand();
};

// Register the on-device base model component, initiating download if needed.
void RegisterOptimizationGuideOnDeviceBaseModelComponent(
    ComponentUpdateService* cus,
    base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
        state_manager,
    optimization_guide::OnDeviceModelRegistrationAttributes attributes);

// Requests uninstallation of the on-device base model component.
void UninstallOptimizationGuideOnDeviceBaseModelComponent(
    base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
        state_manager);

// Register the on-device classifier model component, initiating download if
// needed.
void RegisterOptimizationGuideOnDeviceClassifierModelComponent(
    ComponentUpdateService* cus,
    base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
        state_manager);

// Requests uninstallation of the on-device classifier model component.
void UninstallOptimizationGuideOnDeviceClassifierModelComponent(
    base::WeakPtr<optimization_guide::OnDeviceModelComponentStateManager>
        state_manager);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_OPTIMIZATION_GUIDE_ON_DEVICE_MODEL_INSTALLER_H_
