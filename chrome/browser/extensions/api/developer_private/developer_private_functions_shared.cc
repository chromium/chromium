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

}  // namespace api
}  // namespace extensions
