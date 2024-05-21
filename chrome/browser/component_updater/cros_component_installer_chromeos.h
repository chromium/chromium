// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_CHROMEOS_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_CHROMEOS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/component_updater/ash/component_manager_ash.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"

namespace component_updater {

// A command-line switch that can also be set from chrome://flags for opting in
// or out of DCHECK binaries for Lacros (where available).
extern const char kPreferDcheckSwitch[];
extern const char kPreferDcheckOptIn[];
extern const char kPreferDcheckOptOut[];

// The name of the directory under DIR_COMPONENT_USER that cros component
// installers puts all of the installed components.
extern const char kComponentsRootPath[];

class ComponentUpdateService;
class MetadataTable;
class CrOSComponentInstaller;

// Describes all metadata needed to dynamically install ChromeOS components.
struct ComponentConfig {
  // This is a client-only identifier for the component.
  const char* name;
  // ComponentInstallerPolicy to use.
  enum class PolicyType {
    kEnvVersion,       // Checks env_version, see below.
    kLacros,           // Uses special lacros compatibility rules.
    kDemoApp,          // Adds demo-mode-specific install attributes
    kGrowthCampaigns,  // Adds growth campaigns install attributes
  };
  PolicyType policy_type;
  // This is used for ABI compatibility checks. It is compared against the
  // 'min_env_version' key in the component's manifest.json file. It uses
  // standard major.minor compat rules, where ABI is compatible if and only if
  // major is matching. The client will send this string to the omaha server,
  // which will filter for a compatible update. Likewise, the client will
  // avoid registering a component if there is an ABI mismatch between the
  // already downloaded component and the expected major version. Must be
  // non-empty for PolicyType::kEnvVersion.
  const char* env_version;
  // This is the app-id of the component, converted from [a-p] hex to [0-f] hex.
  const char* sha2hash;
};

// Base class for all Chrome OS components.
class CrOSComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  CrOSComponentInstallerPolicy(
      const ComponentConfig& config,
      CrOSComponentInstaller* cros_component_installer);

  CrOSComponentInstallerPolicy(const CrOSComponentInstallerPolicy&) = delete;
  CrOSComponentInstallerPolicy& operator=(const CrOSComponentInstallerPolicy&) =
      delete;

  ~CrOSComponentInstallerPolicy() override;

  // ComponentInstallerPolicy:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  bool AllowUpdates() const override;

 protected:
  const raw_ptr<CrOSComponentInstaller, DanglingUntriaged>
      cros_component_installer_;

 private:
  const std::string name_;
  std::vector<uint8_t> sha2_hash_;
};

// An installer policy that does ABI compatibility checks based on
// ComponentConfig::env_version, see above.
class EnvVersionInstallerPolicy : public CrOSComponentInstallerPolicy {
 public:
  EnvVersionInstallerPolicy(const ComponentConfig& config,
                            CrOSComponentInstaller* cros_component_installer);
  EnvVersionInstallerPolicy(const EnvVersionInstallerPolicy&) = delete;
  EnvVersionInstallerPolicy& operator=(const EnvVersionInstallerPolicy&) =
      delete;
  ~EnvVersionInstallerPolicy() override;

  // ComponentInstallerPolicy:
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      base::Value::Dict manifest) override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, IsCompatibleOrNot);

  static bool IsCompatible(const std::string& env_version_str,
                           const std::string& min_env_version_str);

  const std::string env_version_;
};

// An installer policy for Lacros components, which have unusual version
// compatibility rules. See ComponentReady() implementation.
class LacrosInstallerPolicy : public CrOSComponentInstallerPolicy {
 public:
  LacrosInstallerPolicy(const ComponentConfig& config,
                        CrOSComponentInstaller* cros_component_installer);
  LacrosInstallerPolicy(const LacrosInstallerPolicy&) = delete;
  LacrosInstallerPolicy& operator=(const LacrosInstallerPolicy&) = delete;
  ~LacrosInstallerPolicy() override;

  // ComponentInstallerPolicy:
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      base::Value::Dict manifest) override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool AllowUpdates() const override;

  static void SetAshVersionForTest(const char* version);
};

// An installer policy for the ChromeOS Demo Mode app, which includes special
// system-sourced installer attributes in the request to receive customized
// app versions
class DemoAppInstallerPolicy : public CrOSComponentInstallerPolicy {
 public:
  DemoAppInstallerPolicy(const ComponentConfig& config,
                         CrOSComponentInstaller* cros_component_installer);
  DemoAppInstallerPolicy(const DemoAppInstallerPolicy&) = delete;
  DemoAppInstallerPolicy& operator=(const DemoAppInstallerPolicy&) = delete;
  ~DemoAppInstallerPolicy() override;

