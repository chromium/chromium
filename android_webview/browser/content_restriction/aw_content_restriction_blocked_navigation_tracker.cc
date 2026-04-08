// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"

#include "content/public/browser/browser_thread.h"

namespace android_webview {

AwContentRestrictionBlockedNavigationTracker::
    AwContentRestrictionBlockedNavigationTracker() = default;

AwContentRestrictionBlockedNavigationTracker::
    ~AwContentRestrictionBlockedNavigationTracker() = default;

void AwContentRestrictionBlockedNavigationTracker::RegisterNavigationAsBlocked(
    int64_t navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  blocked_navigation_ids_.insert(navigation_id);
}

bool AwContentRestrictionBlockedNavigationTracker::IsNavigationBlocked(
    int64_t navigation_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return blocked_navigation_ids_.find(navigation_id) !=
         blocked_navigation_ids_.end();
}

void AwContentRestrictionBlockedNavigationTracker::ClearNavigationBlocked(
    int64_t navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  blocked_navigation_ids_.erase(navigation_id);
}

}  // namespace android_webview
