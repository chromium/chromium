// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_REQUIRED_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_REQUIRED_NAVIGATION_THROTTLE_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class BrowserContext;
class NavigationHandle;
class WebContents;
}  // namespace content

// This navigation throttle will show an interstitial on a page where an
// an enterprise signin interception where a managed profile is required by
// policy occurs.
// After a call to `BlockNavigationUntilEnterpriseActionTaken`, a `BlockingInfo`
// user data is set on the browser context. All navigations happening on
// WebContents from the specified BrowserContext are blocked, unless that
// WebContent is specifically allowed in the `BlockingInfo`.
class ManagedProfileRequiredNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Create a navigation throttle for the given navigation if third-party
  // profile management is enabled. Returns nullptr if no throttling should be
  // done.
  static std::unique_ptr<ManagedProfileRequiredNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* navigation_handle);

  explicit ManagedProfileRequiredNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  ManagedProfileRequiredNavigationThrottle(
      const ManagedProfileRequiredNavigationThrottle&) = delete;
  ManagedProfileRequiredNavigationThrottle& operator=(
      const ManagedProfileRequiredNavigationThrottle&) = delete;
  ~ManagedProfileRequiredNavigationThrottle() override;

  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  ThrottleCheckResult WillFailRequest() override;
  const char* GetNameForLogging() override;

  static bool IsBlockingNavigations(content::BrowserContext* browser_context);

  // Blocks all navigations in the specified `browser_context` except for
  // `allowed_web_contents` if set. This returns a callback that re-allows
  // navigations. The callback must be run when appropriate to re-enable
  // navigation on the `browser_context`.
  [[nodiscard]] static base::ScopedClosureRunner
  BlockNavigationUntilEnterpriseActionTaken(
      content::BrowserContext* browser_context,
      content::WebContents* enterprise_action_web_contents,
      content::WebContents* allowed_web_contents = nullptr);

  static void ShowBlockedWindow(content::BrowserContext* browser_context);
  static void SetReloadRequired(
      content::BrowserContext* browser_context,
      bool success,
      base::OnceCallback<void(content::NavigationHandle&)> on_reload_triggered =
          base::OnceCallback<void(content::NavigationHandle&)>());

 private:
  ThrottleCheckResult ProcessThrottleEvent();
  base::WeakPtrFactory<ManagedProfileRequiredNavigationThrottle>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_REQUIRED_NAVIGATION_THROTTLE_H_
