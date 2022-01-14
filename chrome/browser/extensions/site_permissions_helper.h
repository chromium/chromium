// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SITE_PERMISSIONS_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_SITE_PERMISSIONS_HELPER_H_

#include "base/memory/raw_ptr.h"

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
  enum class SiteAccess {
    kOnClick,
    kOnSite,
    kOnAllSites,
  };

  explicit SitePermissionsHelper(Profile* profile);
  SitePermissionsHelper(const SitePermissionsHelper&) = delete;
  const SitePermissionsHelper& operator=(const SitePermissionsHelper&) = delete;
  ~SitePermissionsHelper();

  // Returns the current site access pointed by `web_contents` for `extension`.
  SiteAccess GetCurrentSiteAccess(const Extension& extension,
                                  content::WebContents* web_contents) const;

  // Updates the site access pointed to by `web_contents` to `new_access` for
  // `extension`. If relevant, this will run any pending extension actions on
  // that site.
  void UpdateSiteAccess(const Extension& extension,
                        content::WebContents* web_contents,
                        SitePermissionsHelper::SiteAccess new_access);

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SITE_PERMISSIONS_HELPER_H_
