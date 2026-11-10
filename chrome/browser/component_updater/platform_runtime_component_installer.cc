// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/platform_runtime_component_installer.h"

#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/native_library.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/common/request_header_integrity/buildflags.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "crypto/hash.h"

#if BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/request_header_integrity/platform_runtime_host.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#endif

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: jidecimafobogahglicpmeajcaaaibib
constexpr uint8_t kPlatformRuntimePublicKeySHA256[32] = {
    0x98, 0x34, 0x28, 0xc0, 0x5e, 0x1e, 0x60, 0x76, 0xb8, 0x2f, 0xc4,
    0x09, 0x20, 0x00, 0x81, 0x81, 0x76, 0x2f, 0x59, 0xa6, 0x57, 0x67,
    0x42, 0xd1, 0xfe, 0xdf, 0xd0, 0x28, 0x86, 0x10, 0xef, 0xf0};

static_assert(std::size(kPlatformRuntimePublicKeySHA256) ==
              crypto::hash::kSha256Size);
constexpr char kPlatformRuntimeManifestName[] = "Platform Runtime";
constexpr base::TimeDelta kPlatformRuntimeStalenessThreshold = base::Days(7);

base::FilePath GetBinaryPath(const base::FilePath& install_dir) {
  return install_dir.AppendASCII(
      base::GetNativeLibraryName("chrome_platform_runtime"));
}

// Extracts the release time from the version.
// Assumes the version is in the format <YEAR>.<MONTH>.<DAY>.<BUILD>.
base::Time GetReleaseTimeFromVersion(const base::Version& version) {
  if (version.IsValid() && version.components().size() >= 3) {
    base::Time::Exploded exploded = {
        .year = static_cast<int>(version.components()[0]),
        .month = static_cast<int>(version.components()[1]),
        .day_of_month = static_cast<int>(version.components()[2]),
    };

    base::Time release_time;
    if (exploded.HasValidValues() &&
        base::Time::FromUTCExploded(exploded, &release_time)) {
      return release_time;
    }
  }
  return base::Time();
}

}  // namespace

namespace component_updater {

PlatformRuntimeComponentInstallerPolicy::
    PlatformRuntimeComponentInstallerPolicy() = default;

#if BUILDFLAG(IS_WIN)
PlatformRuntimeComponentInstallerPolicy::
    PlatformRuntimeComponentInstallerPolicy(
        std::unique_ptr<PlatformRuntimeInstallerDelegate> delegate)
    : installer_delegate_(std::move(delegate)) {}
#endif

PlatformRuntimeComponentInstallerPolicy::
    ~PlatformRuntimeComponentInstallerPolicy() = default;

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
#if BUILDFLAG(IS_WIN)
  const base::FilePath inner_crx =
      install_dir.Append(FILE_PATH_LITERAL("chrome_platform_runtime.crx3"));
  // Names the component in the ComponentUpdater.ProductComponent.* histograms.
  static constexpr char kPlatformRuntimeHistogramTag[] = "PlatformRuntime";
  const ProductComponentInstallOutcome outcome =
      installer_delegate_
          ? InstallProductComponentForTesting(kPlatformRuntimeHistogramTag,
                                              inner_crx, /*user_data_dir=*/{},
                                              installer_delegate_.get())
          : InstallProductComponent(kPlatformRuntimeHistogramTag, inner_crx);

  // Clean up the staged inner CRX from the user dir. The installer (setup.exe
  // or AppCommand) has already copied it to a staging location. Even if
  // installation failed, clean it up so it does not leak disk space, since
  // any subsequent install attempt will download a fresh CRX anyway.
  base::DeleteFile(inner_crx);

  if (!outcome.Succeeded()) {
    // The step at which installation ended and the code that gives its details
    // (e.g., setup.exe's exit code or an HRESULT) are sent to the update server
    // in the component's event ping.
    return update_client::CrxInstaller::Result(update_client::CategorizedError{
        .category = update_client::ErrorCategory::kInstaller,
        .code = static_cast<int>(outcome.step),
        .extra = outcome.extra_code,
    });
  }

