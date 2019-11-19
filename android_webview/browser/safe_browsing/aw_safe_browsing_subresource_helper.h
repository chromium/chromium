// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_SUBRESOURCE_HELPER_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_SUBRESOURCE_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace android_webview {

// This observer creates a blocking page for the web contents if any subresource
// triggered a safe browsing interstitial. Main frame safe browsing errors are
// handled separately (in AwSafeBrowsingNavigationThrottle).
class AwSafeBrowsingSubresourceHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AwSafeBrowsingSubresourceHelper> {
 public:
  ~AwSafeBrowsingSubresourceHelper() override;

  // WebContentsObserver::
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit AwSafeBrowsingSubresourceHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AwSafeBrowsingSubresourceHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
  DISALLOW_COPY_AND_ASSIGN(AwSafeBrowsingSubresourceHelper);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_SUBRESOURCE_HELPER_H_
