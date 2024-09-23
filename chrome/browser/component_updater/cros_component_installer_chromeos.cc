// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"

#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_installer_errors.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/image_loader/image_loader_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

namespace component_updater {

// Switch that can be used for opting in to receive DCHECK-enabled binaries. If
// we need to expose this through chrome://flags on other platforms this can
// move to a shared place (but still share the prefer-dcheck name).
const char kPreferDcheckSwitch[] = "prefer-dcheck";
const char kPreferDcheckOptIn[] = "opt-in";
const char kPreferDcheckOptOut[] = "opt-out";

// Root path where all components are stored.
constexpr char kComponentsRootPath[] = "cros-components";

namespace {

// All downloadable Chrome OS components.
const ComponentConfig kConfigs[] = {
    {"cros-termina", ComponentConfig::PolicyType::kEnvVersion, "980.1",
     "e9d960f84f628e1f42d05de4046bb5b3154b6f1f65c08412c6af57a29aecaffb"},
    {"rtanalytics-full", ComponentConfig::PolicyType::kEnvVersion, "106.0",
     "c93c3e1013c52100a20038b405ac854d69fa889f6dc4fa6f188267051e05e444"},
    {"demo-mode-resources", ComponentConfig::PolicyType::kEnvVersion, "1.0",
     "93c093ebac788581389015e9c59c5af111d2fa5174d206eb795042e6376cbd10"},
    {"demo-mode-app", ComponentConfig::PolicyType::kDemoApp, nullptr,
     "b6c5ce9f03b0ce830eb5f9f92ed3016cfdb7a2327330f0187adbe9a00ddfd34d"},
    // NOTE: If you change the lacros component names, you must also update
    // chrome/browser/ash/crosapi/browser_loader.cc.
    {"lacros-dogfood-canary", ComponentConfig::PolicyType::kLacros, nullptr,
     "7a85ffb4b316a3b89135a3f43660ef3049950a61a2f8df4237e1ec213852b848"},
    {"lacros-dogfood-dev", ComponentConfig::PolicyType::kLacros, nullptr,
     "b3e1ef1780c0acd2d3fa44b4d73c657a0f1ed3ad83fd8c964a18a3502ccf5f4f"},
    {"lacros-dogfood-beta", ComponentConfig::PolicyType::kLacros, nullptr,
     "7d5c1428f7f67b56f95123851adec1da105980c56b5c126352040f3b65d3e43b"},
    {"lacros-dogfood-stable", ComponentConfig::PolicyType::kLacros, nullptr,
     "47f910805afac79e2d4d9117c42d5291a32ac60a4ea1a42e537fd86082c3ba48"},
    {"growth-campaigns", ComponentConfig::PolicyType::kGrowthCampaigns, nullptr,
     "36448796af5fb67380ec0180a8379ddd26fce20d3da6a231e0a60dfe2360407e"},
};

const char* g_ash_version_for_test = nullptr;

// Returns the major version of the current binary, which is the ash/OS binary.
// For example, for ash 89.0.1234.1 returns 89.
uint32_t GetAshMajorVersion() {
  base::Version ash_version = g_ash_version_for_test
                                  ? base::Version(g_ash_version_for_test)
                                  : version_info::GetVersion();
  return ash_version.components()[0];
}

const ComponentConfig* FindConfig(const std::string& name) {
  const ComponentConfig* config =
      base::ranges::find(kConfigs, name, &ComponentConfig::name);
  if (config == std::end(kConfigs)) {
    return nullptr;
  }
  return config;
}

void LogCustomUninstall(std::optional<bool> result) {}

void FinishCustomUninstallOnUIThread(const std::string& name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ash::ImageLoaderClient::Get()->UnmountComponent(
      name, base::BindOnce(&LogCustomUninstall));
}

std::string GenerateId(const std::string& sha2hashstr) {
  // kIdSize is the count of a pair of hex in the sha2hash array.
  // In string representation of sha2hash, size is doubled since each hex is
  // represented by a single char.
  return crx_file::id_util::GenerateIdFromHex(
      sha2hashstr.substr(0, crx_file::id_util::kIdSize * 2));
}

// Returns all installed components.
std::vector<ComponentConfig> GetInstalled() {
  std::vector<ComponentConfig> configs;
  base::FilePath root;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &root)) {
    return configs;
  }

