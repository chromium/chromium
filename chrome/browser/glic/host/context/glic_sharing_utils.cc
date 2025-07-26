// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace glic {

bool IsBrowserValidForSharingInProfile(
    BrowserWindowInterface* browser_interface,
    Profile* profile) {
  return browser_interface && profile &&
         browser_interface->GetProfile() == profile &&
         !profile->IsOffTheRecord();
}

bool IsTabValidForSharing(content::WebContents* web_contents) {
  // We allow allow blank pages to avoid flicker during transitions.
  static const base::NoDestructor<std::vector<GURL>> kUrlAllowList{
      {GURL(), GURL(url::kAboutBlankURL),
       GURL(chrome::kChromeUINewTabPageThirdPartyURL),
       GURL(chrome::kChromeUINewTabPageURL), GURL(chrome::kChromeUINewTabURL),
       GURL(chrome::kChromeUIWhatsNewURL)}};
  if (!web_contents) {
    return false;
  }
  const GURL& url = web_contents->GetLastCommittedURL();
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile() ||
         base::Contains(*kUrlAllowList, url);
}

}  // namespace glic
