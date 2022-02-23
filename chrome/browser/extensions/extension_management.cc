// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
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
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
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

namespace extensions {

ExtensionManagement::ExtensionManagement(Profile* profile)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      is_child_(profile_->IsChild()) {
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
      std::make_unique<StandardManagementPolicyProvider>(this));
  providers_.push_back(
      std::make_unique<PermissionsBasedManagementPolicyProvider>(this));
}

ExtensionManagement::~ExtensionManagement() {
}

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

std::unique_ptr<base::DictionaryValue>
ExtensionManagement::GetForceInstallList() const {
  return GetInstallListByMode(INSTALLATION_FORCED);
}

std::unique_ptr<base::DictionaryValue>
ExtensionManagement::GetRecommendedInstallList() const {
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
  if (!global_settings_->has_restricted_install_sources)
    return false;

  const URLPatternSet& url_patterns = global_settings_->install_sources;

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

  if (!global_settings_->has_restricted_allowed_types)
    return true;
  const std::vector<Manifest::Type>& allowed_types =
      global_settings_->allowed_types;
  return base::Contains(allowed_types, manifest_type);
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

bool ExtensionManagement::IsPolicyBlockedHost(const Extension* extension,
                                              const GURL& url) {
  auto* setting = GetSettingsForId(extension->id());
  if (setting)
    return setting->policy_blocked_hosts.MatchesURL(url);
  return default_settings_->policy_blocked_hosts.MatchesURL(url);
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
  for (const extensions::APIPermission* blocked_api :
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
  const base::ListValue* allowed_list_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kInstallAllowList, true, base::Value::Type::LIST));
  // Allow user to use preference to block certain extensions. Note that policy
  // managed forcelist or allowlist will always override this.
  const base::ListValue* denied_list_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kInstallDenyList, false, base::Value::Type::LIST));
  const base::DictionaryValue* forced_list_pref =
      static_cast<const base::DictionaryValue*>(LoadPreference(
          pref_names::kInstallForceList, true, base::Value::Type::DICTIONARY));
  const base::ListValue* install_sources_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kAllowedInstallSites, true, base::Value::Type::LIST));
  const base::ListValue* allowed_types_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kAllowedTypes, true, base::Value::Type::LIST));
  const base::DictionaryValue* dict_pref =
      static_cast<const base::DictionaryValue*>(
          LoadPreference(pref_names::kExtensionManagement, true,
                         base::Value::Type::DICTIONARY));
  const base::Value* extension_request_pref = LoadPreference(
      prefs::kCloudExtensionRequestEnabled, false, base::Value::Type::BOOLEAN);

  // Reset all settings.
  global_settings_ = std::make_unique<internal::GlobalSettings>();
  settings_by_id_.clear();
  deferred_ids_.clear();
  default_settings_ = std::make_unique<internal::IndividualSettings>();

  // Parse default settings.
  const base::Value wildcard("*");
  if ((denied_list_pref &&
       // TODO(crbug.com/1187106): Use base::Contains once |denied_list_pref| is
       // not a ListValue.
       std::find(denied_list_pref->GetListDeprecated().begin(),
                 denied_list_pref->GetListDeprecated().end(),
                 wildcard) != denied_list_pref->GetListDeprecated().end()) ||
      (extension_request_pref && extension_request_pref->GetBool())) {
    default_settings_->installation_mode = INSTALLATION_BLOCKED;
  }

  const base::DictionaryValue* subdict = NULL;
  if (dict_pref &&
      dict_pref->GetDictionary(schema_constants::kWildcard, &subdict)) {
    if (!default_settings_->Parse(
            subdict, internal::IndividualSettings::SCOPE_DEFAULT)) {
      LOG(WARNING) << "Default extension management settings parsing error.";
      default_settings_->Reset();
    }

    // Settings from new preference have higher priority over legacy ones.
    const base::ListValue* list_value = NULL;
    if (subdict->GetList(schema_constants::kInstallSources, &list_value))
      install_sources_pref = list_value;
    if (subdict->GetList(schema_constants::kAllowedTypes, &list_value))
      allowed_types_pref = list_value;
  }

  // Parse legacy preferences.
  if (allowed_list_pref) {
    for (const auto& entry : allowed_list_pref->GetListDeprecated()) {
      if (entry.is_string() && crx_file::id_util::IdIsValid(entry.GetString()))
        AccessById(entry.GetString())->installation_mode = INSTALLATION_ALLOWED;
    }
  }

  if (denied_list_pref) {
    for (const auto& entry : denied_list_pref->GetListDeprecated()) {
      if (entry.is_string() && crx_file::id_util::IdIsValid(entry.GetString()))
        AccessById(entry.GetString())->installation_mode = INSTALLATION_BLOCKED;
    }
  }

  UpdateForcedExtensions(forced_list_pref);

  if (install_sources_pref) {
    global_settings_->has_restricted_install_sources = true;
    for (const auto& entry : install_sources_pref->GetListDeprecated()) {
      if (entry.is_string()) {
        std::string url_pattern = entry.GetString();
        URLPattern pattern(URLPattern::SCHEME_ALL);
        if (pattern.Parse(url_pattern) == URLPattern::ParseResult::kSuccess) {
          global_settings_->install_sources.AddPattern(pattern);
        } else {
          LOG(WARNING) << "Invalid URL pattern in for preference "
                       << pref_names::kAllowedInstallSites << ": "
                       << url_pattern << ".";
        }
      }
    }
  }

  if (allowed_types_pref) {
    global_settings_->has_restricted_allowed_types = true;
    for (const auto& entry : allowed_types_pref->GetListDeprecated()) {
      if (entry.is_int() && entry.GetInt() >= 0 &&
          entry.GetInt() < Manifest::Type::NUM_LOAD_TYPES) {
        global_settings_->allowed_types.push_back(
            static_cast<Manifest::Type>(entry.GetInt()));
      } else if (entry.is_string()) {
        Manifest::Type manifest_type =
            schema_constants::GetManifestType(entry.GetString());
        if (manifest_type != Manifest::TYPE_UNKNOWN)
          global_settings_->allowed_types.push_back(manifest_type);
      }
    }
  }

  if (dict_pref) {
    // Parse new extension management preference.

    bool defer_load_settings = base::FeatureList::IsEnabled(
        features::kExtensionDeferredIndividualSettings);
    std::unordered_set<std::string> installed_extensions;
    if (defer_load_settings) {
      auto* extension_prefs = ExtensionPrefs::Get(profile_);
      auto extensions_info = extension_prefs->GetInstalledExtensionsInfo();
      for (auto& extension_info : *extensions_info)
        installed_extensions.insert(extension_info->extension_id);
    }

    for (auto iter : dict_pref->DictItems()) {
      if (iter.first == schema_constants::kWildcard)
        continue;
      if (!iter.second.GetAsDictionary(&subdict))
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
                subdict, internal::IndividualSettings::SCOPE_UPDATE_URL)) {
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
                                    const base::Value* subdict,
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
                  subdict->FindStringKey(schema_constants::kInstallationMode);
              if (!install_mode)
                return true;
              // Don't defer if the extension needs to be installed.
              return *install_mode != schema_constants::kForceInstalled &&
                     *install_mode != schema_constants::kNormalInstalled;
            };

            if (should_defer(subdict, &settings_by_id_)) {
              deferred_ids_.insert(extension_id);
              continue;
            }
          }

          internal::IndividualSettings* by_id = AccessById(extension_id);
          const bool included_in_forcelist =
              by_id->installation_mode == InstallationMode::INSTALLATION_FORCED;
          if (!ParseById(extension_id, subdict))
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
    size_t force_pinned_count = GetForcePinnedList().size();
    if (force_pinned_count > 0) {
      base::UmaHistogramCounts100("Extensions.ForceToolbarPinnedCount2",
                                  force_pinned_count);
    }
  }
}

