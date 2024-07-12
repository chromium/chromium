// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_NAVIGATION_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

#include <memory>

namespace content {
class NavigationHandle;
}  // namespace content

namespace android_webview {

// This throttle monitors failed requests in an outer-most main frame (i.e.
// doesn't apply for fenced-frames), and if a request failed due to
// it being blocked by Safe Browsing, it creates and displays an interstitial.
// For other kinds of loads, the interstitial is navigated at the same time the
// load is canceled in BaseUIManager::DisplayBlockingPage
//
// This NavigationThrottle doesn't actually perform a SafeBrowsing check nor
// block the navigation.  That happens in SafeBrowsing's
// BrowserURLLoaderThrottle and RendererURLLoaderThrottles and related code.
// Those cause the navigation to fail which invokes this throttle to show the
// interstitial.
// Lifetime: Temporary
class AwSafeBrowsingNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<AwSafeBrowsingNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* handle);
  ~AwSafeBrowsingNavigationThrottle() override {}
  const char* GetNameForLogging() override;

  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;

 private:
  explicit AwSafeBrowsingNavigationThrottle(content::NavigationHandle* handle);
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_NAVIGATION_THROTTLE_H_
