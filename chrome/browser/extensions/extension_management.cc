// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "chrome/browser/extensions/permissions_based_management_policy_provider.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#else
#include "components/enterprise/browser/reporting/common_pref_names.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#endif

namespace extensions {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Disables off-store force-installed extensions in low trust environments.
BASE_FEATURE(kDisableOffstoreForceInstalledExtensionsInLowTrustEnviroment,
             "DisableOffstoreForceInstalledExtensionsInLowTrustEnviroment",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

ExtensionManagement::ExtensionManagement(Profile* profile)
    : profile_(profile), pref_service_(profile_->GetPrefs()) {
  TRACE_EVENT0("browser,startup",
               "ExtensionManagement::ExtensionManagement::ctor");
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_signin_profile_ = ash::ProfileHelper::IsSigninProfile(profile);
#endif
  pref_change_registrar_.Init(pref_service_);
  base::RepeatingClosure pref_change_callback = base::BindRepeating(
      &ExtensionManagement::OnExtensionPrefChanged, base::Unretained(this));
  pref_change_registrar_.Add(pref_names::kInstallAllowList,
                             pref_change_callback);
  pref_change_registrar_.Add(pref_names::kInstallDenyList,
                             pref_change_callback);
  pref_change_registrar_.Add(pref_names::kInstallForceList,
                             pref_change_callback);
  pref_change_registrar_.Add(pref_names::kAllowedInstallSites,
                             pref_change_callback);
  pref_change_registrar_.Add(pref_names::kAllowedTypes, pref_change_callback);
  pref_change_registrar_.Add(pref_names::kExtensionManagement,
                             pref_change_callback);
  pref_change_registrar_.Add(prefs::kCloudExtensionRequestEnabled,
                             pref_change_callback);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  pref_change_registrar_.Add(enterprise_reporting::kCloudReportingEnabled,
                             pref_change_callback);
#endif
  pref_change_registrar_.Add(pref_names::kManifestV2Availability,
                             pref_change_callback);
  pref_change_registrar_.Add(pref_names::kExtensionUnpublishedAvailability,
                             pref_change_callback);
  // Note that both |global_settings_| and |default_settings_| will be null
  // before first call to Refresh(), so in order to resolve this, Refresh() must
  // be called in the initialization of ExtensionManagement.
  Refresh();
  ReportExtensionManagementInstallCreationStage(
      InstallStageTracker::InstallCreationStage::
          NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED,
      InstallStageTracker::InstallCreationStage::
          NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_NOT_FORCED);
  providers_.push_back(
      std::make_unique<StandardManagementPolicyProvider>(this, profile_.get()));
  providers_.push_back(
      std::make_unique<PermissionsBasedManagementPolicyProvider>(this));
}

ExtensionManagement::~ExtensionManagement() = default;

void ExtensionManagement::Shutdown() {
  pref_change_registrar_.RemoveAll();
  pref_service_ = nullptr;
}

void ExtensionManagement::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionManagement::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

const std::vector<std::unique_ptr<ManagementPolicy::Provider>>&
ExtensionManagement::GetProviders() const {
  return providers_;
}

bool ExtensionManagement::BlocklistedByDefault() const {
  return (default_settings_->installation_mode == INSTALLATION_BLOCKED ||
          default_settings_->installation_mode == INSTALLATION_REMOVED);
}

ExtensionManagement::InstallationMode ExtensionManagement::GetInstallationMode(
    const Extension* extension) {
  const std::string* update_url =
      extension->manifest()->FindStringPath(manifest_keys::kUpdateURL);
  return GetInstallationMode(extension->id(),
                             update_url ? *update_url : std::string());
}

ExtensionManagement::InstallationMode ExtensionManagement::GetInstallationMode(
    const ExtensionId& extension_id,
    const std::string& update_url) {
  // Check per-extension installation mode setting first.
  auto* setting = GetSettingsForId(extension_id);
  if (setting)
    return setting->installation_mode;
  // Check per-update-url installation mode setting.
  if (!update_url.empty()) {
    auto iter_update_url = settings_by_update_url_.find(update_url);
    if (iter_update_url != settings_by_update_url_.end())
      return iter_update_url->second->installation_mode;
  }
  // Fall back to default installation mode setting.
  return default_settings_->installation_mode;
}

base::Value::Dict ExtensionManagement::GetForceInstallList() const {
  return GetInstallListByMode(INSTALLATION_FORCED);
}

base::Value::Dict ExtensionManagement::GetRecommendedInstallList() const {
  return GetInstallListByMode(INSTALLATION_RECOMMENDED);
}

bool ExtensionManagement::HasAllowlistedExtension() {
  // TODO(rdevlin.cronin): investigate implementation correctness per
  // https://crbug.com/1258180.
  if (default_settings_->installation_mode != INSTALLATION_BLOCKED &&
      default_settings_->installation_mode != INSTALLATION_REMOVED) {
    return true;
  }

  for (const auto& it : settings_by_id_) {
    if (it.second->installation_mode == INSTALLATION_ALLOWED)
      return true;
  }

  // If there are deferred extensions try loading them.
  while (!deferred_ids_.empty()) {
    auto extension_id = *deferred_ids_.begin();
    // This will remove the entry from |deferred_ids_|.
    LoadDeferredExtensionSetting(extension_id);
    DCHECK(!base::Contains(deferred_ids_, extension_id));
    if (AccessById(extension_id)->installation_mode == INSTALLATION_ALLOWED)
      return true;
  }

  return false;
}

bool ExtensionManagement::IsUpdateUrlOverridden(const ExtensionId& id) {
  auto* setting = GetSettingsForId(id);
  // No settings explicitly specified for |id|.
  return setting && setting->override_update_url;
}

GURL ExtensionManagement::GetEffectiveUpdateURL(const Extension& extension) {
  if (IsUpdateUrlOverridden(extension.id())) {
    DCHECK(!extension.was_installed_by_default())
        << "Update URL should not be overridden for default-installed "
           "extensions!";
    auto* setting = GetSettingsForId(extension.id());
    DCHECK(setting);
    const GURL update_url(setting->update_url);
    // It's important that we never override a non-webstore update URL to be
    // the webstore URL. Otherwise, a policy may inadvertently cause
    // non-webstore extensions to be treated as from-webstore (including content
    // verification, report abuse options, etc).
    DCHECK(!extension_urls::IsWebstoreUpdateUrl(update_url))
        << "Update URL cannot be overridden to be the webstore URL!";
    return update_url;
  }
  return ManifestURL::GetUpdateURL(&extension);
}

bool ExtensionManagement::UpdatesFromWebstore(const Extension& extension) {
  const bool is_webstore_url = extension_urls::IsWebstoreUpdateUrl(
      GURL(GetEffectiveUpdateURL(extension)));
  if (is_webstore_url) {
    DCHECK(!IsUpdateUrlOverridden(extension.id()))
        << "An extension's update URL cannot be overridden to the webstore.";
  }
  return is_webstore_url;
}

bool ExtensionManagement::IsInstallationExplicitlyAllowed(
    const ExtensionId& id) {
  auto* setting = GetSettingsForId(id);
  // No settings explicitly specified for |id|.
  if (setting == nullptr)
    return false;
  // Checks if the extension is on the automatically installed list or
  // install allow-list.
  InstallationMode mode = setting->installation_mode;
  return mode == INSTALLATION_FORCED || mode == INSTALLATION_RECOMMENDED ||
         mode == INSTALLATION_ALLOWED;
}

bool ExtensionManagement::IsInstallationExplicitlyBlocked(
    const ExtensionId& id) {
  auto* setting = GetSettingsForId(id);
  // No settings explicitly specified for |id|.
  if (setting == nullptr)
    return false;
  // Checks if the extension is listed as blocked or removed.
  InstallationMode mode = setting->installation_mode;
  return mode == INSTALLATION_BLOCKED || mode == INSTALLATION_REMOVED;
}

bool ExtensionManagement::IsOffstoreInstallAllowed(
    const GURL& url,
    const GURL& referrer_url) const {
  // No allowed install sites specified, disallow by default.
  if (!global_settings_->install_sources.has_value())
    return false;

  const URLPatternSet& url_patterns = *global_settings_->install_sources;

  if (!url_patterns.MatchesURL(url))
    return false;

  // The referrer URL must also be allowlisted, unless the URL has the file
  // scheme (there's no referrer for those URLs).
  return url.SchemeIsFile() || url_patterns.MatchesURL(referrer_url);
}

bool ExtensionManagement::IsAllowedManifestType(
    Manifest::Type manifest_type,
    const std::string& extension_id) const {
  // If a managed theme has been set for the current profile, theme extension
  // installations are not allowed.
  if (manifest_type == Manifest::Type::TYPE_THEME &&
      ThemeServiceFactory::GetForProfile(profile_)->UsingPolicyTheme())
    return false;

  if (!global_settings_->allowed_types.has_value())
    return true;
  const std::vector<Manifest::Type>& allowed_types =
      *global_settings_->allowed_types;
  return base::Contains(allowed_types, manifest_type);
}

bool ExtensionManagement::IsAllowedManifestVersion(
    int manifest_version,
    const std::string& extension_id,
    Manifest::Type manifest_type) {
  bool enabled_by_default =
      !base::FeatureList::IsEnabled(
          extensions_features::kExtensionsManifestV3Only) ||
      manifest_version >= 3;

  // Manifest version policy only supports normal extensions and Chrome OS login
  // screen extension.
  if (manifest_type != Manifest::Type::TYPE_EXTENSION &&
      manifest_type != Manifest::Type::TYPE_LOGIN_SCREEN_EXTENSION) {
    return enabled_by_default;
  }
  switch (global_settings_->manifest_v2_setting) {
    case internal::GlobalSettings::ManifestV2Setting::kDefault:
      return enabled_by_default;
    case internal::GlobalSettings::ManifestV2Setting::kDisabled:
      return manifest_version >= 3;
    case internal::GlobalSettings::ManifestV2Setting::kEnabled:
      return true;
    case internal::GlobalSettings::ManifestV2Setting::kEnabledForForceInstalled:
      auto installation_mode =
          GetInstallationMode(extension_id, /*update_url=*/std::string());
      return manifest_version >= 3 ||
             installation_mode == INSTALLATION_FORCED ||
             installation_mode == INSTALLATION_RECOMMENDED;
  }
}

bool ExtensionManagement::IsAllowedManifestVersion(const Extension* extension) {
  return IsAllowedManifestVersion(extension->manifest_version(),
                                  extension->id(), extension->GetType());
}

bool ExtensionManagement::IsExemptFromMV2DeprecationByPolicy(
    int manifest_version,
    const std::string& extension_id,
    Manifest::Type manifest_type) {
  // This policy only affects MV2 extensions.
  if (manifest_version != 2) {
    return false;
  }
  if (manifest_type != Manifest::Type::TYPE_EXTENSION &&
      manifest_type != Manifest::Type::TYPE_LOGIN_SCREEN_EXTENSION) {
    return false;
  }

  switch (global_settings_->manifest_v2_setting) {
    case internal::GlobalSettings::ManifestV2Setting::kDefault:
      // Default browser behavior. Not exempt.
      return false;
    case internal::GlobalSettings::ManifestV2Setting::kDisabled:
      // All MV2 extensions are disallowed. Not exempt.
      return false;
    case internal::GlobalSettings::ManifestV2Setting::kEnabled:
      // All MV2 extensions are allowed. Exempt.
      return true;
    case internal::GlobalSettings::ManifestV2Setting::kEnabledForForceInstalled:
      // Force-installed MV2 extensions are allowed. Exempt if it's a force-
      // installed extension only.
      auto installation_mode =
          GetInstallationMode(extension_id, /*update_url=*/std::string());
      return installation_mode == INSTALLATION_FORCED ||
             installation_mode == INSTALLATION_RECOMMENDED;
  }

  return false;
}

bool ExtensionManagement::IsAllowedByUnpublishedAvailabilityPolicy(
    const Extension* extension) {
  // Check the kill switch before applying policy check.
  if (!base::FeatureList::IsEnabled(kCWSInfoService)) {
    return true;
  }
  // This policy only applies to extensions that update from CWS.
  if (!UpdatesFromWebstore(*extension)) {
    return true;
  }
  if (global_settings_->unpublished_availability_setting ==
      internal::GlobalSettings::UnpublishedAvailability::kAllowUnpublished) {
    return true;
  }
  if (!cws_info_service_) {
    cws_info_service_ = CWSInfoService::Get(profile_);
  }
  // Return the current published status of the extension in CWS if available.
  // Otherwise assume the extension is currently published and return true.
  // Ignore extensions taken down for malware as they are blocklisted and
  // unloaded independently of policy.
  // Current publish status may not available if the policy setting just changed
  // to |kDisableUnpublished|. The actual publish status will be retrieved
  // by CWSInfoService separately and will trigger this same policy check.
  std::optional<CWSInfoServiceInterface::CWSInfo> cws_info =
      cws_info_service_->GetCWSInfo(*extension);
  if (cws_info.has_value() && cws_info->is_present &&
      cws_info->violation_type !=
          CWSInfoServiceInterface::CWSViolationType::kMalware) {
    return cws_info->is_live;
  }
  return true;
}

bool ExtensionManagement::IsAllowedByUnpackedDeveloperModePolicy(
    const Extension& extension) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionDisableUnsupportedDeveloper)) {
    return true;
  }
  if (!extension.is_extension()) {
    return true;
  }
  if (extension.location() != mojom::ManifestLocation::kUnpacked) {
    return true;
  }

  bool in_developer_mode =
      profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  return in_developer_mode;
}

