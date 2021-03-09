// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace apps {

// Allows navigation to be routed to an installed app on Chrome OS, and provides
// a static method for showing an intent picker for the current URL to display
// any handling apps.
class AppsNavigationThrottle : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  // Possibly creates a navigation throttle that checks if any installed apps
  // can handle the URL being navigated to. The user is prompted if they wish to
  // open the app or remain in the browser.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  explicit AppsNavigationThrottle(content::NavigationHandle* navigation_handle);
  AppsNavigationThrottle(const AppsNavigationThrottle&) = delete;
  AppsNavigationThrottle& operator=(const AppsNavigationThrottle&) = delete;
  ~AppsNavigationThrottle() override;

  // content::NavigationHandle overrides
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

 protected:
  // Overrides for Chrome OS to allow ARC handling.
  virtual void MaybeRemoveComingFromArcFlag(content::WebContents* web_contents,
                                            const GURL& previous_url,
                                            const GURL& current_url) {}

  virtual bool ShouldDeferNavigation(content::NavigationHandle* handle);

  virtual bool ShouldCancelNavigation(content::NavigationHandle* handle);

  virtual bool ShouldShowDisablePage(content::NavigationHandle* handle);

  virtual ThrottleCheckResult MaybeShowCustomResult();

  bool navigate_from_link() const;

  // Keeps track of whether we already shown the UI or preferred app. Since
  // AppsNavigationThrottle cannot wait for the user (due to the non-blocking
  // nature of the feature) the best we can do is check if we launched a
  // preferred app or asked the UI to be shown, this flag ensures we never
  // trigger the UI twice for the same throttle.
  // TODO(crbug.com/824598): This is no longer needed after removing
  // ChromeOsAppsNavigationThrottle.
  bool ui_displayed_ = false;

  GURL starting_url_;

 private:
  // Returns whether navigation to |url| was captured by a web app and what to
  // do next if so.
  // Note that this implementation is only for:
  //  - |kDesktopPWAsTabStripLinkCapturing|
  //  - |kWebAppEnableLinkCapturing| when |kIntentPickerPWAPersistence| is
  //    disabled.
  // When |kIntentPickerPWAPersistence| is enabled |kWebAppEnableLinkCapturing|
  // is handled by WebAppsBase::LaunchAppWithIntentImpl() instead and integrates
  // properly with App Service's intent handling system.
  base::Optional<ThrottleCheckResult> CaptureWebAppScopeNavigations(
      content::WebContents* web_contents,
      content::NavigationHandle* handle) const;

  ThrottleCheckResult HandleRequest();

  // Keeps track of whether the navigation is coming from a link or not. If the
  // navigation is not from a link, we will not show the pop up for the intent
  // picker bubble.
  bool navigate_from_link_ = false;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
