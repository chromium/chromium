// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/site_permissions_helper.h"

#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

namespace extensions {

SitePermissionsHelper::SitePermissionsHelper(Profile* profile)
    : profile_(profile) {}

SitePermissionsHelper::~SitePermissionsHelper() = default;

SitePermissionsHelper::SiteAccess SitePermissionsHelper::GetCurrentSiteAccess(
    const Extension& extension,
    content::WebContents* web_contents) const {
  DCHECK(web_contents);
  ScriptingPermissionsModifier modifier(profile_,
                                        base::WrapRefCounted(&extension));
  DCHECK(modifier.CanAffectExtension());

  ScriptingPermissionsModifier::SiteAccess site_access =
      modifier.GetSiteAccess(web_contents->GetLastCommittedURL());
  if (site_access.has_all_sites_access)
    return SiteAccess::kOnAllSites;
  if (site_access.has_site_access)
    return SiteAccess::kOnSite;
  return SiteAccess::kOnClick;
}

void SitePermissionsHelper::UpdateSiteAccess(
    const Extension& extension,
    content::WebContents* web_contents,
    SitePermissionsHelper::SiteAccess new_access) {
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (!runner)
    return;

  auto current_access = GetCurrentSiteAccess(extension, web_contents);
  if (new_access == current_access)
    return;

  runner->HandlePageAccessModified(&extension, current_access, new_access);
}

}  // namespace extensions
