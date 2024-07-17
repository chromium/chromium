// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_urls.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_prefs.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {
constexpr int kHttpErrorCodeBadRequest = 400;
constexpr int kHttpErrorCodeForbidden = 403;
constexpr int kHttpErrorCodeNotFound = 404;
}  // namespace

ForceInstalledTracker::ForceInstalledTracker(ExtensionRegistry* registry,
                                             Profile* profile)
    : extension_management_(
          ExtensionManagementFactory::GetForBrowserContext(profile)),
      registry_(registry),
      profile_(profile),
      pref_service_(profile->GetPrefs()) {
  // Load immediately if PolicyService is ready, or wait for it to finish
  // initializing first.
  if (policy_service()->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME))
    OnPolicyServiceInitialized(policy::POLICY_DOMAIN_CHROME);
  else
    policy_service()->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
}

ForceInstalledTracker::~ForceInstalledTracker() {
  policy_service()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
}

void ForceInstalledTracker::UpdateCounters(ExtensionStatus status, int delta) {
  switch (status) {
    case ExtensionStatus::kPending:
      load_pending_count_ += delta;
      [[fallthrough]];
    case ExtensionStatus::kLoaded:
      ready_pending_count_ += delta;
      break;
    case ExtensionStatus::kReady:
    case ExtensionStatus::kFailed:
      break;
  }
}

void ForceInstalledTracker::AddExtensionInfo(const ExtensionId& extension_id,
                                             ExtensionStatus status,
                                             bool is_from_store) {
  auto result =
      extensions_.emplace(extension_id, ExtensionInfo{status, is_from_store});
  DCHECK(result.second);
  UpdateCounters(result.first->second.status, +1);
}

void ForceInstalledTracker::ChangeExtensionStatus(
    const ExtensionId& extension_id,
    ExtensionStatus status) {
  DCHECK_GE(status_, kWaitingForExtensionLoads);
  auto item = extensions_.find(extension_id);
  if (item == extensions_.end())
    return;
  UpdateCounters(item->second.status, -1);
  item->second.status = status;
  UpdateCounters(item->second.status, +1);
}

void ForceInstalledTracker::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                            const policy::PolicyMap& previous,
                                            const policy::PolicyMap& current) {}

void ForceInstalledTracker::OnPolicyServiceInitialized(
    policy::PolicyDomain domain) {
  DCHECK_EQ(domain, policy::POLICY_DOMAIN_CHROME);
  DCHECK_EQ(status_, kWaitingForPolicyService);

  policy_service()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);

  // Continue to listen to |kInstallForceList| pref changes if it is empty.
  if (!ProceedIfForcedExtensionsPrefReady()) {
    status_ = kWaitingForInstallForcelistPref;
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        pref_names::kInstallForceList,
        base::BindRepeating(&ForceInstalledTracker::OnInstallForcelistChanged,
                            base::Unretained(this)));
  }
}

void ForceInstalledTracker::OnInstallForcelistChanged() {
  DCHECK_EQ(status_, kWaitingForInstallForcelistPref);
  ProceedIfForcedExtensionsPrefReady();
}

bool ForceInstalledTracker::ProceedIfForcedExtensionsPrefReady() {
  DCHECK(
      policy_service()->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME));
  DCHECK(status_ == kWaitingForPolicyService ||
         status_ == kWaitingForInstallForcelistPref);

  const base::Value::Dict& value =
      pref_service_->GetDict(pref_names::kInstallForceList);
  if (!forced_extensions_pref_ready_ && !value.empty()) {
    forced_extensions_pref_ready_ = true;
    OnForcedExtensionsPrefReady();
    return true;
  }
  return false;
}

