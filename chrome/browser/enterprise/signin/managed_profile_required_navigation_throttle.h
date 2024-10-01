// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_REQUIRED_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_REQUIRED_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

class DiceWebSigninInterceptor;

namespace content {
class NavigationHandle;
}  // namespace content

// This navigation throttle will show an interstitial on a page where an
// an enterprise signin interception where a managed profile is required by
// policy occurs.
// The navigation is canceled and an interstitial is shown when the WebContents
// of the navigation is the same as the one of DiceWebSigninInterceptor and the
// current interception time is an forced enterprise interception.
class ManagedProfileRequiredNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Create a navigation throttle for the given navigation if third-party
  // profile management is enabled. Returns nullptr if no throttling should be
  // done.
  static std::unique_ptr<ManagedProfileRequiredNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* navigation_handle);

  ManagedProfileRequiredNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      const std::u16string& profile_management_domain,
      DiceWebSigninInterceptor* signin_interceptor);

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
  void SetManagerForTesting(const std::u16string& manager_for_testing) {
    profile_management_domain_ = manager_for_testing;
  }

 private:
  ThrottleCheckResult ProcessThrottleEvent();
  std::u16string profile_management_domain_;
  raw_ptr<DiceWebSigninInterceptor> signin_interceptor_;
  base::WeakPtrFactory<ManagedProfileRequiredNavigationThrottle>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_MANAGED_PROFILE_REQUIRED_NAVIGATION_THROTTLE_H_
