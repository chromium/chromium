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

SitePermissionsHelper::SiteInteraction
SitePermissionsHelper::GetSiteInteraction(
    const Extension& extension,
    content::WebContents* web_contents) const {
  if (!web_contents) {
    return SiteInteraction::kNone;
  }

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

  if (PermissionsManager::Get(profile_)->HasActiveTabAndCanAccess(extension,
                                                                  url)) {
    return SiteInteraction::kActiveTab;
  }

  return SiteInteraction::kNone;
}

void SitePermissionsHelper::UpdateSiteAccess(
    const Extension& extension,
    content::WebContents* web_contents,
    PermissionsManager::UserSiteAccess new_access) {
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (!runner) {
    return;
  }

  auto current_access = PermissionsManager::Get(profile_)->GetUserSiteAccess(
      extension, web_contents->GetLastCommittedURL());
  if (new_access == current_access) {
    return;
  }

  runner->HandlePageAccessModified(&extension, current_access, new_access);
}

void SitePermissionsHelper::UpdateUserSiteSettings(
    const base::flat_set<ToolbarActionsModel::ActionId>& action_ids,
    content::WebContents* web_contents,
    extensions::PermissionsManager::UserSiteSetting site_setting) {
  DCHECK(web_contents);

  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (!runner) {
    return;
  }

  runner->HandleUserSiteSettingModified(
      action_ids, web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      site_setting);
}

bool SitePermissionsHelper::HasBeenBlocked(
    const Extension& extension,
    content::WebContents* web_contents) const {
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  return action_runner && action_runner->WantsToRun(&extension);
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
      base::Value(show_access_requests_in_toolbar));
  PermissionsManager::Get(profile_)->NotifyShowAccessRequestsInToolbarChanged(
      extension_id, show_access_requests_in_toolbar);
}

}  // namespace extensions