void ForceInstalledTracker::OnForcedExtensionsPrefReady() {
  DCHECK(forced_extensions_pref_ready_);
  DCHECK(
      policy_service()->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME));
  DCHECK(status_ == kWaitingForPolicyService ||
         status_ == kWaitingForInstallForcelistPref);

  pref_change_registrar_.RemoveAll();

  // Listen for extension loads and install failures.
  status_ = kWaitingForExtensionLoads;
  registry_observation_.Observe(registry_.get());
  collector_observation_.Observe(InstallStageTracker::Get(profile_));

  const base::Value::Dict& value =
      pref_service_->GetDict(pref_names::kInstallForceList);

  // Add each extension to |extensions_|.
  for (auto entry : value) {
    const ExtensionId& extension_id = entry.first;
    const std::string* update_url =
        entry.second.is_dict() ? entry.second.GetDict().FindString(
                                     ExternalProviderImpl::kExternalUpdateUrl)
                               : nullptr;
    bool is_from_store =
        update_url && *update_url == extension_urls::kChromeWebstoreUpdateURL;

    ExtensionStatus status = ExtensionStatus::kPending;
    if (registry_->enabled_extensions().Contains(extension_id)) {
      status = registry_->ready_extensions().Contains(extension_id)
                   ? ExtensionStatus::kReady
                   : ExtensionStatus::kLoaded;
    }
    AddExtensionInfo(extension_id, status, is_from_store);
  }

  // Run observers if there are no pending installs.
  MaybeNotifyObservers();
}

void ForceInstalledTracker::OnShutdown(ExtensionRegistry*) {
  registry_observation_.Reset();
}

void ForceInstalledTracker::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void ForceInstalledTracker::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void ForceInstalledTracker::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  ChangeExtensionStatus(extension->id(), ExtensionStatus::kLoaded);
  MaybeNotifyObservers();
}

void ForceInstalledTracker::OnExtensionReady(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  ChangeExtensionStatus(extension->id(), ExtensionStatus::kReady);
  MaybeNotifyObservers();
}

void ForceInstalledTracker::OnExtensionInstallationFailed(
    const ExtensionId& extension_id,
    InstallStageTracker::FailureReason reason) {
  auto item = extensions_.find(extension_id);
  // If the extension is loaded, ignore the failure.
  if (item == extensions_.end() ||
      item->second.status == ExtensionStatus::kLoaded ||
      item->second.status == ExtensionStatus::kReady)
    return;
  ChangeExtensionStatus(extension_id, ExtensionStatus::kFailed);
  bool is_from_store = item->second.is_from_store;
  for (auto& obs : observers_)
    obs.OnForceInstalledExtensionFailed(extension_id, reason, is_from_store);
  MaybeNotifyObservers();
}

bool ForceInstalledTracker::IsDoneLoading() const {
  return status_ == kWaitingForExtensionReady || status_ == kComplete;
}

void ForceInstalledTracker::OnExtensionDownloadCacheStatusRetrieved(
    const ExtensionId& id,
    ExtensionDownloaderDelegate::CacheStatus cache_status) {
  // Report cache status only for forced installed extension.
  if (extensions_.find(id) != extensions_.end()) {
    for (auto& obs : observers_)
      obs.OnExtensionDownloadCacheStatusRetrieved(id, cache_status);
  }
}

bool ForceInstalledTracker::IsReady() const {
  // `kWaitingForInstallForcelistPref` status means that there are no force
  // installed extensions present at the start up.
  return status_ == kComplete || status_ == kWaitingForInstallForcelistPref;
}

bool ForceInstalledTracker::IsComplete() const {
  return status_ == kComplete;
}