  return update_client::CrxInstaller::Result(0);
#else
  return update_client::CrxInstaller::Result(0);
#endif
}

void PlatformRuntimeComponentInstallerPolicy::OnCustomUninstall() {}

bool PlatformRuntimeComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
#if BUILDFLAG(IS_WIN)
  base::FilePath app_dir =
      installer::GetInstalledDirectory(install_static::IsSystemInstall());
  if (app_dir.empty()) {
    return false;
  }
  base::Version disk_version;
  base::FilePath latest_dir = InstallUtil::GetLatestInstalledComponentDir(
      app_dir,
      crx_file::id_util::GenerateIdFromHash(kPlatformRuntimePublicKeySHA256),
      &disk_version);
  if (latest_dir.empty() || !disk_version.IsValid()) {
    return false;
  }

  return base::PathExists(GetBinaryPath(latest_dir)) &&
         base::PathExists(
             latest_dir.Append(FILE_PATH_LITERAL("manifest.json")));
#else
  return base::PathExists(GetBinaryPath(install_dir));
#endif
}

void PlatformRuntimeComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  PrefService* local_state = g_browser_process->local_state();
  CHECK(local_state);
  base::Version last_version(
      local_state->GetString(kPlatformRuntimeLastInstalledVersion));
  // Component update detected, or first install.
  if ((last_version.IsValid() && last_version != version) ||
      !last_version.IsValid()) {
    local_state->SetString(kPlatformRuntimeLastInstalledVersion,
                           version.GetString());
    // Persist the release date of the newly installed component version.
    // If the version does not encode a date, fall back to base::Time::Now().
    base::Time release_time = GetReleaseTimeFromVersion(version);
    local_state->SetTime(
        kPlatformRuntimeLastReleaseTime,
        release_time.is_null() ? base::Time::Now() : release_time);

    base::UmaHistogramEnumeration(
        "ComponentUpdater.PlatformRuntime.InstallTrigger", install_trigger_);
    // Reset trigger to background for any future updates.
    install_trigger_ = PlatformRuntimeInstallTrigger::kBackground;
  }

  // Offload blocking path resolution to a background thread.
  // Note: GetInstalledDirectory() and GetLatestInstalledComponentDir() perform
  // blocking I/O which is disallowed on the UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const base::Version& version,
             const base::FilePath& install_dir) -> base::FilePath {
            base::FilePath dll_path;
#if BUILDFLAG(IS_WIN)
            base::FilePath app_dir = installer::GetInstalledDirectory(
                install_static::IsSystemInstall());
            if (app_dir.empty()) {
              return base::FilePath();
            }
            base::Version disk_version;
            base::FilePath latest_dir =
                InstallUtil::GetLatestInstalledComponentDir(
                    app_dir,
                    crx_file::id_util::GenerateIdFromHash(
                        kPlatformRuntimePublicKeySHA256),
                    &disk_version);
            if (latest_dir.empty()) {
              return base::FilePath();
            }
            dll_path = GetBinaryPath(latest_dir);
            const base::Version& loaded_version =
                disk_version.IsValid() ? disk_version : version;
#else
            dll_path = GetBinaryPath(install_dir);
            const base::Version& loaded_version = version;
#endif
            if (!base::PathExists(dll_path)) {
              return base::FilePath();
            }
            VLOG(1) << "Platform Runtime component ready, version "
                    << loaded_version.GetString() << " in " << dll_path.value();
            return dll_path;
          },
          version, install_dir),
      base::BindOnce([](const base::FilePath& dll_path) {
        if (dll_path.empty()) {
          return;
        }
    // The library is executed only in a sandboxed utility process, so the
    // browser just records where it is.
#if BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY) && !BUILDFLAG(IS_ANDROID)
        if (g_browser_process && g_browser_process->GetFeatures()) {
          if (auto* host =
                  g_browser_process->GetFeatures()->platform_runtime_host()) {
            host->OnComponentReady(dll_path);
          }
        }
#endif
      }));
}