  root = root.Append(kComponentsRootPath);
  for (const ComponentConfig& config : kConfigs) {
    base::FilePath component_path = root.Append(config.name);
    if (base::PathExists(component_path)) {
      configs.push_back(config);
    }
  }
  return configs;
}

const bool kDefaultLacrosAllowUpdates = true;

// Report Error code.
void ReportError(ComponentManagerAsh::Error error) {
  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.InstallResult", error);
}

}  // namespace

CrOSComponentInstallerPolicy::CrOSComponentInstallerPolicy(
    const ComponentConfig& config,
    CrOSComponentInstaller* cros_component_installer)
    : cros_component_installer_(cros_component_installer), name_(config.name) {
  if (strlen(config.sha2hash) != crypto::kSHA256Length * 2) {
    return;
  }

  bool converted = base::HexStringToBytes(config.sha2hash, &sha2_hash_);
  DCHECK(converted);
  DCHECK_EQ(crypto::kSHA256Length, sha2_hash_.size());
}

CrOSComponentInstallerPolicy::~CrOSComponentInstallerPolicy() = default;

bool CrOSComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool CrOSComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return true;
}

bool CrOSComponentInstallerPolicy::AllowUpdates() const {
  return true;
}

update_client::CrxInstaller::Result
CrOSComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  cros_component_installer_->EmitInstalledSignal(GetName());

  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void CrOSComponentInstallerPolicy::OnCustomUninstall() {
  cros_component_installer_->UnregisterCompatiblePath(name_);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FinishCustomUninstallOnUIThread, name_));
}

bool CrOSComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath CrOSComponentInstallerPolicy::GetRelativeInstallDir() const {
  base::FilePath path = base::FilePath(kComponentsRootPath);
  return path.Append(name_);
}

void CrOSComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  *hash = sha2_hash_;
}

std::string CrOSComponentInstallerPolicy::GetName() const {
  return name_;
}

EnvVersionInstallerPolicy::EnvVersionInstallerPolicy(
    const ComponentConfig& config,
    CrOSComponentInstaller* cros_component_installer)
    : CrOSComponentInstallerPolicy(config, cros_component_installer),
      env_version_(config.env_version) {
  DCHECK(!env_version_.empty());
}

EnvVersionInstallerPolicy::~EnvVersionInstallerPolicy() = default;

void EnvVersionInstallerPolicy::ComponentReady(const base::Version& version,
                                               const base::FilePath& path,
                                               base::Value::Dict manifest) {
  std::string* min_env_version = manifest.FindString("min_env_version");
  if (!min_env_version) {
    return;
  }

  if (!IsCompatible(env_version_, *min_env_version)) {
    return;
  }

  cros_component_installer_->RegisterCompatiblePath(
      GetName(), CompatibleComponentInfo(path, version));
}

update_client::InstallerAttributes
EnvVersionInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attrs;
  attrs["_env_version"] = env_version_;
  return attrs;
}

// static
bool EnvVersionInstallerPolicy::IsCompatible(
    const std::string& env_version_str,
    const std::string& min_env_version_str) {
  base::Version env_version(env_version_str);
  base::Version min_env_version(min_env_version_str);
  return env_version.IsValid() && min_env_version.IsValid() &&
         env_version.components()[0] == min_env_version.components()[0] &&
         env_version >= min_env_version;
}

LacrosInstallerPolicy::LacrosInstallerPolicy(
    const ComponentConfig& config,
    CrOSComponentInstaller* cros_component_installer)
    : CrOSComponentInstallerPolicy(config, cros_component_installer) {}

