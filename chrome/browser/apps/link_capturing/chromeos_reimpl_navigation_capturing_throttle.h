// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_REIMPL_NAVIGATION_CAPTURING_THROTTLE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_REIMPL_NAVIGATION_CAPTURING_THROTTLE_H_

#include "base/auto_reset.h"
#include "base/values.h"
#include "content/public/browser/navigation_throttle.h"

class Profile;

namespace base {
class TickClock;
}

namespace apps {

// Navigation throttle used on ChromeOS to implement link capturing for cases
// not handled by `NavigationCapturingProcess` (which handles V2 capturing for
// new frames/dispositions).
//
// Specifically, this throttle handles:
// 1. ARC apps (Android apps on ChromeOS classic).
// 2. Projector System Web App (and potentially other System Web Apps).
// 3. ChromeOS Web App Experiments (e.g. Microsoft 365 experiment app).
// 4. Supplemental V2 capturing for regular Web Apps when navigating in existing
//    frames (if `kNavigationCapturingOnExistingFrames` is enabled).
class ChromeOsReimplNavigationCapturingThrottle
    : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  static bool MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  // Set clock used for timing to enable manipulation during tests.
  static base::AutoReset<const base::TickClock*> SetClockForTesting(
      const base::TickClock* tick_clock);

  ChromeOsReimplNavigationCapturingThrottle(
      const ChromeOsReimplNavigationCapturingThrottle&) = delete;
  ChromeOsReimplNavigationCapturingThrottle& operator=(
      const ChromeOsReimplNavigationCapturingThrottle&) = delete;
  ~ChromeOsReimplNavigationCapturingThrottle() override;

  // content::NavigationHandle overrides:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

 private:
  ChromeOsReimplNavigationCapturingThrottle(
      content::NavigationThrottleRegistry& registry,
      Profile* profile);

  ThrottleCheckResult HandleRequest();

  // Identify whether web contents need to be deleted post navigation capturing.
  bool IsEmptyDanglingWebContentsAfterLinkCapture();

  base::DictValue debug_data_;

  raw_ref<Profile> profile_;
  base::WeakPtrFactory<ChromeOsReimplNavigationCapturingThrottle>
      weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_REIMPL_NAVIGATION_CAPTURING_THROTTLE_H_
