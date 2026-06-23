// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/platform_runtime_component_installer.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/platform_runtime/platform_runtime_impl.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "content/public/browser/network_service_instance.h"
#include "crypto/sha2.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: jidecimafobogahglicpmeajcaaaibib
constexpr uint8_t kPlatformRuntimePublicKeySHA256[32] = {
    0x98, 0x34, 0x28, 0xc0, 0x5e, 0x1e, 0x60, 0x76, 0xb8, 0x2f, 0xc4,
    0x09, 0x20, 0x00, 0x81, 0x81, 0x76, 0x2f, 0x59, 0xa6, 0x57, 0x67,
    0x42, 0xd1, 0xfe, 0xdf, 0xd0, 0x28, 0x86, 0x10, 0xef, 0xf0};

static_assert(std::size(kPlatformRuntimePublicKeySHA256) ==
              crypto::kSHA256Length);
constexpr char kPlatformRuntimeManifestName[] = "Platform Runtime";
constexpr base::TimeDelta kPlatformRuntimeStalenessThreshold = base::Days(7);

base::FilePath GetBinaryPath(const base::FilePath& install_dir) {
  return install_dir.AppendASCII(
      base::GetNativeLibraryName("chrome_platform_runtime"));
}

}  // namespace

namespace component_updater {

BASE_FEATURE(kEnablePlatformRuntimeComponent,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool PlatformRuntimeComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool PlatformRuntimeComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
PlatformRuntimeComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void PlatformRuntimeComponentInstallerPolicy::OnCustomUninstall() {}

bool PlatformRuntimeComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetBinaryPath(install_dir));
}

void PlatformRuntimeComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  VLOG(1) << "Platform Runtime component ready, version " << version.GetString()
          << " in " << install_dir.value();

  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    base::Version last_version(
        local_state->GetString(kPlatformRuntimeLastInstalledVersion));
    // Component update detected, or first install.
    if ((last_version.IsValid() && last_version != version) ||
        !last_version.IsValid()) {
      local_state->SetString(kPlatformRuntimeLastInstalledVersion,
                             version.GetString());
      local_state->SetTime(kPlatformRuntimeLastInstallTime, base::Time::Now());
    }
  }

  // Load the library in the Browser Process (where Component Updater runs).
  // Post it to a background thread because loading a DLL involves blocking I/O.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const base::FilePath& path) {
            platform_runtime::PlatformRuntimeImpl::GetInstance()
                ->UpdatePlatformRuntimeLibrary(path);
          },
          GetBinaryPath(install_dir)));
}

base::FilePath PlatformRuntimeComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("PlatformRuntime"));
}

void PlatformRuntimeComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPlatformRuntimePublicKeySHA256),
               std::end(kPlatformRuntimePublicKeySHA256));
}

std::string PlatformRuntimeComponentInstallerPolicy::GetName() const {
  return kPlatformRuntimeManifestName;
}

update_client::InstallerAttributes
PlatformRuntimeComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

// static
void PlatformRuntimeComponentInstallerPolicy::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kPlatformRuntimeLastInstallTime, base::Time());
  registry->RegisterStringPref(kPlatformRuntimeLastInstalledVersion,
                               std::string());
}

// static
void PlatformRuntimeComponentInstallerPolicy::UpdateOnDemand(
    ComponentUpdateService* cus,
    const std::string& id,
    OnDemandUpdater::Priority priority) {
  cus->GetOnDemandUpdater().OnDemandUpdate(
      id, priority, base::BindOnce([](update_client::Error error) {
        if (error != update_client::Error::NONE &&
            error != update_client::Error::UPDATE_IN_PROGRESS) {
          LOG(ERROR)
              << "Failed to update Platform Runtime component with error "
              << static_cast<int>(error);
        }
      }));
}

// static
bool PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
    ComponentUpdateService* cus,
    PrefService* local_state,
    const std::string& crx_id) {
  update_client::CrxUpdateItem item;
  bool success = cus->GetComponentDetails(crx_id, &item);
  bool is_installed = success && item.component.has_value() &&
                      item.component->version.IsValid() &&
                      item.component->version != base::Version(kNullVersion);

  if (!is_installed) {
    return true;
  }

  if (local_state) {
    base::Time last_on_demand_time =
        local_state->GetTime(kPlatformRuntimeLastInstallTime);
    if (last_on_demand_time.is_null() ||
        (base::Time::Now() - last_on_demand_time) >
            kPlatformRuntimeStalenessThreshold) {
      return true;
    }
  }

  return false;
}

void MaybeRegisterPlatformRuntimeComponent(ComponentUpdateService* cus) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!base::FeatureList::IsEnabled(kEnablePlatformRuntimeComponent)) {
    return;
  }

  std::unique_ptr<ComponentInstallerPolicy> policy =
      std::make_unique<PlatformRuntimeComponentInstallerPolicy>();
  std::vector<uint8_t> public_key_hash;
  policy->GetHash(&public_key_hash);
  const std::string crx_id =
      crx_file::id_util::GenerateIdFromHash(public_key_hash);
  auto installer = base::MakeRefCounted<ComponentInstaller>(std::move(policy));

  installer->Register(
      cus,
      base::BindOnce(
          [](const std::string& crx_id, ComponentUpdateService* cus) {
            PrefService* local_state = g_browser_process->local_state();
            if (PlatformRuntimeComponentInstallerPolicy::
                    ShouldTriggerInstallOrUpdate(cus, local_state, crx_id)) {
              VLOG(1) << "Platform Runtime component not installed or stale "
                         "locally. Triggering on-demand install.";
              PlatformRuntimeComponentInstallerPolicy::UpdateOnDemand(
                  cus, crx_id, OnDemandUpdater::Priority::FOREGROUND);
            }
          },
          crx_id, cus));
#endif
}

}  // namespace component_updater