bool ExtensionManagement::ShouldBlockForceInstalledOffstoreExtension(
    const Extension& extension) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (!base::FeatureList::IsEnabled(
          kDisableOffstoreForceInstalledExtensionsInLowTrustEnviroment)) {
    return false;
  }
  if (extension.from_webstore() || UpdatesFromWebstore(extension)) {
    return false;
  }
  if (GetInstallationMode(&extension) !=
      ExtensionManagement::INSTALLATION_FORCED) {
    return false;
  }
  if (!Manifest::IsPolicyLocation(extension.location())) {
    return false;
  }

  // The highest level of ManagementAuthorityTrustworthiness of either
  // platform or browser are taken into account.
  policy::ManagementAuthorityTrustworthiness platform_trustworthiness =
      policy::ManagementServiceFactory::GetForPlatform()
          ->GetManagementAuthorityTrustworthiness();
  policy::ManagementAuthorityTrustworthiness browser_trustworthiness =
      policy::ManagementServiceFactory::GetForProfile(profile_)
          ->GetManagementAuthorityTrustworthiness();
  policy::ManagementAuthorityTrustworthiness highest_trustworthiness =
      std::max(platform_trustworthiness, browser_trustworthiness);

  return highest_trustworthiness <
         policy::ManagementAuthorityTrustworthiness::TRUSTED;