LacrosInstallerPolicy::~LacrosInstallerPolicy() = default;

void LacrosInstallerPolicy::ComponentReady(const base::Version& version,
                                           const base::FilePath& path,
                                           base::Value::Dict manifest) {
  // Each version of Lacros guarantees it will be compatible through the same
  // major ash/OS version and -2. For example, Lacros 89 will work with ash/OS
  // 89, 88, and 87. But it may not work with ash/OS 86 or 90.
  //
  // As you see we (client side) only enforces the Lacros/Ash same version
  // check here, while the code does not check the -2 version skew requirement.
  // This is because go/lacros-version-skew-guide mentions the restriction on
  // lacros being too new is enforced on the Omaha server side - and the too
  // old check is enforced client side. Supposedly this makes it easy for us to
  // start supporting newer lacros versions by just updating the Omaha server
  // code.
  uint32_t lacros_major_version = version.components()[0];
  if (lacros_major_version < GetAshMajorVersion()) {
    // Current lacros install is not compatible.
    return;
  }
  cros_component_installer_->RegisterCompatiblePath(
      GetName(), CompatibleComponentInfo(path, version));

  // Clear the load cache for the newly installed component version to avoid
  // loading stale components on successive loads, causing a version update
  // restart loop (see crbug.com/1322678).
  cros_component_installer_->RemoveLoadCacheEntry(GetName());
}

update_client::InstallerAttributes
LacrosInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;
  auto* const cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(kPreferDcheckSwitch)) {
    attributes[kPreferDcheckSwitch] =
        cmdline->GetSwitchValueASCII(kPreferDcheckSwitch);
  }
  return attributes;
}

// Lacros is supposed to be updated even if component updates are turned off.
bool LacrosInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool LacrosInstallerPolicy::AllowUpdates() const {
  bool allow_updates = kDefaultLacrosAllowUpdates;

  ash::CrosSettings* settings = ash::CrosSettings::Get();
  if (!settings) {
    return allow_updates;
  }

  const base::Value* os_updates_disabled =
      settings->GetPref(ash::kUpdateDisabled);
  if (os_updates_disabled == nullptr || !os_updates_disabled->is_bool()) {
    return allow_updates;
  }

  // We disable Lacros updates when ChromeOS system updates are disabled.
  allow_updates = !os_updates_disabled->GetBool();

  return allow_updates;
}

// static
void LacrosInstallerPolicy::SetAshVersionForTest(const char* version) {
  g_ash_version_for_test = version;
}

DemoAppInstallerPolicy::DemoAppInstallerPolicy(
    const ComponentConfig& config,
    CrOSComponentInstaller* cros_component_installer)
    : CrOSComponentInstallerPolicy(config, cros_component_installer) {}

DemoAppInstallerPolicy::~DemoAppInstallerPolicy() = default;

void DemoAppInstallerPolicy::ComponentReady(const base::Version& version,
                                            const base::FilePath& path,
                                            base::Value::Dict manifest) {
  cros_component_installer_->RegisterCompatiblePath(
      GetName(), CompatibleComponentInfo(path, version));
}

update_client::InstallerAttributes
DemoAppInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes demo_app_installer_attributes;
  demo_app_installer_attributes["retailer_id"] = ash::demo_mode::RetailerName();
  demo_app_installer_attributes["store_id"] = ash::demo_mode::StoreNumber();
  demo_app_installer_attributes["demo_country"] = ash::demo_mode::Country();
  demo_app_installer_attributes["is_cloud_gaming_device"] =
      ash::demo_mode::IsCloudGamingDevice() ? "true" : "false";
  demo_app_installer_attributes["is_feature_aware_device"] =
      ash::demo_mode::IsFeatureAwareDevice() ? "true" : "false";

  auto* const cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(switches::kDemoModeTestTag)) {
    demo_app_installer_attributes["tag"] =
        cmdline->GetSwitchValueASCII(switches::kDemoModeTestTag);
  }
  return demo_app_installer_attributes;
}

