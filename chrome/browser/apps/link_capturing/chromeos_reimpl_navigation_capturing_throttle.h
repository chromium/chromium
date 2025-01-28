// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_REIMPL_NAVIGATION_CAPTURING_THROTTLE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_REIMPL_NAVIGATION_CAPTURING_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace apps {

// Navigation throttle that will be used on ChromeOS to implement the parts of
// navigation capturing reimplementation that is not handled by the
// `NavigationCapturingProcess`.
class ChromeOsReimplNavigationCapturingThrottle
    : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  ChromeOsReimplNavigationCapturingThrottle(
      const ChromeOsReimplNavigationCapturingThrottle&) = delete;
  ChromeOsReimplNavigationCapturingThrottle& operator=(
      const ChromeOsReimplNavigationCapturingThrottle&) = delete;
  ~ChromeOsReimplNavigationCapturingThrottle() override;

  // content::NavigationHandle overrides:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;

 private:
  explicit ChromeOsReimplNavigationCapturingThrottle(
      content::NavigationHandle* navigation_handle);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_REIMPL_NAVIGATION_CAPTURING_THROTTLE_H_