#else
  return false;
#endif
}

APIPermissionSet ExtensionManagement::GetBlockedAPIPermissions(
    const Extension* extension) {
  const std::string* update_url =
      extension->manifest()->FindStringPath(manifest_keys::kUpdateURL);
  return GetBlockedAPIPermissions(extension->id(),
                                  update_url ? *update_url : std::string());
}

APIPermissionSet ExtensionManagement::GetBlockedAPIPermissions(
    const ExtensionId& extension_id,
    const std::string& update_url) {
  // Fetch per-extension blocked permissions setting.
  auto* setting = GetSettingsForId(extension_id);

  // Fetch per-update-url blocked permissions setting.
  auto iter_update_url = settings_by_update_url_.end();
  if (!update_url.empty())
    iter_update_url = settings_by_update_url_.find(update_url);

  if (setting && iter_update_url != settings_by_update_url_.end()) {
    // Blocked permissions setting are specified in both per-extension and
    // per-update-url settings, try to merge them.
    APIPermissionSet merged;
    APIPermissionSet::Union(setting->blocked_permissions,
                            iter_update_url->second->blocked_permissions,
                            &merged);
    return merged;
  }
  // Check whether if in one of them, setting is specified.
  if (setting)
    return setting->blocked_permissions.Clone();
  if (iter_update_url != settings_by_update_url_.end())
    return iter_update_url->second->blocked_permissions.Clone();
  // Fall back to the default blocked permissions setting.
  return default_settings_->blocked_permissions.Clone();
}