GrowthCampaignsInstallerPolicy::GrowthCampaignsInstallerPolicy(
    const ComponentConfig& config,
    CrOSComponentInstaller* cros_component_installer)
    : CrOSComponentInstallerPolicy(config, cros_component_installer) {}

GrowthCampaignsInstallerPolicy::~GrowthCampaignsInstallerPolicy() = default;

void GrowthCampaignsInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    base::Value::Dict manifest) {
  cros_component_installer_->RegisterCompatiblePath(
      GetName(), CompatibleComponentInfo(path, version));
}

update_client::InstallerAttributes
GrowthCampaignsInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;
  auto* const cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(switches::kCampaignsTestTag)) {
    attributes["tag"] =
        cmdline->GetSwitchValueASCII(switches::kCampaignsTestTag);
  }
  return attributes;
}

CrOSComponentInstaller::CrOSComponentInstaller(
    std::unique_ptr<MetadataTable> metadata_table,
    ComponentUpdateService* component_updater)
    : metadata_table_(std::move(metadata_table)),
      component_updater_(component_updater) {}

CrOSComponentInstaller::~CrOSComponentInstaller() = default;

void CrOSComponentInstaller::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void CrOSComponentInstaller::Load(const std::string& name,
                                  MountPolicy mount_policy,
                                  UpdatePolicy update_policy,
                                  LoadCallback load_callback) {
  if (!IsCompatible(name) || update_policy == UpdatePolicy::kForce) {
    // A compatible component is not installed, or forced update is requested.
    // Start registration and installation/update process.
    Install(name, update_policy, mount_policy, std::move(load_callback));
  } else if (mount_policy == MountPolicy::kMount) {
    // A compatible component is installed, load it.
    LoadInternal(name, std::move(load_callback));
  } else {
    // A compatible component is installed, do not load it.
    constexpr Error error = Error::NONE;
    ReportError(error);
    std::move(load_callback).Run(error, base::FilePath());
  }
}

bool CrOSComponentInstaller::Unload(const std::string& name) {
  DispatchFailedLoads(std::move(load_cache_[name].callbacks));
  load_cache_.erase(name);

  const ComponentConfig* config = FindConfig(name);
  if (!config) {
    // Component |name| does not exist.
    return false;
  }
  const std::string id = GenerateId(config->sha2hash);
  metadata_table_->DeleteComponentForCurrentUser(name);
  return metadata_table_->HasComponentForAnyUser(name) ||
         component_updater_->UnregisterComponent(id);
}

void CrOSComponentInstaller::GetVersion(
    const std::string& name,
    base::OnceCallback<void(const base::Version&)> version_callback) const {
  if (!IsCompatible(name)) {
    // `name` does not match to any component.
    std::move(version_callback).Run(base::Version());
    return;
  }

  auto component_iter = compatible_components_.find(name);

  // Path compatible to `name` must exist.
  CHECK(component_iter != compatible_components_.end() &&
        !(component_iter->second.path.empty()));
  if (component_iter->second.version.has_value()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(version_callback),
                                  component_iter->second.version.value()));
  } else {
    ash::ImageLoaderClient::Get()->RequestComponentVersion(
        name, base::BindOnce(&CrOSComponentInstaller::FinishGetVersion,
                             weak_factory_.GetWeakPtr(),
                             std::move(version_callback)));
  }
}

void CrOSComponentInstaller::RegisterInstalled() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(GetInstalled),
      base::BindOnce(&CrOSComponentInstaller::RegisterN,
                     weak_factory_.GetWeakPtr()));
}

void CrOSComponentInstaller::RegisterCompatiblePath(
    const std::string& name,
    CompatibleComponentInfo info) {
  compatible_components_[name] = std::move(info);
}

void CrOSComponentInstaller::UnregisterCompatiblePath(const std::string& name) {
  DispatchFailedLoads(std::move(load_cache_[name].callbacks));
  load_cache_.erase(name);
  compatible_components_.erase(name);
}

