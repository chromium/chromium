// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_NAVIGATION_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace android_webview {
class AwSafeBrowsingNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit AwSafeBrowsingNavigationThrottle(content::NavigationHandle* handle);
  ~AwSafeBrowsingNavigationThrottle() override {}
  const char* GetNameForLogging() override;

  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_NAVIGATION_THROTTLE_H_