const URLPatternSet& ExtensionManagement::GetDefaultPolicyBlockedHosts() const {
  return default_settings_->policy_blocked_hosts;
}

const URLPatternSet& ExtensionManagement::GetDefaultPolicyAllowedHosts() const {
  return default_settings_->policy_allowed_hosts;
}

const URLPatternSet& ExtensionManagement::GetPolicyBlockedHosts(
    const Extension* extension) {
  auto* setting = GetSettingsForId(extension->id());
  if (setting)
    return setting->policy_blocked_hosts;
  return default_settings_->policy_blocked_hosts;
}

const URLPatternSet& ExtensionManagement::GetPolicyAllowedHosts(
    const Extension* extension) {
  auto* setting = GetSettingsForId(extension->id());
  if (setting)
    return setting->policy_allowed_hosts;
  return default_settings_->policy_allowed_hosts;
}

bool ExtensionManagement::UsesDefaultPolicyHostRestrictions(
    const Extension* extension) {
  return GetSettingsForId(extension->id()) == nullptr;
}

std::unique_ptr<const PermissionSet> ExtensionManagement::GetBlockedPermissions(
    const Extension* extension) {
  // Only api permissions are supported currently.
  return std::unique_ptr<const PermissionSet>(new PermissionSet(
      GetBlockedAPIPermissions(extension), ManifestPermissionSet(),
      URLPatternSet(), URLPatternSet()));
}

bool ExtensionManagement::IsPermissionSetAllowed(const Extension* extension,
                                                 const PermissionSet& perms) {
  const std::string* update_url =
      extension->manifest()->FindStringPath(manifest_keys::kUpdateURL);
  return IsPermissionSetAllowed(
      extension->id(), update_url ? *update_url : std::string(), perms);
}