base::FilePath CrOSComponentInstaller::GetCompatiblePath(
    const std::string& name) const {
  const auto it = compatible_components_.find(name);
  return it == compatible_components_.end() ? base::FilePath()
                                            : it->second.path;
}

void CrOSComponentInstaller::EmitInstalledSignal(const std::string& component) {
  if (delegate_) {
    delegate_->EmitInstalledSignal(component);
  }
}

CrOSComponentInstaller::LoadInfo::LoadInfo() = default;
CrOSComponentInstaller::LoadInfo::~LoadInfo() = default;
std::map<std::string, CrOSComponentInstaller::LoadInfo>&
CrOSComponentInstaller::GetLoadCacheForTesting() {
  return load_cache_;
}

void CrOSComponentInstaller::RemoveLoadCacheEntry(
    const std::string& component_name) {
  load_cache_.erase(component_name);
}

bool CrOSComponentInstaller::IsRegisteredMayBlock(const std::string& name) {
  base::FilePath root;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &root)) {
    return false;
  }

  return base::PathExists(root.Append(kComponentsRootPath).Append(name));
}

void CrOSComponentInstaller::Register(const ComponentConfig& config,
                                      base::OnceClosure register_callback) {
  std::unique_ptr<CrOSComponentInstallerPolicy> policy;
  switch (config.policy_type) {
    case ComponentConfig::PolicyType::kEnvVersion:
      policy = std::make_unique<EnvVersionInstallerPolicy>(config, this);
      break;
    case ComponentConfig::PolicyType::kLacros:
      policy = std::make_unique<LacrosInstallerPolicy>(config, this);
      break;
    case ComponentConfig::PolicyType::kDemoApp:
      policy = std::make_unique<DemoAppInstallerPolicy>(config, this);
      break;
    case ComponentConfig::PolicyType::kGrowthCampaigns:
      policy = std::make_unique<GrowthCampaignsInstallerPolicy>(config, this);
      break;
  }
  auto installer = base::MakeRefCounted<ComponentInstaller>(std::move(policy));
  installer->Register(component_updater_, std::move(register_callback));
}

void CrOSComponentInstaller::Install(const std::string& name,
                                     UpdatePolicy update_policy,
                                     MountPolicy mount_policy,
                                     LoadCallback load_callback) {
  const ComponentConfig* config = FindConfig(name);
  if (!config) {
    constexpr Error error = Error::UNKNOWN_COMPONENT;
    ReportError(error);
    std::move(load_callback).Run(error, base::FilePath());
    return;
  }

  Register(
      *config,
      base::BindOnce(
          &CrOSComponentInstaller::StartInstall, weak_factory_.GetWeakPtr(),
          name, GenerateId(config->sha2hash), update_policy,
          base::BindOnce(&CrOSComponentInstaller::FinishInstall,
                         weak_factory_.GetWeakPtr(), name, mount_policy,
                         update_policy, std::move(load_callback))));
}

void CrOSComponentInstaller::StartInstall(
    const std::string& name,
    const std::string& id,
    UpdatePolicy update_policy,
    update_client::Callback install_callback) {
  // Check whether an installed component was found during registration, and
  // determine whether OnDemandUpdater should be started accordingly.
  const bool is_compatible = IsCompatible(name);
  if (update_policy == UpdatePolicy::kSkip ||
      (is_compatible && update_policy != UpdatePolicy::kForce)) {
    std::move(install_callback).Run(update_client::Error::NONE);
    return;
  }

  const component_updater::OnDemandUpdater::Priority priority =
      is_compatible ? component_updater::OnDemandUpdater::Priority::BACKGROUND
                    : component_updater::OnDemandUpdater::Priority::FOREGROUND;
  component_updater_->GetOnDemandUpdater().OnDemandUpdate(
      id, priority, std::move(install_callback));
}