base::FilePath PlatformRuntimeComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("PlatformRuntime"));
}

void PlatformRuntimeComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign_range(kPlatformRuntimePublicKeySHA256);
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
  registry->RegisterTimePref(kPlatformRuntimeLastReleaseTime, base::Time());
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
              << std::to_underlying(error);
        }
      }));
}

bool PlatformRuntimeComponentInstallerPolicy::ShouldTriggerInstallOrUpdate(
    ComponentUpdateService* cus,
    PrefService* local_state,
    const std::string& crx_id) {
  CHECK(local_state);
  base::Version installed_version;
  base::Time last_release_time;

  update_client::CrxUpdateItem item;
  if (cus && cus->GetComponentDetails(crx_id, &item) &&
      item.component.has_value() && item.component->version.IsValid() &&
      item.component->version != base::Version(kNullVersion)) {
    installed_version = item.component->version;
    last_release_time = local_state->GetTime(kPlatformRuntimeLastReleaseTime);
  }

  if (!installed_version.IsValid()) {
    SetInstallTrigger(PlatformRuntimeInstallTrigger::kMissing);
    return true;
  }

  // If local state doesn't have a recorded release time (e.g. initial run or
  // cleared prefs), extract the release date directly from the installed
  // component version string.
  if (last_release_time.is_null()) {
    last_release_time = GetReleaseTimeFromVersion(installed_version);
  }

  // Trigger an on-demand update if the release date is unknown or older than
  // the staleness threshold.
  if (last_release_time.is_null() || (base::Time::Now() - last_release_time) >
                                         kPlatformRuntimeStalenessThreshold) {
    SetInstallTrigger(PlatformRuntimeInstallTrigger::kStale);
    return true;
  }

  return false;
}

void MaybeRegisterPlatformRuntimeComponent(ComponentUpdateService* cus) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!base::FeatureList::IsEnabled(kEnablePlatformRuntimeComponent)) {
    return;
  }
  // The browser connects to the machine-wide updater installation, which will
  // then run a command for the machine-wide install of Chrome that it manages.
  // We only install the component if this currently-running browser is the one
  // that is actually registered with the machine-wide updater.
  if (!installer::IsCurrentProcessInstalled()) {
    return;
  }

  std::unique_ptr<PlatformRuntimeComponentInstallerPolicy> policy =
      std::make_unique<PlatformRuntimeComponentInstallerPolicy>();
  PlatformRuntimeComponentInstallerPolicy* policy_ptr = policy.get();
  auto installer = base::MakeRefCounted<ComponentInstaller>(std::move(policy));

  // The lifecycle of `policy_ptr` is managed by `installer` which owns the
  // policy. Since the callback is executed during the registration of the
  // ref-counted `installer` (which is kept alive during registration and
  // retained by the ComponentUpdateService afterward), the policy is
  // guaranteed to outlive the callback.
  installer->Register(
      cus,
      base::BindOnce(
          [](PlatformRuntimeComponentInstallerPolicy* policy,
             const std::string& crx_id, ComponentUpdateService* cus) {
            if (policy->ShouldTriggerInstallOrUpdate(
                    cus, g_browser_process->local_state(), crx_id)) {
              VLOG(1) << "Platform Runtime component not installed or stale "
                         "locally. Triggering on-demand install.";
              PlatformRuntimeComponentInstallerPolicy::UpdateOnDemand(
                  cus, crx_id, OnDemandUpdater::Priority::FOREGROUND);
            }
          },
          base::Unretained(policy_ptr),
          crx_file::id_util::GenerateIdFromHash(
              kPlatformRuntimePublicKeySHA256),
          cus));
#endif
}

}  // namespace component_updater