bool ExtensionManagement::IsPermissionSetAllowed(
    const ExtensionId& extension_id,
    const std::string& update_url,
    const PermissionSet& perms) {
  for (const APIPermission* blocked_api :
       GetBlockedAPIPermissions(extension_id, update_url)) {
    if (perms.HasAPIPermission(blocked_api->id()))
      return false;
  }
  return true;
}

const std::string ExtensionManagement::BlockedInstallMessage(
    const ExtensionId& id) {
  auto* setting = GetSettingsForId(id);
  if (setting)
    return setting->blocked_install_message;
  return default_settings_->blocked_install_message;
}

ExtensionIdSet ExtensionManagement::GetForcePinnedList() const {
  ExtensionIdSet force_pinned_list;
  for (const auto& entry : settings_by_id_) {
    if (entry.second->toolbar_pin == ToolbarPinMode::kForcePinned)
      force_pinned_list.insert(entry.first);
  }
  return force_pinned_list;
}

bool ExtensionManagement::IsFileUrlNavigationAllowed(const ExtensionId& id) {
  auto* setting = GetSettingsForId(id);
  return setting && setting->file_url_navigation_allowed;
}

bool ExtensionManagement::CheckMinimumVersion(const Extension* extension,
                                              std::string* required_version) {
  auto* setting = GetSettingsForId(extension->id());
  // If there are no minimum version required for |extension|, return true.
  if (!setting || !setting->minimum_version_required)
    return true;
  bool meets_requirement =
      extension->version().CompareTo(*setting->minimum_version_required) >= 0;
  // Output a human readable version string for prompting if necessary.
  if (!meets_requirement && required_version)
    *required_version = setting->minimum_version_required->GetString();
  return meets_requirement;
}

