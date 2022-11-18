// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/site_permissions_helper.h"

#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// A preference indicating if the extension can show site access requests
// directly in the toolbar next to the omnibox.
constexpr const char kPrefShowAccessRequestsInToolbar[] =
    "show_access_requests_in_toolbar";

}  // namespace

SitePermissionsHelper::SitePermissionsHelper(Profile* profile)
    : profile_(profile) {}

SitePermissionsHelper::~SitePermissionsHelper() = default;

SitePermissionsHelper::SiteAccess SitePermissionsHelper::GetSiteAccess(
    const Extension& extension,
    const GURL& gurl) const {
  DCHECK(
      !extension.permissions_data()->IsRestrictedUrl(gurl, /*error=*/nullptr));

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile_);

  // Extension with no host permissions but with active tab permission has "on
  // click" access.
  if (!permissions_manager->CanAffectExtension(extension) &&
      HasActiveTabAndCanAccess(extension, gurl))
    return SiteAccess::kOnClick;

  DCHECK(permissions_manager->CanAffectExtension(extension));

  PermissionsManager::ExtensionSiteAccess site_access =
      permissions_manager->GetSiteAccess(extension, gurl);
  if (site_access.has_all_sites_access)
    return SiteAccess::kOnAllSites;
  if (site_access.has_site_access)
    return SiteAccess::kOnSite;
  return SiteAccess::kOnClick;
}

SitePermissionsHelper::SiteInteraction
SitePermissionsHelper::GetSiteInteraction(
    const Extension& extension,
    content::WebContents* web_contents) const {
  if (!web_contents)
    return SiteInteraction::kNone;

  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  const GURL& url = web_contents->GetLastCommittedURL();
  PermissionsData::PageAccess page_access =
      extension.permissions_data()->GetPageAccess(url, tab_id,
                                                  /*error=*/nullptr);
  PermissionsData::PageAccess script_access =
      extension.permissions_data()->GetContentScriptAccess(url, tab_id,
                                                           /*error=*/nullptr);

  if (page_access == PermissionsData::PageAccess::kAllowed ||
      script_access == PermissionsData::PageAccess::kAllowed) {
    return SiteInteraction::kGranted;
  }

  // An extension can request both host permissions and activeTab permission.
  // Withholding a host permission takes priority over activeTab permission,
  // because withheld hosts are hosts that the extension explicitly marked as
  // 'required' permissions, so it is a stronger signal that the extension
  // should run on the site. ActiveTab extensions, by contrast, are designed to
  // run when the user explicitly invokes them.
  // TODO(tjudkins): Investigate if we need to check HasBeenBlocked() for this
  // case. We do know that extensions that have been blocked should always be
  // marked pending, but those cases should be covered by the withheld page
  // access checks.
  if (page_access == PermissionsData::PageAccess::kWithheld ||
      script_access == PermissionsData::PageAccess::kWithheld ||
      HasBeenBlocked(extension, web_contents)) {
    return SiteInteraction::kWithheld;
  }

  if (HasActiveTabAndCanAccess(extension, url)) {
    return SiteInteraction::kActiveTab;
  }

  return SiteInteraction::kNone;
}

void SitePermissionsHelper::UpdateSiteAccess(
    const Extension& extension,
    content::WebContents* web_contents,
    SitePermissionsHelper::SiteAccess new_access) {
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (!runner)
    return;

  auto current_access =
      GetSiteAccess(extension, web_contents->GetLastCommittedURL());
  if (new_access == current_access)
    return;

  runner->HandlePageAccessModified(&extension, current_access, new_access);
}

void SitePermissionsHelper::UpdateUserSiteSettings(
    const base::flat_set<ToolbarActionsModel::ActionId>& action_ids,
    content::WebContents* web_contents,
    extensions::PermissionsManager::UserSiteSetting site_setting) {
  DCHECK(web_contents);

  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (!runner)
    return;

  runner->HandleUserSiteSettingModified(
      action_ids, web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      site_setting);
}

bool SitePermissionsHelper::CanSelectSiteAccess(const Extension& extension,
                                                const GURL& url,
                                                SiteAccess site_access) const {
  // Extensions cannot run on sites restricted to them (ever), so no type of
  // site access is selectable.
  if (extension.permissions_data()->IsRestrictedUrl(url, /*error=*/nullptr))
    return false;

  // The "on click" option is enabled if the extension has active tab,
  // regardless of its granted host permissions.
  if (site_access == SitePermissionsHelper::SiteAccess::kOnClick &&
      HasActiveTabAndCanAccess(extension, url))
    return true;

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile_);
  if (!permissions_manager->CanAffectExtension(extension))
    return false;

  PermissionsManager::ExtensionSiteAccess extension_access =
      permissions_manager->GetSiteAccess(extension, url);
  switch (site_access) {
    case SitePermissionsHelper::SiteAccess::kOnClick:
      // The "on click" option is only enabled if the extension has active tab,
      // previously handled, or wants to always run on the site without user
      // interaction.
      return extension_access.has_site_access ||
             extension_access.withheld_site_access;
    case SitePermissionsHelper::SiteAccess::kOnSite:
      // The "on site" option is only enabled if the extension wants to
      // always run on the site without user interaction.
      return extension_access.has_site_access ||
             extension_access.withheld_site_access;
    case SitePermissionsHelper::SiteAccess::kOnAllSites:
      // The "on all sites" option is only enabled if the extension wants to be
      // able to run everywhere.
      return extension_access.has_all_sites_access ||
             extension_access.withheld_all_sites_access;
  }
}

bool SitePermissionsHelper::HasBeenBlocked(
    const Extension& extension,
    content::WebContents* web_contents) const {
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  return action_runner && action_runner->WantsToRun(&extension);
}

bool SitePermissionsHelper::HasActiveTabAndCanAccess(const Extension& extension,
                                                     const GURL& url) const {
  return extension.permissions_data()->HasAPIPermission(
             mojom::APIPermissionID::kActiveTab) &&
         !extension.permissions_data()->IsRestrictedUrl(url,
                                                        /*error=*/nullptr) &&
         (!url.SchemeIsFile() ||
          util::AllowFileAccess(extension.id(), profile_));
}

bool SitePermissionsHelper::ShowAccessRequestsInToolbar(
    const std::string& extension_id) {
  // By default, extensions requesting access should be visible in toolbar,
  // otherwise the user would most likely never grant the extensions access.
  bool show_access_requests = true;

  ExtensionPrefs::Get(profile_)->ReadPrefAsBoolean(
      extension_id, kPrefShowAccessRequestsInToolbar, &show_access_requests);
  return show_access_requests;
}

void SitePermissionsHelper::SetShowAccessRequestsInToolbar(
    const std::string& extension_id,
    bool show_access_requests_in_toolbar) {
  ExtensionPrefs::Get(profile_)->UpdateExtensionPref(
      extension_id, kPrefShowAccessRequestsInToolbar,
      std::make_unique<base::Value>(show_access_requests_in_toolbar));
}

}  // namespace extensions
