// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_navigation_throttle.h"

#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {

constexpr char kTestUrl[] = "https://example.com";

class AwContentRestrictionNavigationThrottleTest : public testing::Test {
 protected:
  void SetUp() override {
    handle_.set_url(GURL(kTestUrl));
    handle_.set_has_committed(true);
  }

  content::BrowserTaskEnvironment task_environment_;
  content::MockNavigationHandle handle_;
  content::MockNavigationThrottleRegistry registry_{&handle_};
  AwContentRestrictionBlockedNavigationTracker tracker_;
};

TEST_F(AwContentRestrictionNavigationThrottleTest,
       ProceedsWhenNavigationNotBlocked) {
  AwContentRestrictionNavigationThrottle throttle(registry_, &tracker_);
  EXPECT_EQ(throttle.WillFailRequest().action(),
            content::NavigationThrottle::PROCEED);
}

TEST_F(AwContentRestrictionNavigationThrottleTest,
       CancelsWhenNavigationBlocked) {
  const int64_t navigation_id = handle_.GetNavigationId();
  tracker_.RegisterNavigationAsBlocked(navigation_id);

  AwContentRestrictionNavigationThrottle throttle(registry_, &tracker_);
  content::NavigationThrottle::ThrottleCheckResult result =
      throttle.WillFailRequest();

  EXPECT_EQ(result.action(), content::NavigationThrottle::CANCEL);
  EXPECT_EQ(result.net_error_code(), net::ERR_BLOCKED_BY_CLIENT);

  // Also verify that the navigation is unregistered in the tracker after the
  // error page is shown.
  ASSERT_TRUE(result.error_page_content().has_value());
  EXPECT_FALSE(result.error_page_content()->empty());
  EXPECT_FALSE(tracker_.IsNavigationBlocked(navigation_id));
}

}  // namespace
}  // namespace android_webview
