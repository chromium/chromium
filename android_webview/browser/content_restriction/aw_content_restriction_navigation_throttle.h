// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_NAVIGATION_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_NAVIGATION_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"

namespace android_webview {
class AwContentRestrictionBlockedNavigationTracker;

// Navigation throttle implementation that presents a custom error page when a
// navigation is blocked through content restriction.
class AwContentRestrictionNavigationThrottle
    : public content::NavigationThrottle {
 public:
  explicit AwContentRestrictionNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      AwContentRestrictionBlockedNavigationTracker* tracker);
  AwContentRestrictionNavigationThrottle(
      const AwContentRestrictionNavigationThrottle&) = delete;
  AwContentRestrictionNavigationThrottle& operator=(
      const AwContentRestrictionNavigationThrottle&) = delete;
  ~AwContentRestrictionNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillFailRequest() override;
  const char* GetNameForLogging() override;

 private:
  raw_ptr<AwContentRestrictionBlockedNavigationTracker> tracker_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_NAVIGATION_THROTTLE_H_
