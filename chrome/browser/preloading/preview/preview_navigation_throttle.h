// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

// Enforces block policy for Link Preview.
//
// This throttle is installed only for Link Preview navigations, which are used
// to show preview in preview window. Link Preview has limitations on
// navigations, e.g. it blocks non-HTTPS navigations and shows an error page
// instead.
//
// For more details, see
// https://docs.google.com/document/d/1hrWfVIDrPkrBlf8A576dDBH7Q34ESMLvOObt0j9i0SU
// and
// https://docs.google.com/document/d/1ogg_As8_IqhIX9Ck0AJ2Wgg_ezZsLsCj9jgbQhuCfI0
class PreviewNavigationThrottle : public content::NavigationThrottle {
 public:
  ~PreviewNavigationThrottle() override;

  static std::unique_ptr<PreviewNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  // content::NavigationThrottle:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

 private:
  explicit PreviewNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  ThrottleCheckResult WillStartRequestOrRedirect();
};

#endif  // CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_NAVIGATION_THROTTLE_H_