bool ExtensionManagement::ParseById(const std::string& extension_id,
                                    const base::DictionaryValue* subdict) {
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

  const base::DictionaryValue* dict_pref =
      static_cast<const base::DictionaryValue*>(
          LoadPreference(pref_names::kExtensionManagement, true,
                         base::Value::Type::DICTIONARY));
  bool found = false;
  for (auto iter : dict_pref->DictItems()) {
    if (iter.first == schema_constants::kWildcard ||
        base::StartsWith(iter.first, schema_constants::kUpdateUrlPrefix,
                         base::CompareCase::SENSITIVE)) {
      continue;
    }
    const base::DictionaryValue* subdict = nullptr;
    if (!iter.second.GetAsDictionary(&subdict))
      continue;

    auto extension_ids = base::SplitStringPiece(
        iter.first, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (base::Contains(extension_ids, extension_id)) {
      // Found our settings. After parsing, continue looking for more entries.
      ParseById(extension_id, subdict);
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

std::unique_ptr<base::DictionaryValue>
ExtensionManagement::GetInstallListByMode(
    InstallationMode installation_mode) const {
  // This is only meaningful if we 've loaded the extensions for the given
  // installation mode.
  DCHECK(installation_mode == INSTALLATION_FORCED ||
         installation_mode == INSTALLATION_RECOMMENDED);

  auto extension_dict = std::make_unique<base::DictionaryValue>();
  for (const auto& entry : settings_by_id_) {
    if (entry.second->installation_mode == installation_mode) {
      ExternalPolicyLoader::AddExtension(extension_dict.get(), entry.first,
                                         entry.second->update_url);
    }
  }
  return extension_dict;
}

void ExtensionManagement::UpdateForcedExtensions(
    const base::DictionaryValue* extension_dict) {
  if (!extension_dict)
    return;

  InstallStageTracker* install_stage_tracker =
      InstallStageTracker::Get(profile_);
  for (auto it : extension_dict->DictItems()) {
    if (!crx_file::id_util::IdIsValid(it.first)) {
      install_stage_tracker->ReportFailure(
          it.first, InstallStageTracker::FailureReason::INVALID_ID);
      continue;
    }
    const base::DictionaryValue* dict_value = nullptr;
    if (!it.second.GetAsDictionary(&dict_value)) {
      install_stage_tracker->ReportFailure(
          it.first, InstallStageTracker::FailureReason::NO_UPDATE_URL);
      continue;
    }
    const std::string* update_url =
        dict_value->FindStringKey(ExternalProviderImpl::kExternalUpdateUrl);
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
    : BrowserContextKeyedServiceFactory(
          "ExtensionManagement",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(InstallStageTrackerFactory::GetInstance());
}

ExtensionManagementFactory::~ExtensionManagementFactory() {}

KeyedService* ExtensionManagementFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  TRACE_EVENT0("browser,startup",
               "ExtensionManagementFactory::BuildServiceInstanceFor");
  return new ExtensionManagement(Profile::FromBrowserContext(context));
}

content::BrowserContext* ExtensionManagementFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

void ExtensionManagementFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterDictionaryPref(pref_names::kExtensionManagement);
}

}  // namespace extensions
