// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_WEB_APP_LINK_CAPTURING_DELEGATE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_WEB_APP_LINK_CAPTURING_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"

class GURL;
class Profile;

namespace content {
class WebContents;
class NavigationHandle;
}  // namespace content

namespace web_app {

class WebAppLinkCapturingDelegate
    : public apps::LinkCapturingNavigationThrottle::Delegate {
 public:
  WebAppLinkCapturingDelegate();
  ~WebAppLinkCapturingDelegate() override;

  // apps::LinkCapturingNavigationThrottle::Delegate:
  bool ShouldCancelThrottleCreation(content::NavigationHandle* handle) override;
  std::optional<apps::LinkCapturingNavigationThrottle::LaunchCallback>
  CreateLinkCaptureLaunchClosure(Profile* profile,
                                 content::WebContents* web_contents,
                                 const GURL& url,
                                 bool is_navigation_from_link) final;

  base::WeakPtrFactory<WebAppLinkCapturingDelegate> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_WEB_APP_LINK_CAPTURING_DELEGATE_H_