  // ComponentInstallerPolicy:
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      base::Value::Dict manifest) override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

// An installer policy for the ChromeOS growth campaigns, which includes special
// system-sourced installer attributes in the request to receive customized
// campaigns versions.
class GrowthCampaignsInstallerPolicy : public CrOSComponentInstallerPolicy {
 public:
  GrowthCampaignsInstallerPolicy(
      const ComponentConfig& config,
      CrOSComponentInstaller* cros_component_installer);
  GrowthCampaignsInstallerPolicy(const GrowthCampaignsInstallerPolicy&) =
      delete;
  GrowthCampaignsInstallerPolicy& operator=(
      const GrowthCampaignsInstallerPolicy&) = delete;
  ~GrowthCampaignsInstallerPolicy() override;

  // ComponentInstallerPolicy:
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      base::Value::Dict manifest) override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

// This class contains functions used to register and install a component.
class CrOSComponentInstaller : public ComponentManagerAsh {
 public:
  CrOSComponentInstaller(std::unique_ptr<MetadataTable> metadata_table,
                         ComponentUpdateService* component_updater);

  CrOSComponentInstaller(const CrOSComponentInstaller&) = delete;
  CrOSComponentInstaller& operator=(const CrOSComponentInstaller&) = delete;

  // ComponentManagerAsh:
  void SetDelegate(Delegate* delegate) override;
  void Load(const std::string& name,
            MountPolicy mount_policy,
            UpdatePolicy update_policy,
            LoadCallback load_callback) override;
  bool Unload(const std::string& name) override;
  void GetVersion(const std::string& name,
                  base::OnceCallback<void(const base::Version&)>
                      version_callback) const override;
  void RegisterCompatiblePath(const std::string& name,
                              CompatibleComponentInfo info) override;
  void RegisterInstalled() override;

  void UnregisterCompatiblePath(const std::string& name) override;
  base::FilePath GetCompatiblePath(const std::string& name) const override;
  bool IsRegisteredMayBlock(const std::string& name) override;

  // Called when a component is installed/updated.
  // Broadcasts a D-Bus signal for a successful component installation.
  void EmitInstalledSignal(const std::string& component);

  // The load cache contains three pieces of information:
  //   (1) For a given component, whether the load request was successful, a
  //   failure, or in-progress.
  //   (2) If the load request was successful, the file path to the loaded
  //   image.
  //   (3) If the load request is in progress, the callbacks to invoke after the
  //   load request finishes.
  struct LoadInfo {
    LoadInfo();
    ~LoadInfo();
    // If null, then the request is pending.
    std::optional<bool> success;
    // Only populated on success.
    base::FilePath path;
    // Only populated if request is pending. Includes all subsequent callbacks
    // after the first.
    std::vector<LoadCallback> callbacks;
  };

  // Removes the load cache entry for `component_name`. Currently this is done
  // to avoid dispatching loads for old component versions. This can occur when
  // the old version has loaded successfully and is now in the load cache.
  void RemoveLoadCacheEntry(const std::string& component_name);

  // Test-only method for introspection.
  std::map<std::string, LoadInfo>& GetLoadCacheForTesting();

 protected:
  ~CrOSComponentInstaller() override;

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
                  const std::string& name,
                  std::optional<base::FilePath> result);

  // Calls `version_callback` and pass in the parameter `result` (component
  // version).
  void FinishGetVersion(
      base::OnceCallback<void(const base::Version&)> version_callback,
      std::optional<std::string> result) const;

  // Registers component |configs| to be updated.
  void RegisterN(const std::vector<ComponentConfig>& configs);

  // Checks if the current installed component is compatible given a component
  // |name|.
  bool IsCompatible(const std::string& name) const;

  // Posts a task with the response information for |callback|.
  void DispatchLoadCallback(LoadCallback callback,
                            base::FilePath path,
                            bool success);
  // Repeatedly calls DispatchLoadCallback with failure parameters.
  void DispatchFailedLoads(std::vector<LoadCallback> callbacks);

  // Maps from a compatible component name to its info.
  base::flat_map<std::string, CompatibleComponentInfo> compatible_components_;

  // A weak pointer to a Delegate for emitting D-Bus signal.
  raw_ptr<Delegate> delegate_ = nullptr;

  // Table storing metadata (installs, usage, etc.).
  std::unique_ptr<MetadataTable> metadata_table_;

  // The load cache stores ongoing load requests, as well as the finished
  // results.
  std::map<std::string, LoadInfo> load_cache_;

  const raw_ptr<ComponentUpdateService> component_updater_;

  base::WeakPtrFactory<CrOSComponentInstaller> weak_factory_{this};
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_CHROMEOS_H_
