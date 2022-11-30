// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace apps {

// Allows canceling a navigation to instead be routed to an installed app.
class AppsNavigationThrottle : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  // Possibly creates a navigation throttle that checks if any installed apps
  // can handle the URL being navigated to.
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
  virtual bool ShouldCancelNavigation(content::NavigationHandle* handle);

  virtual bool ShouldShowDisablePage(content::NavigationHandle* handle);

  virtual ThrottleCheckResult MaybeShowCustomResult();

  virtual bool ShouldOverrideUrlLoadingForOfficeExperiment(
      const GURL& previous_url,
      const GURL& current_url);

  bool navigate_from_link() const;

  GURL starting_url_;

 private:
  ThrottleCheckResult HandleRequest();

  // Keeps track of whether the navigation is coming from a link or not. If the
  // navigation is not from a link, we will not show the pop up for the intent
  // picker bubble.
  bool navigate_from_link_ = false;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_THROTTLE_H_
