// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_functions_shared.h"

#include "base/barrier_closure.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_event_router.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/api/developer_private/profile_info_generator.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/ui_util.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/permissions/permissions_data.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "extensions/browser/ui_util.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace extensions {

namespace developer = api::developer_private;

namespace {
const char kCannotUpdateChildAccountProfileSettingsError[] =
    "Cannot change settings for a child account profile.";

std::optional<URLPattern> ParseRuntimePermissionsPattern(
    const std::string& pattern_str) {
  constexpr int kValidRuntimePermissionSchemes = URLPattern::SCHEME_HTTP |
                                                 URLPattern::SCHEME_HTTPS |
                                                 URLPattern::SCHEME_FILE;

  URLPattern pattern(kValidRuntimePermissionSchemes);
  if (pattern.Parse(pattern_str) != URLPattern::ParseResult::kSuccess) {
    return std::nullopt;
  }

  // We don't allow adding paths for permissions, because they aren't meaningful
  // in terms of origin access. The frontend should validate this, but there's
  // a chance something can slip through, so we should fail gracefully.
  if (pattern.path() != "/*") {
    return std::nullopt;
  }

  return pattern;
}

GURL ConvertHostToUrl(const std::string& host) {
  return GURL(base::StrCat(
      {url::kHttpScheme, url::kStandardSchemeSeparator, host, "/"}));
}

#if !BUILDFLAG(IS_ANDROID)
// Runs the install verifier for all extensions that are enabled, disabled, or
// terminated.
void PerformVerificationCheck(content::BrowserContext* context) {
  const ExtensionSet extensions =
      ExtensionRegistry::Get(context)->GenerateInstalledExtensionsSet(
          ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
          ExtensionRegistry::TERMINATED);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  bool should_do_verification_check = false;
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (ui_util::ShouldDisplayInExtensionSettings(*extension) &&
        prefs->HasDisableReason(extension->id(),
                                disable_reason::DISABLE_NOT_VERIFIED)) {
      should_do_verification_check = true;
      break;
    }
  }

