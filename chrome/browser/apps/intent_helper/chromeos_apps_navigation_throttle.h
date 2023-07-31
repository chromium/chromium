// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_APPS_NAVIGATION_THROTTLE_H_

#include <memory>
#include <vector>

#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace base {
class TickClock;
}

namespace content {
class NavigationHandle;
}  // namespace content

namespace apps {

// Allows navigation to be routed to an installed app. This throttle supports
// all type of apps in the Chrome OS platform using App Service.
class ChromeOsAppsNavigationThrottle : public apps::AppsNavigationThrottle {
 public:
  // Possibly creates a navigation throttle that checks if any installed apps
  // can handle the URL being navigated to.
  static std::unique_ptr<apps::AppsNavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  // Method intended for testing purposes only.
  // Set clock used for timing to enable manipulation during tests.
  static void SetClockForTesting(const base::TickClock* tick_clock);

  static base::OnceClosure& GetLinkCaptureLaunchCallbackForTesting();

  explicit ChromeOsAppsNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  ChromeOsAppsNavigationThrottle(const ChromeOsAppsNavigationThrottle&) =
      delete;
  ChromeOsAppsNavigationThrottle& operator=(
      const ChromeOsAppsNavigationThrottle&) = delete;

  ~ChromeOsAppsNavigationThrottle() override;

 private:
  bool ShouldCancelNavigation(content::NavigationHandle* handle) override;
  bool ShouldShowDisablePage(content::NavigationHandle* handle) override;
  ThrottleCheckResult MaybeShowCustomResult() override;

  // Used to create a unique timestamped URL to force reload apps.
  // Points to the base::DefaultTickClock by default.
  static const base::TickClock* clock_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_APPS_NAVIGATION_THROTTLE_H_
