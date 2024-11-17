// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PERMISSIONS_SITE_PERMISSIONS_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_PERMISSIONS_SITE_PERMISSIONS_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "extensions/browser/permissions_manager.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;

// A helper class responsible for providing the permissions data to models used
// in the Extensions toolbar (e.g: ExtensionContextMenuModel).
class SitePermissionsHelper {
 public:
  // The interaction of the extension with the site. This is independent
  // of the action's clickability.
  // TODO(crbug.com/40817514): Move enum and related methods to
  // PermissionsManager.
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

  // Returns the site interaction for `extension` in the current site pointed by
  // `web_contents`.
  SiteInteraction GetSiteInteraction(const Extension& extension,
                                     content::WebContents* web_contents) const;

  // Updates the site access pointed to by `web_contents` to `new_access` for
  // `extension` or `extensions`. If relevant, this will run any pending
  // extension actions on that site and/or show a reload dialog for new site
  // access to take effect.
  void UpdateSiteAccess(const Extension& extension,
                        content::WebContents* web_contents,
                        PermissionsManager::UserSiteAccess new_access);
  void UpdateSiteAccess(const std::vector<const Extension*>& extensions,
                        content::WebContents* web_contents,
                        PermissionsManager::UserSiteAccess new_access);

  // Returns whether the `extension` has been blocked on the given
  // `web_contents`.
  bool HasBeenBlocked(const Extension& extension,
                      content::WebContents* web_contents) const;

  // Returns whether the `blocked_actions` need a page refresh to run.
  bool PageNeedsRefreshToRun(int blocked_actions);

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

#endif  // CHROME_BROWSER_EXTENSIONS_PERMISSIONS_SITE_PERMISSIONS_HELPER_H_