  if (should_do_verification_check) {
    InstallVerifier::Get(context)->VerifyAllExtensions();
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

std::string GetETldPlusOne(const GURL& site) {
  DCHECK(site.is_valid());
  std::string etld_plus_one =
      net::registry_controlled_domains::GetDomainAndRegistry(
          site, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return etld_plus_one.empty() ? site.host() : etld_plus_one;
}

developer::SiteInfo CreateSiteInfo(const std::string& site,
                                   developer::SiteSet site_set,
                                   size_t num_extensions = 0u) {
  developer::SiteInfo site_info;
  site_info.site = site;
  site_info.site_set = site_set;
  site_info.num_extensions = num_extensions;
  return site_info;
}

// Adds `site` grouped under `etld_plus_one` into `site_groups`. This function
// is a no-op if `site` already exists inside the SiteGroup for `etld_plus_one`.
void AddSiteToSiteGroups(
    std::map<std::string, developer::SiteGroup>* site_groups,
    const std::string& site,
    const std::string& etld_plus_one,
    developer::SiteSet site_set) {
  auto [it, inserted] = site_groups->try_emplace(etld_plus_one);
  if (inserted) {
    it->second.etld_plus_one = etld_plus_one;
    it->second.sites.push_back(CreateSiteInfo(site, site_set));
  } else if (!base::Contains(it->second.sites, site,
                             &developer::SiteInfo::site)) {
    it->second.sites.push_back(CreateSiteInfo(site, site_set));
  }
}

// Adds an extension's granted host permissions in `distinct_hosts` to
// `site_groups`,
void ProcessSitesForRuntimeHostPermissions(
    std::map<std::string, developer::SiteGroup>* site_groups,
    const std::vector<URLPattern>& distinct_hosts) {
  for (const auto& pattern : distinct_hosts) {
    // Do not add the pattern if it matches an overly broad set of urls (all
    // urls under one or all top level domains).
    if (pattern.match_all_urls() || pattern.host().empty() ||
        pattern.MatchesEffectiveTld()) {
      continue;
    }

    std::string etld_plus_one =
        GetETldPlusOne(ConvertHostToUrl(pattern.host()));
    // Process the site if:
    // 1) It does not match any subdomains, or:
    // 2) It matches subdomains but the host portion does not equal
    //    `etld_plus_one`. This treats patterns such as "*.sub.etldplusone.com"
    //    as just "sub.etldplusone.com" and prevents "*.etldplusone.com" from
    //    being processed as "etldplusone.com", since such patterns will be
    //    processed separately.
    if (!pattern.match_subdomains() || pattern.host() != etld_plus_one) {
      AddSiteToSiteGroups(site_groups, pattern.host(), etld_plus_one,
                          developer::SiteSet::kExtensionSpecified);
    }
  }
}

// Updates num_extensions counts in `site_groups` for `granted_hosts` from one
// extension.
void UpdateSiteGroupCountsForExtensionHosts(
    std::map<std::string, developer::SiteGroup>* site_groups,
    std::map<std::string, size_t>* match_subdomains_count,
    const URLPatternSet& granted_hosts) {
  for (auto& entry : *site_groups) {
    bool can_run_on_site_group = false;
    // For each site under the eTLD+1, increment num_extensions if the extension
    // can access the site.
    for (developer::SiteInfo& site_info : entry.second.sites) {
      // When updating num_extensions counts, only look at extension specified
      // hosts as num_extensions is not useful for user specified hosts. (i.e.
      // user permitted sites can be accessed to any extensions that specify the
      // site in their host permissions, user restricted sites cannot be
      // accessed by any extensions.)
      if (site_info.site_set != developer::SiteSet::kExtensionSpecified) {
        continue;
      }

      if (granted_hosts.MatchesHost(ConvertHostToUrl(site_info.site),
                                    /*require_match_subdomains=*/false)) {
        can_run_on_site_group = true;
        site_info.num_extensions++;
      }
    }

    // Check if the extension can run on all sites under this eTLD+1 and
    // update `match_subdomains_count` for this eTLD+1. The SiteInfo entry will
    // be created later if at least one extension can run on all subdomains.
    if (granted_hosts.MatchesHost(ConvertHostToUrl(entry.first),
                                  /*require_match_subdomains=*/true)) {
      (*match_subdomains_count)[entry.first]++;
      can_run_on_site_group = true;
    }

    if (can_run_on_site_group) {
      entry.second.num_extensions++;
    }
  }
}

}  // namespace

namespace api {

DeveloperPrivateAPIFunction::~DeveloperPrivateAPIFunction() = default;

const Extension* DeveloperPrivateAPIFunction::GetExtensionById(
    const ExtensionId& id) {
  return ExtensionRegistry::Get(browser_context())
      ->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
}

const Extension* DeveloperPrivateAPIFunction::GetEnabledExtensionById(
    const ExtensionId& id) {
  return ExtensionRegistry::Get(browser_context())
      ->enabled_extensions()
      .GetByID(id);
}

DeveloperPrivateGetExtensionsInfoFunction::
    DeveloperPrivateGetExtensionsInfoFunction() = default;

DeveloperPrivateGetExtensionsInfoFunction::
    ~DeveloperPrivateGetExtensionsInfoFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionsInfoFunction::Run() {
  std::optional<developer::GetExtensionsInfo::Params> params =
      developer::GetExtensionsInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool include_disabled = true;
  bool include_terminated = true;
  if (params->options) {
    if (params->options->include_disabled) {
      include_disabled = *params->options->include_disabled;
    }
    if (params->options->include_terminated) {
      include_terminated = *params->options->include_terminated;
    }
  }

  info_generator_ = std::make_unique<ExtensionInfoGenerator>(browser_context());
  info_generator_->CreateExtensionsInfo(
      include_disabled, include_terminated,
      base::BindOnce(
          &DeveloperPrivateGetExtensionsInfoFunction::OnInfosGenerated, this));

  return RespondLater();
}

void DeveloperPrivateGetExtensionsInfoFunction::OnInfosGenerated(
    ExtensionInfoGenerator::ExtensionInfoList list) {
  Respond(ArgumentList(developer::GetExtensionsInfo::Results::Create(list)));
}

DeveloperPrivateGetExtensionInfoFunction::
    DeveloperPrivateGetExtensionInfoFunction() = default;

DeveloperPrivateGetExtensionInfoFunction::
    ~DeveloperPrivateGetExtensionInfoFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionInfoFunction::Run() {
  std::optional<developer::GetExtensionInfo::Params> params =
      developer::GetExtensionInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  info_generator_ = std::make_unique<ExtensionInfoGenerator>(browser_context());
  info_generator_->CreateExtensionInfo(
      params->id,
      base::BindOnce(
          &DeveloperPrivateGetExtensionInfoFunction::OnInfosGenerated, this));

  return RespondLater();
}

void DeveloperPrivateGetExtensionInfoFunction::OnInfosGenerated(
    ExtensionInfoGenerator::ExtensionInfoList list) {
  DCHECK_LE(1u, list.size());
  Respond(list.empty() ? Error(kNoSuchExtensionError)
                       : WithArguments(list[0].ToValue()));
}

DeveloperPrivateGetProfileConfigurationFunction::
    ~DeveloperPrivateGetProfileConfigurationFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetProfileConfigurationFunction::Run() {
  developer::ProfileInfo info =
      CreateProfileInfo(Profile::FromBrowserContext(browser_context()));

#if !BUILDFLAG(IS_ANDROID)
  // If this is called from the chrome://extensions page, we use this as a
  // heuristic that it's a good time to verify installs. We do this on startup,
  // but there's a chance that it failed erroneously, so it's good to double-
  // check.
  if (source_context_type() == mojom::ContextType::kWebUi) {
    PerformVerificationCheck(browser_context());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return RespondNow(WithArguments(info.ToValue()));
}

DeveloperPrivateUpdateProfileConfigurationFunction::
    ~DeveloperPrivateUpdateProfileConfigurationFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateProfileConfigurationFunction::Run() {
  std::optional<developer::UpdateProfileConfiguration::Params> params =
      developer::UpdateProfileConfiguration::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const developer::ProfileConfigurationUpdate& update = params->update;

  if (update.in_developer_mode) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    CHECK(profile);
    if (supervised_user::AreExtensionsPermissionsEnabled(profile)) {
      return RespondNow(Error(kCannotUpdateChildAccountProfileSettingsError));
    }
    util::SetDeveloperModeForProfile(profile, *update.in_developer_mode);
  }

// Consider the deprecation notice already dismissed on Android.
#if !BUILDFLAG(IS_ANDROID)
  if (update.is_mv2_deprecation_notice_dismissed.value_or(false)) {
    ManifestV2ExperimentManager::Get(browser_context())
        ->MarkNoticeAsAcknowledgedGlobally();
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return RespondNow(NoArguments());
}

DeveloperPrivateUpdateExtensionConfigurationFunction::
    ~DeveloperPrivateUpdateExtensionConfigurationFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateExtensionConfigurationFunction::Run() {
  std::optional<developer::UpdateExtensionConfiguration::Params> params =
      developer::UpdateExtensionConfiguration::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const developer::ExtensionConfigurationUpdate& update = params->update;

  const Extension* extension = GetExtensionById(update.extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  // The chrome://extensions page uses toggles which, when dragged, do not
  // invoke a user gesture. Work around this for the chrome://extensions page.
  // TODO(dpapad): Remove this exemption when sliding a toggle counts as a
  // gesture.
  bool allowed =
      source_context_type() == mojom::ContextType::kWebUi || user_gesture();
  if (!allowed) {
    return RespondNow(Error(kRequiresUserGestureError));
  }

  if (update.file_access) {
    util::SetAllowFileAccess(extension->id(), browser_context(),
                             *update.file_access);
  }
  if (update.incognito_access) {
    util::SetIsIncognitoEnabled(extension->id(), browser_context(),
                                *update.incognito_access);
  }
  if (update.user_scripts_access) {
    ExtensionSystem::Get(browser_context())
        ->user_script_manager()
        ->SetUserScriptPrefEnabled(extension->id(),
                                   *update.user_scripts_access);
  }
  if (update.error_collection) {
    ErrorConsole::Get(browser_context())
        ->SetReportingAllForExtension(extension->id(),
                                      *update.error_collection);
  }
  if (update.host_access != developer::HostAccess::kNone) {
    PermissionsManager* manager = PermissionsManager::Get(browser_context());
    if (!manager->CanAffectExtension(*extension)) {
      return RespondNow(Error(kCannotChangeHostPermissions));
    }

    ScriptingPermissionsModifier modifier(browser_context(), extension);
    switch (update.host_access) {
      case developer::HostAccess::kOnClick:
        modifier.SetWithholdHostPermissions(true);
        modifier.RemoveAllGrantedHostPermissions();
        break;
      case developer::HostAccess::kOnSpecificSites:
        if (manager->HasBroadGrantedHostPermissions(*extension)) {
          modifier.RemoveBroadGrantedHostPermissions();
        }
        modifier.SetWithholdHostPermissions(true);
        break;
      case developer::HostAccess::kOnAllSites:
        modifier.SetWithholdHostPermissions(false);
        break;
      case developer::HostAccess::kNone:
        NOTREACHED();
    }
  }
  if (update.acknowledge_safety_check_warning_reason !=
      developer_private::SafetyCheckWarningReason::kNone) {
    ExtensionPrefs::Get(browser_context())
        ->SetIntegerPref(
            extension->id(), kPrefAcknowledgeSafetyCheckWarningReason,
            static_cast<int>(update.acknowledge_safety_check_warning_reason));
    DeveloperPrivateEventRouter* event_router =
        DeveloperPrivateAPI::Get(browser_context())
            ->developer_private_event_router();
    if (event_router) {
      event_router->OnExtensionConfigurationChanged(extension->id());
    }
  }
// TODO(crbug.com/392777363): Enable this code when toolbars are supported on
// desktop Android.
#if !BUILDFLAG(IS_ANDROID)
  if (update.show_access_requests_in_toolbar) {
    SitePermissionsHelper(Profile::FromBrowserContext(browser_context()))
        .SetShowAccessRequestsInToolbar(
            extension->id(), *update.show_access_requests_in_toolbar);
  }
  if (update.pinned_to_toolbar) {
    ToolbarActionsModel* toolbar_actions_model = ToolbarActionsModel::Get(
        Profile::FromBrowserContext(browser_context()));
    if (!toolbar_actions_model->HasAction(extension->id())) {
      return RespondNow(Error(kCannotSetPinnedWithoutAction));
    }

    bool is_action_pinned =
        toolbar_actions_model->IsActionPinned(extension->id());
    if (is_action_pinned != *update.pinned_to_toolbar) {
      toolbar_actions_model->SetActionVisibility(extension->id(),
                                                 !is_action_pinned);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DeveloperPrivateIsProfileManagedFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  return RespondNow(WithArguments(
      profile && supervised_user::AreExtensionsPermissionsEnabled(profile)));
}

DeveloperPrivateIsProfileManagedFunction::
    ~DeveloperPrivateIsProfileManagedFunction() = default;

DeveloperPrivateDeleteExtensionErrorsFunction::
    ~DeveloperPrivateDeleteExtensionErrorsFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateDeleteExtensionErrorsFunction::Run() {
  std::optional<developer::DeleteExtensionErrors::Params> params =
      developer::DeleteExtensionErrors::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::DeleteExtensionErrorsProperties& properties =
      params->properties;

  ErrorConsole* error_console = ErrorConsole::Get(browser_context());
  int type = -1;
  if (properties.type != developer::ErrorType::kNone) {
    type = properties.type == developer::ErrorType::kManifest
               ? static_cast<int>(ExtensionError::Type::kManifestError)
               : static_cast<int>(ExtensionError::Type::kRuntimeError);
  }
  std::set<int> error_ids;
  if (properties.error_ids) {
    error_ids.insert(properties.error_ids->begin(),
                     properties.error_ids->end());
  }
  error_console->RemoveErrors(
      ErrorMap::Filter(properties.extension_id, type, error_ids, false));

  return RespondNow(NoArguments());
}

DeveloperPrivateAddHostPermissionFunction::
    DeveloperPrivateAddHostPermissionFunction() = default;
DeveloperPrivateAddHostPermissionFunction::
    ~DeveloperPrivateAddHostPermissionFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateAddHostPermissionFunction::Run() {
  std::optional<developer::AddHostPermission::Params> params =
      developer::AddHostPermission::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::optional<URLPattern> pattern =
      ParseRuntimePermissionsPattern(params->host);
  if (!pattern) {
    return RespondNow(Error(kInvalidHost));
  }

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  if (!PermissionsManager::Get(browser_context())
           ->CanAffectExtension(*extension)) {
    return RespondNow(Error(kCannotChangeHostPermissions));
  }

  URLPatternSet new_host_permissions({*pattern});
  PermissionsUpdater(browser_context())
      .GrantRuntimePermissions(
          *extension,
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        new_host_permissions.Clone(),
                        new_host_permissions.Clone()),
          base::BindOnce(&DeveloperPrivateAddHostPermissionFunction::
                             OnRuntimePermissionsGranted,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeveloperPrivateAddHostPermissionFunction::OnRuntimePermissionsGranted() {
  Respond(NoArguments());
}

DeveloperPrivateRemoveHostPermissionFunction::
    DeveloperPrivateRemoveHostPermissionFunction() = default;
DeveloperPrivateRemoveHostPermissionFunction::
    ~DeveloperPrivateRemoveHostPermissionFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateRemoveHostPermissionFunction::Run() {
  std::optional<developer::RemoveHostPermission::Params> params =
      developer::RemoveHostPermission::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::optional<URLPattern> pattern =
      ParseRuntimePermissionsPattern(params->host);
  if (!pattern) {
    return RespondNow(Error(kInvalidHost));
  }

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  if (!manager->CanAffectExtension(*extension)) {
    return RespondNow(Error(kCannotChangeHostPermissions));
  }

  URLPatternSet host_permissions_to_remove({*pattern});
  std::unique_ptr<const PermissionSet> permissions_to_remove =
      PermissionSet::CreateIntersection(
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        host_permissions_to_remove.Clone(),
                        host_permissions_to_remove.Clone()),
          *manager->GetRevokablePermissions(*extension),
          URLPatternSet::IntersectionBehavior::kDetailed);
  if (permissions_to_remove->IsEmpty()) {
    return RespondNow(Error("Cannot remove a host that hasn't been granted."));
  }

  PermissionsUpdater(browser_context())
      .RevokeRuntimePermissions(
          *extension, *permissions_to_remove,
          base::BindOnce(&DeveloperPrivateRemoveHostPermissionFunction::
                             OnRuntimePermissionsRevoked,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeveloperPrivateRemoveHostPermissionFunction::
    OnRuntimePermissionsRevoked() {
  Respond(NoArguments());
}

DeveloperPrivateGetUserSiteSettingsFunction::
    DeveloperPrivateGetUserSiteSettingsFunction() = default;
DeveloperPrivateGetUserSiteSettingsFunction::
    ~DeveloperPrivateGetUserSiteSettingsFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetUserSiteSettingsFunction::Run() {
  developer::UserSiteSettings user_site_settings =
      DeveloperPrivateEventRouter::ConvertToUserSiteSettings(
          PermissionsManager::Get(browser_context())
              ->GetUserPermissionsSettings());

  return RespondNow(WithArguments(user_site_settings.ToValue()));
}

DeveloperPrivateAddUserSpecifiedSitesFunction::
    DeveloperPrivateAddUserSpecifiedSitesFunction() = default;
DeveloperPrivateAddUserSpecifiedSitesFunction::
    ~DeveloperPrivateAddUserSpecifiedSitesFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateAddUserSpecifiedSitesFunction::Run() {
  std::optional<developer::AddUserSpecifiedSites::Params> params =
      developer::AddUserSpecifiedSites::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::set<url::Origin> origins;
  for (const auto& host : params->options.hosts) {
    GURL url(host);
    if (!url.is_valid()) {
      return RespondNow(Error("Invalid host: " + host));
    }
    origins.insert(url::Origin::Create(url));
  }

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  switch (params->options.site_set) {
    case developer::SiteSet::kUserPermitted:
      for (const auto& origin : origins) {
        manager->AddUserPermittedSite(origin);
      }
      break;
    case developer::SiteSet::kUserRestricted:
      for (const auto& origin : origins) {
        manager->AddUserRestrictedSite(origin);
      }
      break;
    case developer::SiteSet::kExtensionSpecified:
      return RespondNow(
          Error("Site set must be USER_PERMITTED or USER_RESTRICTED"));
    case developer::SiteSet::kNone:
      NOTREACHED();
  }

  return RespondNow(NoArguments());
}

DeveloperPrivateRemoveUserSpecifiedSitesFunction::
    DeveloperPrivateRemoveUserSpecifiedSitesFunction() = default;
DeveloperPrivateRemoveUserSpecifiedSitesFunction::
    ~DeveloperPrivateRemoveUserSpecifiedSitesFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateRemoveUserSpecifiedSitesFunction::Run() {
  std::optional<developer::RemoveUserSpecifiedSites::Params> params =
      developer::RemoveUserSpecifiedSites::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::set<url::Origin> origins;
  for (const auto& host : params->options.hosts) {
    GURL url(host);
    if (!url.is_valid()) {
      return RespondNow(Error("Invalid host: " + host));
    }
    origins.insert(url::Origin::Create(url));
  }

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  switch (params->options.site_set) {
    case developer::SiteSet::kUserPermitted:
      for (const auto& origin : origins) {
        manager->RemoveUserPermittedSite(origin);
      }
      break;
    case developer::SiteSet::kUserRestricted:
      for (const auto& origin : origins) {
        manager->RemoveUserRestrictedSite(origin);
      }
      break;
    case developer::SiteSet::kExtensionSpecified:
      return RespondNow(
          Error("Site set must be USER_PERMITTED or USER_RESTRICTED"));
    case developer::SiteSet::kNone:
      NOTREACHED();
  }

  return RespondNow(NoArguments());
}

DeveloperPrivateGetUserAndExtensionSitesByEtldFunction::
    DeveloperPrivateGetUserAndExtensionSitesByEtldFunction() = default;
DeveloperPrivateGetUserAndExtensionSitesByEtldFunction::
    ~DeveloperPrivateGetUserAndExtensionSitesByEtldFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetUserAndExtensionSitesByEtldFunction::Run() {
  std::map<std::string, developer::SiteGroup> site_groups;
  const PermissionsManager::UserPermissionsSettings& settings =
      PermissionsManager::Get(browser_context())->GetUserPermissionsSettings();
  for (const url::Origin& site : settings.permitted_sites) {
    AddSiteToSiteGroups(&site_groups, site.host(),
                        GetETldPlusOne(site.GetURL()),
                        developer::SiteSet::kUserPermitted);
  }

  for (const url::Origin& site : settings.restricted_sites) {
    AddSiteToSiteGroups(&site_groups, site.host(),
                        GetETldPlusOne(site.GetURL()),
                        developer::SiteSet::kUserRestricted);
  }

  std::vector<scoped_refptr<const Extension>> extensions_to_check;
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser_context());

  // Note: we are only counting enabled extensions as the returned extension
  // counts will reflect how many extensions can actually run on each site at
  // the current moment.
  for (const auto& extension : registry->enabled_extensions()) {
    if (!ui_util::ShouldDisplayInExtensionSettings(*extension)) {
      continue;
    }

    std::unique_ptr<const PermissionSet> granted_permissions =
        permissions_manager->GetExtensionGrantedPermissions(*extension);
    std::vector<URLPattern> distinct_hosts =
        ExtensionInfoGenerator::GetDistinctHosts(
            granted_permissions->effective_hosts());

    ProcessSitesForRuntimeHostPermissions(&site_groups, distinct_hosts);
    extensions_to_check.push_back(extension);
  }

  // Maps an eTLD+1 to the number of extensions that can run on all subdomains
  // of that eTLD+1.
  std::map<std::string, size_t> match_subdomains_count;

  // Iterate over `site_groups` again and count the number of extensions that
  // can run on each site. This is in a separate loop as `site_groups` needs to
  // be fully populated before these checks can be made, so the num_extensions
  // counts are accurate.
  for (const auto& extension : extensions_to_check) {
    std::unique_ptr<const PermissionSet> granted_permissions =
        permissions_manager->GetExtensionGrantedPermissions(*extension);
    UpdateSiteGroupCountsForExtensionHosts(
        &site_groups, &match_subdomains_count,
        granted_permissions->effective_hosts());
  }

  std::vector<developer::SiteGroup> site_group_list;
  site_group_list.reserve(site_groups.size());
  for (auto& entry : site_groups) {
    // Sort the sites in each SiteGroup in ascending order by site.
    std::sort(entry.second.sites.begin(), entry.second.sites.end(),
              [](const developer::SiteInfo& a, const developer::SiteInfo& b) {
                return b.site > a.site;
              });

    size_t subdomains_count_for_site = match_subdomains_count[entry.first];
    if (subdomains_count_for_site > 0u) {
      // Append the all subdomains info to the end of the list.
      developer::SiteInfo all_subdomains_info = CreateSiteInfo(
          base::StrCat({"*.", entry.first}),
          developer::SiteSet::kExtensionSpecified, subdomains_count_for_site);

      entry.second.sites.push_back(std::move(all_subdomains_info));
    }
    site_group_list.push_back(std::move(entry.second));
  }

  return RespondNow(
      ArgumentList(developer::GetUserAndExtensionSitesByEtld::Results::Create(
          site_group_list)));
}

DeveloperPrivateGetMatchingExtensionsForSiteFunction::
    DeveloperPrivateGetMatchingExtensionsForSiteFunction() = default;
DeveloperPrivateGetMatchingExtensionsForSiteFunction::
    ~DeveloperPrivateGetMatchingExtensionsForSiteFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetMatchingExtensionsForSiteFunction::Run() {
  std::optional<developer::GetMatchingExtensionsForSite::Params> params =
      developer::GetMatchingExtensionsForSite::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  URLPattern parsed_site(Extension::kValidHostPermissionSchemes);
  if (parsed_site.Parse(params->site) != URLPattern::ParseResult::kSuccess) {
    return RespondNow(Error("Invalid site: " + params->site));
  }

  constexpr bool kIncludeApiPermissions = false;

  std::vector<developer::MatchingExtensionInfo> matching_extensions;
  URLPatternSet site_pattern({parsed_site});
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context())->enabled_extensions();
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser_context());
  for (const auto& extension : enabled_extensions) {
    std::unique_ptr<const PermissionSet> granted_permissions =
        permissions_manager->GetExtensionGrantedPermissions(*extension);
    const URLPatternSet& extension_withheld_sites =
        extension->permissions_data()->withheld_permissions().effective_hosts();
    const URLPatternSet granted_intersection =
        URLPatternSet::CreateIntersection(
            site_pattern, granted_permissions->effective_hosts(),
            URLPatternSet::IntersectionBehavior::kDetailed);
    const URLPatternSet withheld_intersection =
        URLPatternSet::CreateIntersection(
            site_pattern, extension_withheld_sites,
            URLPatternSet::IntersectionBehavior::kDetailed);

    if (granted_intersection.is_empty() && withheld_intersection.is_empty()) {
      continue;
    }

    // By default, return ON_CLICK if the extension has requested but does not
    // have access to any sites that match `site_pattern`.
    developer::HostAccess host_access = developer::HostAccess::kOnClick;

    // TODO(crbug.com/40278776): Add a version of CanUserSelectSiteAccess to
    // PermissionsManager which takes in a URLPattern.
    bool can_request_all_sites =
        granted_permissions->ShouldWarnAllHosts(kIncludeApiPermissions) ||
        extension->permissions_data()
            ->withheld_permissions()
            .ShouldWarnAllHosts(kIncludeApiPermissions);

    // If the extension has access to at least one site that matches
    // `site_pattern`, return ON_ALL_SITES if the extension can request all
    // sites and has no withheld sites, or ON_SPECIFIC_SITES otherwise.
    if (!granted_intersection.is_empty()) {
      host_access = can_request_all_sites && extension_withheld_sites.is_empty()
                        ? developer::HostAccess::kOnAllSites
                        : developer::HostAccess::kOnSpecificSites;
    }

    developer::MatchingExtensionInfo matching_info;
    matching_info.can_request_all_sites = can_request_all_sites;
    matching_info.site_access = host_access;
    matching_info.id = extension->id();
    matching_extensions.push_back(std::move(matching_info));
  }

  return RespondNow(
      ArgumentList(developer::GetMatchingExtensionsForSite::Results::Create(
          matching_extensions)));
}

DeveloperPrivateUpdateSiteAccessFunction::
    DeveloperPrivateUpdateSiteAccessFunction() = default;
DeveloperPrivateUpdateSiteAccessFunction::
    ~DeveloperPrivateUpdateSiteAccessFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateSiteAccessFunction::Run() {
  std::optional<developer::UpdateSiteAccess::Params> params =
      developer::UpdateSiteAccess::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  URLPattern parsed_site(Extension::kValidHostPermissionSchemes);
  if (parsed_site.Parse(params->site) != URLPattern::ParseResult::kSuccess) {
    return RespondNow(Error("Invalid site: " + params->site));
  }

  base::RepeatingClosure done_callback = base::BarrierClosure(
      params->updates.size(),
      base::BindOnce(
          &DeveloperPrivateUpdateSiteAccessFunction::OnSiteSettingsUpdated,
          base::RetainedRef(this)));

  // To ensure that this function is atomic, return with an error if any
  // extension specified does not exist or cannot have its host permissions
  // changed.
  PermissionsManager* const permissions_manager =
      PermissionsManager::Get(browser_context());
  std::vector<std::pair<const Extension&, developer::HostAccess>>
      extensions_to_modify;
  extensions_to_modify.reserve(params->updates.size());
  for (const auto& update : params->updates) {
    const Extension* extension = GetExtensionById(update.id);
    if (!extension) {
      return RespondNow(Error(kNoSuchExtensionError));
    }
    if (!permissions_manager->CanAffectExtension(*extension)) {
      return RespondNow(Error(kCannotChangeHostPermissions));
    }

    extensions_to_modify.emplace_back(*extension, update.site_access);
  }

  for (const auto& update : extensions_to_modify) {
    const Extension& extension = update.first;

    std::unique_ptr<const PermissionSet> permissions;
    ScriptingPermissionsModifier modifier(browser_context(), &extension);
    bool has_withheld_permissions =
        permissions_manager->HasWithheldHostPermissions(extension);
    switch (update.second) {
      case developer::HostAccess::kOnClick:
        // If the extension has no withheld permissions and can run on all of
        // its requested hosts, withhold all of its host permissions as a
        // blocklist based model for runtime host permissions (i.e. run on all
        // sites except these) is not currently supported.
        if (!has_withheld_permissions) {
          modifier.SetWithholdHostPermissions(true);
          modifier.RemoveAllGrantedHostPermissions();
          done_callback.Run();
        } else {
          modifier.RemoveHostPermissions(parsed_site, done_callback);
        }
        break;
      case developer::HostAccess::kOnSpecificSites:
        // If the extension has no withheld host permissions and can run on
        // all of its requested hosts, withhold all of its permissions
        // before granting `site`.
        if (!has_withheld_permissions) {
          modifier.SetWithholdHostPermissions(true);
          modifier.RemoveAllGrantedHostPermissions();
        }
        modifier.GrantHostPermission(parsed_site, done_callback);
        break;
      case developer::HostAccess::kOnAllSites:
        modifier.SetWithholdHostPermissions(false);
        done_callback.Run();
        break;
      case developer::HostAccess::kNone:
        NOTREACHED();
    }
  }

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeveloperPrivateUpdateSiteAccessFunction::OnSiteSettingsUpdated() {
  Respond(NoArguments());
}

DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction::
    DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction() =
        default;
DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction::
    ~DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction() =
        default;

ExtensionFunction::ResponseAction
DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction::Run() {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error(kCouldNotFindWebContentsError));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  SafetyHubMenuNotificationServiceFactory::GetForProfile(profile)
      ->DismissActiveNotificationOfModule(
          safety_hub::SafetyHubModuleType::EXTENSIONS);
  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace extensions
