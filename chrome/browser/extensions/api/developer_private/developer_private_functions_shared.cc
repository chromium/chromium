// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_functions_shared.h"

#include "chrome/browser/extensions/api/developer_private/developer_private_event_router.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/permissions/permissions_data.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace extensions {

namespace {
const char kCannotUpdateChildAccountProfileSettingsError[] =
    "Cannot change settings for a child account profile.";
}

namespace developer = api::developer_private;

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

}  // namespace api
}  // namespace extensions
