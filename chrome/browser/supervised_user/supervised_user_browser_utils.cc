// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include <string>

#include "base/strings/string_util.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_urls.h"
#include "url/url_constants.h"

namespace supervised_user {

bool IsSupportedChromeExtensionURL(const GURL& effective_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  static const char* const kCrxDownloadUrls[] = {
      "https://clients2.googleusercontent.com/crx/blobs/",
      "https://chrome.google.com/webstore/download/"};

  // Chrome Webstore.
  if (extension_urls::IsWebstoreDomain(
          url_matcher::util::Normalize(effective_url))) {
    return true;
  }

  // Allow webstore crx downloads. This applies to both extension installation
  // and updates.
  if (extension_urls::GetWebstoreUpdateUrl() ==
      url_matcher::util::Normalize(effective_url)) {
    return true;
  }

  // The actual CRX files are downloaded from other URLs. Allow them too.
  // These URLs have https scheme.
  if (!effective_url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  for (const char* crx_download_url_str : kCrxDownloadUrls) {
    GURL crx_download_url(crx_download_url_str);
    if (crx_download_url.host_piece() == effective_url.host_piece() &&
        base::StartsWith(effective_url.path_piece(),
                         crx_download_url.path_piece(),
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

bool ShouldContentSkipParentAllowlistFiltering(content::WebContents* contents) {
  // Note that |contents| can be an inner WebContents. Get the outer most
  // WebContents and check if it belongs to the EDUCoexistence login flow.
  content::WebContents* outer_most_content =
      contents->GetOutermostWebContents();

  return outer_most_content->GetLastCommittedURL() ==
         GURL(chrome::kChromeUIEDUCoexistenceLoginURLV2);
}

void CleanUpInfoBarForContent(content::WebContents* web_contents) {
  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  if (manager) {
    content::LoadCommittedDetails details;
    // |details.is_same_document| is default false, and |details.is_main_frame|
    // is default true. This results in is_navigation_to_different_page()
    // returning true.
    DCHECK(details.is_navigation_to_different_page());
    content::NavigationController& controller = web_contents->GetController();
    details.entry = controller.GetVisibleEntry();
    if (controller.GetLastCommittedEntry()) {
      details.previous_entry_index = controller.GetLastCommittedEntryIndex();
      details.previous_main_frame_url =
          controller.GetLastCommittedEntry()->GetURL();
    }
    for (int i = manager->infobar_count() - 1; i >= 0; --i) {
      infobars::InfoBar* infobar = manager->infobar_at(i);

      if (infobar->delegate()->ShouldExpire(
              infobars::ContentInfoBarManager::
                  NavigationDetailsFromLoadCommittedDetails(details))) {
        manager->RemoveInfoBar(infobar);
      }
    }
  }
}

}  // namespace supervised_user
