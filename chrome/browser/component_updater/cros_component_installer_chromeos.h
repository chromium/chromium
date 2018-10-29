// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_CHROMEOS_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"

namespace component_updater {

class ComponentUpdateService;
class MetadataTable;
class CrOSComponentInstaller;

struct ComponentConfig {
  const char* name;
  const char* env_version;
  const char* sha2hash;
};

class CrOSComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  CrOSComponentInstallerPolicy(
      const ComponentConfig& config,
      CrOSComponentInstaller* cros_component_installer);
  ~CrOSComponentInstallerPolicy() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, IsCompatibleOrNot);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, CompatibilityOK);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest,
                           CompatibilityMissingManifest);

  // ComponentInstallerPolicy:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

  // This is virtual so unit tests can override it.
  virtual bool IsCompatible(const std::string& env_version_str,
                            const std::string& min_env_version_str);

  CrOSComponentInstaller* const cros_component_installer_;

  const std::string name_;
  const std::string env_version_;
  std::vector<uint8_t> sha2_hash_;

  DISALLOW_COPY_AND_ASSIGN(CrOSComponentInstallerPolicy);
};

// This class contains functions used to register and install a component.
class CrOSComponentInstaller : public CrOSComponentManager {
 public:
  CrOSComponentInstaller(std::unique_ptr<MetadataTable> metadata_table,
                         ComponentUpdateService* component_updater);
  ~CrOSComponentInstaller() override;

  // CrOSComponentManager:
  void SetDelegate(Delegate* delegate) override;
  void Load(const std::string& name,
            MountPolicy mount_policy,
            UpdatePolicy update_policy,
            LoadCallback load_callback) override;
  bool Unload(const std::string& name) override;
  void RegisterCompatiblePath(const std::string& name,
                              const base::FilePath& path) override;
  void RegisterInstalled() override;

  void UnregisterCompatiblePath(const std::string& name) override;
  base::FilePath GetCompatiblePath(const std::string& name) const override;
  bool IsRegistered(const std::string& name) const override;

  // Called when a component is installed/updated.
  // Broadcasts a D-Bus signal for a successful component installation.
  void EmitInstalledSignal(const std::string& component);


 private:
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, RegisterComponent);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest,
                           BPPPCompatibleCrOSComponent);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, CompatibilityOK);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest,
                           CompatibilityMissingManifest);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, IsCompatibleOrNot);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, CompatibleCrOSComponent);

  // Registers a component with a dedicated ComponentUpdateService instance.
  void Register(const ComponentConfig& config,
                base::OnceClosure register_callback);

  // Installs a component with a dedicated ComponentUpdateService instance.
  void Install(const std::string& name,
               UpdatePolicy update_policy,
               MountPolicy mount_policy,
               LoadCallback load_callback);

  // Calls OnDemandUpdate to install the component right after being registered.
  // |id| is the component id generated from its sha2 hash.
  void StartInstall(const std::string& name,
                    const std::string& id,
                    UpdatePolicy update_policy,
                    update_client::Callback install_callback);

  // Calls LoadInternal to load the installed component.
  void FinishInstall(const std::string& name,
                     MountPolicy mount_policy,
                     UpdatePolicy update_policy,
                     LoadCallback load_callback,
                     update_client::Error error);

  // Internal function to load a component.
  void LoadInternal(const std::string& name, LoadCallback load_callback);

  // Calls load_callback and pass in the parameter |result| (component mount
  // point).
  void FinishLoad(LoadCallback load_callback,
                  const base::TimeTicks start_time,
                  const std::string& name,
                  base::Optional<base::FilePath> result);

  // Registers component |configs| to be updated.
  void RegisterN(const std::vector<ComponentConfig>& configs);

  // Checks if the current installed component is compatible given a component
  // |name|.
  bool IsCompatible(const std::string& name) const;

  // Maps from a compatible component name to its installed path.
  base::flat_map<std::string, base::FilePath> compatible_components_;

  // A weak pointer to a Delegate for emitting D-Bus signal.
  Delegate* delegate_ = nullptr;

  // Table storing metadata (installs, usage, etc.).
  std::unique_ptr<MetadataTable> metadata_table_;

  ComponentUpdateService* const component_updater_;

  DISALLOW_COPY_AND_ASSIGN(CrOSComponentInstaller);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_CHROMEOS_H_
