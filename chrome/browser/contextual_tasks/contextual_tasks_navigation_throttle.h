// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

// A navigation throttle used for the "Contextual Tasks" feature. This throttle
// delegates the decision of whether we intercept out to the
// `ContextualTasksUiService`. If this throttle is created, the action is always
// to cancel the navigation.
class ContextualTasksNavigationThrottle : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  explicit ContextualTasksNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ContextualTasksNavigationThrottle(const ContextualTasksNavigationThrottle&) =
      delete;
  ContextualTasksNavigationThrottle& operator=(
      const ContextualTasksNavigationThrottle&) = delete;
  ~ContextualTasksNavigationThrottle() override;

  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

  // Adds the navigation throttle if the URL being navigated to and its host
  // WebContents meet specific criteria. See `ContextualTasksUiService` for more
  // details.
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

 private:
  // A helper to handle both normal navigation and reidrects.
  ThrottleCheckResult ProcessNavigation();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_NAVIGATION_THROTTLE_H_
