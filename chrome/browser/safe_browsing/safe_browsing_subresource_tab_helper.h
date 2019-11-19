// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SUBRESOURCE_TAB_HELPER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SUBRESOURCE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace safe_browsing {

class SafeBrowsingSubresourceTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SafeBrowsingSubresourceTabHelper> {
 public:
  ~SafeBrowsingSubresourceTabHelper() override;

  // WebContentsObserver::
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit SafeBrowsingSubresourceTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SafeBrowsingSubresourceTabHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingSubresourceTabHelper);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SUBRESOURCE_TAB_HELPER_H_
