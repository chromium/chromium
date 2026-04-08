// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_BLOCKED_NAVIGATION_TRACKER_H_
#define ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_BLOCKED_NAVIGATION_TRACKER_H_

#include <stdint.h>

#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace android_webview {

// A tracker implementation for tracking blocked WebView navigations due to
// content restriction capabilities enforced on the WebView instance. Primarily
// used by the navigation throttle to surface a custom error page.
class AwContentRestrictionBlockedNavigationTracker {
 public:
  AwContentRestrictionBlockedNavigationTracker();
  AwContentRestrictionBlockedNavigationTracker(
      const AwContentRestrictionBlockedNavigationTracker&) = delete;
  AwContentRestrictionBlockedNavigationTracker& operator=(
      const AwContentRestrictionBlockedNavigationTracker&) = delete;
  ~AwContentRestrictionBlockedNavigationTracker();

  // Registers the specified navigation ID as being blocked.
  void RegisterNavigationAsBlocked(int64_t navigation_id);

  // Returns true if the specified navigation ID has been registered as blocked.
  // False otherwise.
  bool IsNavigationBlocked(int64_t navigation_id) const;

  // Clears the specified navigation ID if it has been registered. Normally done
  // after lookup to free up memory.
  void ClearNavigationBlocked(int64_t navigation_id);

 private:
  absl::flat_hash_set<int64_t> blocked_navigation_ids_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_CONTENT_RESTRICTION_AW_CONTENT_RESTRICTION_BLOCKED_NAVIGATION_TRACKER_H_