bool ForceInstalledTracker::IsMisconfiguration(
    const InstallStageTracker::InstallationData& installation_data,
    const ExtensionId& id) const {
  if (installation_data.install_error_detail) {
    CrxInstallErrorDetail detail =
        installation_data.install_error_detail.value();
    if (detail == CrxInstallErrorDetail::KIOSK_MODE_ONLY)
      return true;

    if (installation_data.extension_type &&
        detail == CrxInstallErrorDetail::DISALLOWED_BY_POLICY &&
        !extension_management_->IsAllowedManifestType(
            installation_data.extension_type.value(), id)) {
      return true;
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // REPLACED_BY_SYSTEM_APP is a misconfiguration because these apps are legacy
  // apps and are replaced by system apps.
  if (installation_data.failure_reason ==
      InstallStageTracker::FailureReason::REPLACED_BY_SYSTEM_APP) {
    return true;
  }

  // REPLACED_BY_ARC_APP error is a misconfiguration if ARC++ is enabled for
  // the device.
  if (profile_->GetPrefs()->IsManagedPreference(arc::prefs::kArcEnabled) &&
      profile_->GetPrefs()->GetBoolean(arc::prefs::kArcEnabled) &&
      installation_data.failure_reason ==
          InstallStageTracker::FailureReason::REPLACED_BY_ARC_APP) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (installation_data.failure_reason ==
      InstallStageTracker::FailureReason::NOT_PERFORMING_NEW_INSTALL) {
    return true;
  }
  if (installation_data.failure_reason ==
      InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY) {
    DCHECK(installation_data.no_updates_info);
    if (installation_data.no_updates_info.value() ==
        InstallStageTracker::NoUpdatesInfo::kEmpty) {
      return true;
    }
  }

  if (installation_data.manifest_invalid_error ==
          ManifestInvalidError::BAD_APP_STATUS &&
      installation_data.app_status_error ==
          InstallStageTracker::AppStatusError::kErrorUnknownApplication) {
    return true;
  }

  if (installation_data.unpacker_failure_reason ==
      SandboxedUnpackerFailureReason::CRX_HEADER_INVALID) {
    auto extension = extensions_.find(id);
    // Extension id may be missing from this list if there is a change in
    // ExtensionInstallForcelist policy after the user has logged in and
    // |IsMisconfiguration| method is called from
    // |ExtensionInstallEventLogCollector|.
    if (extension != extensions_.end() && !extension->second.is_from_store &&
        !IsExtensionFetchedFromCache(
            installation_data.downloading_cache_status)) {
      return true;
    }
  }

  // When we receive 403 during update manifest fetch, it means that either
  // update URL is wrong, or self-hosting server is misconfigured. Both cases
  // are misconfigurations from Chrome's point view.
  if (installation_data.failure_reason ==
      InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED) {
    auto extension = extensions_.find(id);
    if (extension != extensions_.end() && !extension->second.is_from_store) {
      if (installation_data.response_code == kHttpErrorCodeBadRequest ||
          installation_data.response_code == kHttpErrorCodeForbidden ||
          installation_data.response_code == kHttpErrorCodeNotFound) {
        return true;
      }
    }
  }

  if (installation_data.failure_reason ==
      InstallStageTracker::FailureReason::MANIFEST_INVALID) {
    auto extension = extensions_.find(id);
    if (extension != extensions_.end() && !extension->second.is_from_store) {
      return true;
    }
  }

  if (installation_data.failure_reason ==
      InstallStageTracker::FailureReason::OVERRIDDEN_BY_SETTINGS) {
    return true;
  }

  return false;
}

// static
bool ForceInstalledTracker::IsExtensionFetchedFromCache(
    const std::optional<ExtensionDownloaderDelegate::CacheStatus>& status) {
  if (!status)
    return false;
  return status.value() == ExtensionDownloaderDelegate::CacheStatus::
                               CACHE_HIT_ON_MANIFEST_FETCH_FAILURE ||
         status.value() == ExtensionDownloaderDelegate::CacheStatus::CACHE_HIT;
}

policy::PolicyService* ForceInstalledTracker::policy_service() {
  return profile_->GetProfilePolicyConnector()->policy_service();
}

void ForceInstalledTracker::MaybeNotifyObservers() {
  DCHECK_GE(status_, kWaitingForExtensionLoads);
  if (status_ == kWaitingForExtensionLoads && load_pending_count_ == 0) {
    for (auto& obs : observers_)
      obs.OnForceInstalledExtensionsLoaded();
    status_ = kWaitingForExtensionReady;
  }
  if (status_ == kWaitingForExtensionReady && ready_pending_count_ == 0) {
    for (auto& obs : observers_)
      obs.OnForceInstalledExtensionsReady();
    status_ = kComplete;
    registry_observation_.Reset();
    collector_observation_.Reset();
    InstallStageTracker::Get(profile_)->Clear();
  }
}

}  //  namespace extensions
