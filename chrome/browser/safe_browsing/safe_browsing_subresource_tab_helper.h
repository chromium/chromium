// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SUBRESOURCE_TAB_HELPER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SUBRESOURCE_TAB_HELPER_H_

#include "base/supports_user_data.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace safe_browsing {

class SafeBrowsingUIManager;

class SafeBrowsingSubresourceTabHelper : public content::WebContentsObserver,
                                         public base::SupportsUserData::Data {
 public:
  SafeBrowsingSubresourceTabHelper(content::WebContents* web_contents,
                                   SafeBrowsingUIManager* manager);
  ~SafeBrowsingSubresourceTabHelper() override;

  static void CreateForWebContents(content::WebContents* web_contents,
                                   SafeBrowsingUIManager* manager);

  static SafeBrowsingSubresourceTabHelper* FromWebContents(
      content::WebContents* web_contents);

  // WebContentsObserver::
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  static const char kSafeBrowsingSubresourceTabHelperWebContentsUserDataKey[];

  SafeBrowsingUIManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingSubresourceTabHelper);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SUBRESOURCE_TAB_HELPER_H_