void ExtensionManagement::Refresh() {
  TRACE_EVENT0("browser,startup", "ExtensionManagement::Refresh");
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.Management_Refresh");
  // Load all extension management settings preferences.
  const base::Value::List* allowed_list_pref =
      LoadListPreference(pref_names::kInstallAllowList, true);
  // Allow user to use preference to block certain extensions. Note that policy
  // managed forcelist or allowlist will always override this.
  const base::Value::List* denied_list_pref =
      LoadListPreference(pref_names::kInstallDenyList, false);
  const base::Value::Dict* forced_list_pref =
      LoadDictPreference(pref_names::kInstallForceList, true);
  const base::Value::List* install_sources_pref =
      LoadListPreference(pref_names::kAllowedInstallSites, true);
  const base::Value::List* allowed_types_pref =
      LoadListPreference(pref_names::kAllowedTypes, true);
  const base::Value::Dict* dict_pref =
      LoadDictPreference(pref_names::kExtensionManagement, true);
  const base::Value* extension_request_pref = LoadPreference(
      prefs::kCloudExtensionRequestEnabled, false, base::Value::Type::BOOLEAN);

  const base::Value* manifest_v2_pref =
      LoadPreference(pref_names::kManifestV2Availability,
                     /*force_managed=*/true, base::Value::Type::INTEGER);
  const base::Value* unpublished_availability_pref =
      LoadPreference(pref_names::kExtensionUnpublishedAvailability,
                     /*force_managed=*/true, base::Value::Type::INTEGER);

  // Reset all settings.
  global_settings_ = std::make_unique<internal::GlobalSettings>();
  settings_by_id_.clear();
  deferred_ids_.clear();
  default_settings_ = std::make_unique<internal::IndividualSettings>();

  // Parse default settings.
  const base::Value wildcard("*");
  if ((denied_list_pref && base::Contains(*denied_list_pref, wildcard)) ||
      (extension_request_pref && extension_request_pref->GetBool())) {
    default_settings_->installation_mode = INSTALLATION_BLOCKED;
  }

  if (const base::Value::Dict* subdict =
          dict_pref ? dict_pref->FindDict(schema_constants::kWildcard)
                    : nullptr) {
    if (!default_settings_->Parse(
            *subdict, internal::IndividualSettings::SCOPE_DEFAULT)) {
      LOG(WARNING) << "Default extension management settings parsing error.";
      default_settings_->Reset();
    }

    // Settings from new preference have higher priority over legacy ones.
    const base::Value::List* list_value =
        subdict->FindList(schema_constants::kInstallSources);
    if (list_value)
      install_sources_pref = list_value;

    list_value = subdict->FindList(schema_constants::kAllowedTypes);
    if (list_value)
      allowed_types_pref = list_value;
  }

  // Parse legacy preferences.
  if (allowed_list_pref) {
    for (const auto& entry : *allowed_list_pref) {
      if (entry.is_string() && crx_file::id_util::IdIsValid(entry.GetString()))
        AccessById(entry.GetString())->installation_mode = INSTALLATION_ALLOWED;
    }
  }

  if (denied_list_pref) {
    for (const auto& entry : *denied_list_pref) {
      if (entry.is_string() && crx_file::id_util::IdIsValid(entry.GetString()))
        AccessById(entry.GetString())->installation_mode = INSTALLATION_BLOCKED;
    }
  }

  UpdateForcedExtensions(forced_list_pref);

  if (install_sources_pref) {
    global_settings_->install_sources = URLPatternSet();
    for (const auto& entry : *install_sources_pref) {
      if (entry.is_string()) {
        std::string url_pattern = entry.GetString();
        URLPattern pattern(URLPattern::SCHEME_ALL);
        if (pattern.Parse(url_pattern) == URLPattern::ParseResult::kSuccess) {
          global_settings_->install_sources->AddPattern(pattern);
        } else {
          LOG(WARNING) << "Invalid URL pattern in for preference "
                       << pref_names::kAllowedInstallSites << ": "
                       << url_pattern << ".";
        }
      }
    }
  }

  if (allowed_types_pref) {
    global_settings_->allowed_types.emplace();
    for (const auto& entry : *allowed_types_pref) {
      if (entry.is_int() && entry.GetInt() >= 0 &&
          entry.GetInt() < Manifest::Type::NUM_LOAD_TYPES) {
        global_settings_->allowed_types->push_back(
            static_cast<Manifest::Type>(entry.GetInt()));
      } else if (entry.is_string()) {
        Manifest::Type manifest_type =
            schema_constants::GetManifestType(entry.GetString());
        if (manifest_type != Manifest::TYPE_UNKNOWN)
          global_settings_->allowed_types->push_back(manifest_type);
      }
    }
  }

  if (manifest_v2_pref) {
    global_settings_->manifest_v2_setting =
        static_cast<internal::GlobalSettings::ManifestV2Setting>(
            manifest_v2_pref->GetInt());
  }

  if (unpublished_availability_pref) {
    global_settings_->unpublished_availability_setting =
        static_cast<internal::GlobalSettings::UnpublishedAvailability>(
            unpublished_availability_pref->GetInt());
  }

  if (dict_pref) {
    // Parse new extension management preference.

    bool defer_load_settings = base::FeatureList::IsEnabled(
        features::kExtensionDeferredIndividualSettings);
    std::unordered_set<std::string> installed_extensions;
    if (defer_load_settings) {
      auto* extension_prefs = ExtensionPrefs::Get(profile_);
      auto extensions_info = extension_prefs->GetInstalledExtensionsInfo();
      for (const auto& extension_info : extensions_info) {
        installed_extensions.insert(extension_info.extension_id);
      }
    }

    for (auto iter : *dict_pref) {
      if (iter.first == schema_constants::kWildcard)
        continue;
      const base::Value::Dict* subdict = iter.second.GetIfDict();
      if (!subdict)
        continue;
      if (base::StartsWith(iter.first, schema_constants::kUpdateUrlPrefix,
                           base::CompareCase::SENSITIVE)) {
        const std::string& update_url =
            iter.first.substr(strlen(schema_constants::kUpdateUrlPrefix));
        if (!GURL(update_url).is_valid()) {
          LOG(WARNING) << "Invalid update URL: " << update_url << ".";
          continue;
        }
        internal::IndividualSettings* by_update_url =
            AccessByUpdateUrl(update_url);
        if (!by_update_url->Parse(
                *subdict, internal::IndividualSettings::SCOPE_UPDATE_URL)) {
          settings_by_update_url_.erase(update_url);
          LOG(WARNING) << "Malformed Extension Management settings for "
                          "extensions with update url: "
                       << update_url << ".";
        }
      } else {
        std::vector<std::string> extension_ids = base::SplitString(
            iter.first, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
        for (const auto& extension_id : extension_ids) {
          if (!crx_file::id_util::IdIsValid(extension_id)) {
            SYSLOG(WARNING) << "Invalid extension ID : " << extension_id << ".";
            continue;
          }

          if (defer_load_settings) {
            auto should_defer = [&extension_id, &installed_extensions](
                                    const base::Value::Dict& dict,
                                    const SettingsIdMap* settings_by_id) {
              // If in legacy force list, don't defer since already have an
              // entry. This ensures that the entry in these settings matches
              // the entry in the forcelist. Also don't defer if the extension
              // is installed.
              if (base::Contains(*settings_by_id, extension_id) ||
                  base::Contains(installed_extensions, extension_id)) {
                return false;
              }
              auto* install_mode =
                  dict.FindString(schema_constants::kInstallationMode);
              if (!install_mode)
                return true;
              // Don't defer if the extension needs to be installed.
              return *install_mode != schema_constants::kForceInstalled &&
                     *install_mode != schema_constants::kNormalInstalled;
            };

            if (should_defer(*subdict, &settings_by_id_)) {
              deferred_ids_.insert(extension_id);
              continue;
            }
          }

          internal::IndividualSettings* by_id = AccessById(extension_id);
          const bool included_in_forcelist =
              by_id->installation_mode == InstallationMode::INSTALLATION_FORCED;
          if (!ParseById(extension_id, *subdict))
            continue;

          // If applying the ExtensionSettings policy changes installation mode
          // from force-installed to anything else, the extension might not get
          // installed and will get stuck in CREATED stage.
          if (included_in_forcelist &&
              by_id->installation_mode !=
                  InstallationMode::INSTALLATION_FORCED) {
            InstallStageTracker::Get(profile_)->ReportFailure(
                extension_id,
                InstallStageTracker::FailureReason::OVERRIDDEN_BY_SETTINGS);
          }
        }
      }
    }
  }
}

bool ExtensionManagement::ParseById(const std::string& extension_id,
                                    const base::Value::Dict& subdict) {
  internal::IndividualSettings* by_id = AccessById(extension_id);
  if (by_id->Parse(subdict, internal::IndividualSettings::SCOPE_INDIVIDUAL))
    return true;

  settings_by_id_.erase(extension_id);
  InstallStageTracker::Get(profile_)->ReportFailure(
      extension_id,
      InstallStageTracker::FailureReason::MALFORMED_EXTENSION_SETTINGS);
  SYSLOG(WARNING) << "Malformed Extension Management settings for "
                  << extension_id << ".";
  return false;
}

internal::IndividualSettings* ExtensionManagement::GetSettingsForId(
    const std::string& extension_id) {
  if (base::Contains(deferred_ids_, extension_id))
    LoadDeferredExtensionSetting(extension_id);

  auto iter_id = settings_by_id_.find(extension_id);
  if (iter_id == settings_by_id_.end())
    return nullptr;

  return iter_id->second.get();
}

void ExtensionManagement::LoadDeferredExtensionSetting(
    const std::string& extension_id) {
  DCHECK(base::Contains(deferred_ids_, extension_id));

  // No need to check again later.
  deferred_ids_.erase(extension_id);

  const base::Value::Dict* dict_pref =
      LoadDictPreference(pref_names::kExtensionManagement, true);
  bool found = false;
  for (auto iter : *dict_pref) {
    if (iter.first == schema_constants::kWildcard ||
        base::StartsWith(iter.first, schema_constants::kUpdateUrlPrefix,
                         base::CompareCase::SENSITIVE)) {
      continue;
    }
    const base::Value::Dict* subdict = iter.second.GetIfDict();
    if (!subdict)
      continue;

    auto extension_ids = base::SplitStringPiece(
        iter.first, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (base::Contains(extension_ids, extension_id)) {
      // Found our settings. After parsing, continue looking for more entries.
      ParseById(extension_id, *subdict);
      found = true;
    }
  }

  DCHECK(found) << "Couldn't find dictionary for extension in deferred_ids_.";
}

const base::Value* ExtensionManagement::LoadPreference(
    const char* pref_name,
    bool force_managed,
    base::Value::Type expected_type) const {
  if (!pref_service_)
    return nullptr;
  const PrefService::Preference* pref =
      pref_service_->FindPreference(pref_name);
  if (pref && !pref->IsDefaultValue() &&
      (!force_managed || pref->IsManaged())) {
    const base::Value* value = pref->GetValue();
    if (value && value->type() == expected_type)
      return value;
  }
  return nullptr;
}

const base::Value::Dict* ExtensionManagement::LoadDictPreference(
    const char* pref_name,
    bool force_managed) const {
  const base::Value* value =
      LoadPreference(pref_name, force_managed, base::Value::Type::DICT);
  return value ? &value->GetDict() : nullptr;
}

const base::Value::List* ExtensionManagement::LoadListPreference(
    const char* pref_name,
    bool force_managed) const {
  const base::Value* value =
      LoadPreference(pref_name, force_managed, base::Value::Type::LIST);
  return value ? &value->GetList() : nullptr;
}

void ExtensionManagement::OnExtensionPrefChanged() {
  Refresh();
  NotifyExtensionManagementPrefChanged();
}

void ExtensionManagement::NotifyExtensionManagementPrefChanged() {
  ReportExtensionManagementInstallCreationStage(
      InstallStageTracker::InstallCreationStage::NOTIFIED_FROM_MANAGEMENT,
      InstallStageTracker::InstallCreationStage::
          NOTIFIED_FROM_MANAGEMENT_NOT_FORCED);
  for (auto& observer : observer_list_)
    observer.OnExtensionManagementSettingsChanged();
}

void ExtensionManagement::ReportExtensionManagementInstallCreationStage(
    InstallStageTracker::InstallCreationStage forced_stage,
    InstallStageTracker::InstallCreationStage other_stage) {
  InstallStageTracker* install_stage_tracker =
      InstallStageTracker::Get(profile_);
  for (const auto& entry : settings_by_id_) {
    if (entry.second->installation_mode == INSTALLATION_FORCED) {
      install_stage_tracker->ReportInstallCreationStage(entry.first,
                                                        forced_stage);
    } else {
      install_stage_tracker->ReportInstallCreationStage(entry.first,
                                                        other_stage);
    }
  }
}

base::Value::Dict ExtensionManagement::GetInstallListByMode(
    InstallationMode installation_mode) const {
  // This is only meaningful if we 've loaded the extensions for the given
  // installation mode.
  DCHECK(installation_mode == INSTALLATION_FORCED ||
         installation_mode == INSTALLATION_RECOMMENDED);

  base::Value::Dict extension_dict;
  for (const auto& [id, settings] : settings_by_id_) {
    if (settings->installation_mode == installation_mode) {
      ExternalPolicyLoader::AddExtension(extension_dict, id,
                                         settings->update_url);
    }
  }
  return extension_dict;
}

void ExtensionManagement::UpdateForcedExtensions(
    const base::Value::Dict* extension_dict) {
  if (!extension_dict)
    return;

  InstallStageTracker* install_stage_tracker =
      InstallStageTracker::Get(profile_);
  for (auto it : *extension_dict) {
    if (!crx_file::id_util::IdIsValid(it.first)) {
      install_stage_tracker->ReportFailure(
          it.first, InstallStageTracker::FailureReason::INVALID_ID);
      continue;
    }
    const base::Value::Dict* dict_value = it.second.GetIfDict();
    if (!dict_value) {
      install_stage_tracker->ReportFailure(
          it.first, InstallStageTracker::FailureReason::NO_UPDATE_URL);
      continue;
    }
    const std::string* update_url =
        dict_value->FindString(ExternalProviderImpl::kExternalUpdateUrl);
    if (!update_url) {
      install_stage_tracker->ReportFailure(
          it.first, InstallStageTracker::FailureReason::NO_UPDATE_URL);
      continue;
    }
    internal::IndividualSettings* by_id = AccessById(it.first);
    by_id->installation_mode = INSTALLATION_FORCED;
    by_id->update_url = *update_url;
    install_stage_tracker->ReportInstallationStage(
        it.first, InstallStageTracker::Stage::CREATED);
    install_stage_tracker->ReportInstallCreationStage(
        it.first,
        InstallStageTracker::InstallCreationStage::CREATION_INITIATED);
  }
}

internal::IndividualSettings* ExtensionManagement::AccessById(
    const ExtensionId& id) {
  DCHECK(crx_file::id_util::IdIsValid(id)) << "Invalid ID: " << id;
  std::unique_ptr<internal::IndividualSettings>& settings = settings_by_id_[id];
  if (!settings) {
    settings =
        std::make_unique<internal::IndividualSettings>(default_settings_.get());
  }
  return settings.get();
}

internal::IndividualSettings* ExtensionManagement::AccessByUpdateUrl(
    const std::string& update_url) {
  DCHECK(GURL(update_url).is_valid()) << "Invalid update URL: " << update_url;
  std::unique_ptr<internal::IndividualSettings>& settings =
      settings_by_update_url_[update_url];
  if (!settings) {
    settings =
        std::make_unique<internal::IndividualSettings>(default_settings_.get());
  }
  return settings.get();
}

// static
ExtensionManagement* ExtensionManagementFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionManagement*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionManagementFactory* ExtensionManagementFactory::GetInstance() {
  return base::Singleton<ExtensionManagementFactory>::get();
}

ExtensionManagementFactory::ExtensionManagementFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionManagement",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(InstallStageTrackerFactory::GetInstance());
}

ExtensionManagementFactory::~ExtensionManagementFactory() {}

std::unique_ptr<KeyedService>
ExtensionManagementFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  TRACE_EVENT0("browser,startup",
               "ExtensionManagementFactory::BuildServiceInstanceFor");
  return std::make_unique<ExtensionManagement>(
      Profile::FromBrowserContext(context));
}

void ExtensionManagementFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterDictionaryPref(pref_names::kExtensionManagement);
}

}  // namespace extensions
