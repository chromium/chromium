// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APPS_INTENT_HELPER_COMMON_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ASH_APPS_INTENT_HELPER_COMMON_APPS_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace apps {

// Allows navigation to be routed to an installed app, and provides
// a static method for showing an intent picker for the current URL to display
// any handling apps. This is a common throttle that work with the App Service.
// This only works with Chrome OS at the moment and will work with all platforms
// after the App Service supports apps for all platforms.
// TODO(crbug.com/853604): Add metrics, add ARC auto pop up, add persistency.
class CommonAppsNavigationThrottle : public apps::AppsNavigationThrottle {
 public:
  // Possibly creates a navigation throttle that checks if any installed apps
  // can handle the URL being navigated to. The user is prompted if they wish to
  // open the app or remain in the browser.
  static std::unique_ptr<apps::AppsNavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  explicit CommonAppsNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  ~CommonAppsNavigationThrottle() override;

 private:
  bool ShouldCancelNavigation(content::NavigationHandle* handle) override;
  bool ShouldShowDisablePage(content::NavigationHandle* handle) override;
  ThrottleCheckResult MaybeShowCustomResult() override;

  DISALLOW_COPY_AND_ASSIGN(CommonAppsNavigationThrottle);
};

}  // namespace apps

#endif  // CHROME_BROWSER_ASH_APPS_INTENT_HELPER_COMMON_APPS_NAVIGATION_THROTTLE_H_
