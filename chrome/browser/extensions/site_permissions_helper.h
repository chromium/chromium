// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SITE_PERMISSIONS_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_SITE_PERMISSIONS_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "extensions/browser/permissions_manager.h"

class Profile;
class GURL;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;

// A helper class responsible for providing the permissions data to models used
// in the Extensions toolbar (e.g: ExtensionContextMenuModel).
class SitePermissionsHelper {
 public:
  enum class SiteAccess {
    kOnClick,
    kOnSite,
    kOnAllSites,
  };

  // The interaction of the extension with the site. This is independent
  // of the action's clickability.
  enum class SiteInteraction {
    // The extension cannot run on the site.
    kNone,
    // The extension has withheld site access by the user.
    kWithheld,
    // The extension has activeTab permission to run on the site, but is pending
    // user action to run.
    kActiveTab,
    // The extension has permission to run on the site.
    kGranted,
  };

  explicit SitePermissionsHelper(Profile* profile);
  SitePermissionsHelper(const SitePermissionsHelper&) = delete;
  const SitePermissionsHelper& operator=(const SitePermissionsHelper&) = delete;
  ~SitePermissionsHelper();

  // Returns the site access for `extension` in `gurl`. This can only be called
  // if the url is not restricted, and if the user can configure site access for
  // the extension (which excludes things like policy extensions) or if the
  // extension has active tab permission.
  SiteAccess GetSiteAccess(const Extension& extension, const GURL& gurl) const;

  // Returns the site interaction for `extension` in the current site pointed by
  // `web_contents`.
  SiteInteraction GetSiteInteraction(const Extension& extension,
                                     content::WebContents* web_contents) const;

  // Updates the site access pointed to by `web_contents` to `new_access` for
  // `extension`. If relevant, this will run any pending extension actions on
  // that site.
  void UpdateSiteAccess(const Extension& extension,
                        content::WebContents* web_contents,
                        SitePermissionsHelper::SiteAccess new_access);

  // Updates the user site settings pointed to by `web_contents` to
  // `site_setting` for `action_ids`.
  void UpdateUserSiteSettings(
      const base::flat_set<ToolbarActionsModel::ActionId>& action_ids,
      content::WebContents* web_contents,
      PermissionsManager::UserSiteSetting site_setting);

  // Returns whether `site_access` option can be selected for `extension` in
  // `url`.
  bool CanSelectSiteAccess(const Extension& extension,
                           const GURL& gurl,
                           SiteAccess site_access) const;

  // Returns whether the `extension` has been blocked on the given
  // `web_contents`.
  bool HasBeenBlocked(const Extension& extension,
                      content::WebContents* web_contents) const;

  // Returns true if this extension uses the activeTab permission and would
  // probably be able to to access the given `url`. The actual checks when an
  // activeTab extension tries to run are a little more complicated and can be
  // seen in ExtensionActionRunner and ActiveTabPermissionGranter.
  // Note: The rare cases where this gets it wrong should only be for false
  // positives, where it reports that the extension wants access but it can't
  // actually be given access when it tries to run.
  bool HasActiveTabAndCanAccess(const Extension& extension,
                                const GURL& url) const;

  // Returns true if `extension_id` can show site access requests in the
  // toolbar.
  bool ShowAccessRequestsInToolbar(const std::string& extension_id);

  // Sets whether `extenson_id` can show site access requests in the toolbar.
  void SetShowAccessRequestsInToolbar(const std::string& extension_id,
                                      bool show_access_requests_in_toolbar);

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SITE_PERMISSIONS_HELPER_H_