void CrOSComponentInstaller::FinishInstall(const std::string& name,
                                           MountPolicy mount_policy,
                                           UpdatePolicy update_policy,
                                           LoadCallback load_callback,
                                           update_client::Error error) {
  if (error != update_client::Error::NONE) {
    Error err = Error::INSTALL_FAILURE;
    if (error == update_client::Error::UPDATE_IN_PROGRESS) {
      err = Error::UPDATE_IN_PROGRESS;
    }
    ReportError(err);
    std::move(load_callback).Run(err, base::FilePath());
  } else if (!IsCompatible(name)) {
    const Error err = update_policy == UpdatePolicy::kSkip
                          ? Error::NOT_FOUND
                          : Error::COMPATIBILITY_CHECK_FAILED;
    ReportError(err);
    std::move(load_callback).Run(err, base::FilePath());
  } else if (mount_policy == MountPolicy::kMount) {
    LoadInternal(name, std::move(load_callback));
  } else {
    constexpr Error err = Error::NONE;
    ReportError(err);
    std::move(load_callback).Run(err, base::FilePath());
  }
}

void CrOSComponentInstaller::LoadInternal(const std::string& name,
                                          LoadCallback load_callback) {
  // Use the cached value if it exists.
  auto it = load_cache_.find(name);
  if (it != load_cache_.end()) {
    // If the request is ongoing, queue up a callback.
    if (!it->second.success.has_value()) {
      it->second.callbacks.push_back(std::move(load_callback));
      return;
    }
    // Otherwise immediately dispatch.
    DispatchLoadCallback(std::move(load_callback), it->second.path,
                         it->second.success.value());
    return;
  }

  // Update the cache to indicate the request is being queued.
  load_cache_[name].success = std::nullopt;

  const base::FilePath path = GetCompatiblePath(name);
  DCHECK(!path.empty());
  ash::ImageLoaderClient::Get()->LoadComponentAtPath(
      name, path,
      base::BindOnce(&CrOSComponentInstaller::FinishLoad,
                     weak_factory_.GetWeakPtr(), std::move(load_callback),
                     name));
}

void CrOSComponentInstaller::FinishLoad(LoadCallback load_callback,
                                        const std::string& name,
                                        std::optional<base::FilePath> result) {
  // ImageLoader returns an empty path if mount failed.
  bool success = result.has_value() && !result.value().empty();
  base::FilePath path;
  if (success) {
    path = result.value();
  }

  DispatchLoadCallback(std::move(load_callback), path, success);

  // Update the cache.
  auto it = load_cache_.find(name);
  if (it != load_cache_.end()) {
    it->second.success = success;
    it->second.path = path;

    // Dispatch queued up callbacks.
    for (LoadCallback& queued_callback : it->second.callbacks) {
      DispatchLoadCallback(std::move(queued_callback), path, success);
    }
    it->second.callbacks.clear();
  }
}

void CrOSComponentInstaller::FinishGetVersion(
    base::OnceCallback<void(const base::Version&)> version_callback,
    std::optional<std::string> result) const {
  std::move(version_callback).Run(base::Version(result.value_or("")));
}

void CrOSComponentInstaller::RegisterN(
    const std::vector<ComponentConfig>& configs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& config : configs) {
    Register(config, base::OnceClosure());
  }
}

bool CrOSComponentInstaller::IsCompatible(const std::string& name) const {
  return compatible_components_.count(name) > 0;
}

void CrOSComponentInstaller::DispatchLoadCallback(LoadCallback callback,
                                                  base::FilePath path,
                                                  bool success) {
  Error error = success ? Error::NONE : Error::MOUNT_FAILURE;
  ReportError(error);
  std::move(callback).Run(error, std::move(path));
}

void CrOSComponentInstaller::DispatchFailedLoads(
    std::vector<LoadCallback> callbacks) {
  for (LoadCallback& callback : callbacks) {
    DispatchLoadCallback(std::move(callback), base::FilePath(),
                         /*success=*/false);
  }
}

}  // namespace component_updater
