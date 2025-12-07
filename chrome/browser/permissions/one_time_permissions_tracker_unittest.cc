// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_tracker.h"

#include <cstdint>

#include "base/test/task_environment.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_context_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
class OneTimePermissionsTrackerObserverForTesting
    : public OneTimePermissionsTrackerObserver {
 public:
  void OnAllTabsInBackgroundTimerExpired(
      const url::Origin& origin,
      const BackgroundExpiryType& expiry_type) override {
    switch (expiry_type) {
      case BackgroundExpiryType::kTimeout:
        ++notified_count_short_timeout_;
        break;
      case BackgroundExpiryType::kLongTimeout:
        ++notified_count_long_timeout_;
        break;
    }
    last_notified_origin_ = origin;
  }

  uint32_t NotifiedCountShortTimeout() { return notified_count_short_timeout_; }
  uint32_t NotifiedCountLongTimeout() { return notified_count_long_timeout_; }
  const url::Origin& LastNotifiedOrigin() { return last_notified_origin_; }

 private:
  uint32_t notified_count_short_timeout_ = 0;
  uint32_t notified_count_long_timeout_ = 0;
  url::Origin last_notified_origin_;
};
}  // namespace

class OneTimePermissionsTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  OneTimePermissionsTrackerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tracker_ = std::make_unique<OneTimePermissionsTracker>();
  }

  void TearDown() override {
    tracker_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  OneTimePermissionsTracker* tracker() { return tracker_.get(); }

 private:
  std::unique_ptr<OneTimePermissionsTracker> tracker_;
};

TEST_F(OneTimePermissionsTrackerTest, ShouldIgnoreOrigin_IsolatedWebApp) {
  GURL isolated_web_app_url(
      "isolated-app://"
      "cpt62davrxj4yzauslsummydorzgy2kcnhbayaziceuqlzhaue7qaaic/");
  url::Origin isolated_web_app_origin =
      url::Origin::Create(isolated_web_app_url);
  EXPECT_FALSE(tracker()->ShouldIgnoreOrigin(isolated_web_app_origin));
}

TEST_F(OneTimePermissionsTrackerTest, ShouldIgnoreOrigin_OpaqueOrigin) {
  url::Origin opaque_origin;
  ASSERT_TRUE(opaque_origin.opaque());
  EXPECT_TRUE(tracker()->ShouldIgnoreOrigin(opaque_origin));
}

TEST_F(OneTimePermissionsTrackerTest, ShouldIgnoreOrigin_ChromePages) {
  GURL page_1("chrome://newtab/");
  url::Origin page_1_origin = url::Origin::Create(page_1);
  EXPECT_TRUE(tracker()->ShouldIgnoreOrigin(page_1_origin));

  GURL page_2("chrome://new-tab-page/");
  url::Origin page_2_origin = url::Origin::Create(page_2);
  EXPECT_TRUE(tracker()->ShouldIgnoreOrigin(page_2_origin));
}

TEST_F(OneTimePermissionsTrackerTest, NotifyAfterShortTimeout) {
  const url::Origin origin = url::Origin::Create(
      GURL("isolated-app://"
           "cpt62davrxj4yzauslsummydorzgy2kcnhbayaziceuqlzhaue7qaaic/"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  tracker()->WebContentsLoadedOrigin(origin);
  tracker()->WebContentsBackgrounded(origin);

  ASSERT_EQ(observer.NotifiedCountShortTimeout(), 0u);
  // Fast forward time by more than the timeout.
  task_environment()->FastForwardBy(permissions::kOneTimePermissionTimeout +
                                    base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountShortTimeout(), 1u);
  EXPECT_EQ(observer.LastNotifiedOrigin(), origin);
  tracker()->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, DoNotNotifyBeforeShortTimeout) {
  const url::Origin origin = url::Origin::Create(
      GURL("isolated-app://"
           "cpt62davrxj4yzauslsummydorzgy2kcnhbayaziceuqlzhaue7qaaic/"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  tracker()->WebContentsLoadedOrigin(origin);
  tracker()->WebContentsBackgrounded(origin);

  ASSERT_EQ(observer.NotifiedCountShortTimeout(), 0u);
  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(permissions::kOneTimePermissionTimeout -
                                    base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountShortTimeout(), 0u);
  tracker()->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, ShortTimerResetOnUnbackgrounded) {
  const url::Origin origin = url::Origin::Create(
      GURL("isolated-app://"
           "cpt62davrxj4yzauslsummydorzgy2kcnhbayaziceuqlzhaue7qaaic/"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  tracker()->WebContentsLoadedOrigin(origin);
  tracker()->WebContentsBackgrounded(origin);

  ASSERT_EQ(observer.NotifiedCountShortTimeout(), 0u);
  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(permissions::kOneTimePermissionTimeout -
                                    base::Seconds(1));
  ASSERT_EQ(observer.NotifiedCountShortTimeout(), 0u);

  // Unbackground and background the page to simulate usage.
  tracker()->WebContentsUnbackgrounded(origin);
  tracker()->WebContentsBackgrounded(origin);

  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(permissions::kOneTimePermissionTimeout -
                                    base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountShortTimeout(), 0u);
  tracker()->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, NotifyAfterLongTimeout) {
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  tracker()->WebContentsLoadedOrigin(origin);
  tracker()->WebContentsBackgrounded(origin);

  ASSERT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  // Fast forward time by more than the timeout.
  task_environment()->FastForwardBy(
      permissions::kOneTimePermissionMaximumLifetime + base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountLongTimeout(), 1u);
  EXPECT_EQ(observer.LastNotifiedOrigin(), origin);
  tracker()->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, DoNotNotifyBeforeLongTimeout) {
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  tracker()->WebContentsLoadedOrigin(origin);
  tracker()->WebContentsBackgrounded(origin);

  ASSERT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(
      permissions::kOneTimePermissionMaximumLifetime - base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  tracker()->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, LongTimerResetOnUnbackgrounded) {
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  tracker()->WebContentsLoadedOrigin(origin);
  tracker()->WebContentsBackgrounded(origin);

  ASSERT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(
      permissions::kOneTimePermissionMaximumLifetime - base::Seconds(1));
  ASSERT_EQ(observer.NotifiedCountLongTimeout(), 0u);

  // Unbackground and background the page to simulate usage.
  tracker()->WebContentsUnbackgrounded(origin);
  tracker()->WebContentsBackgrounded(origin);

  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(
      permissions::kOneTimePermissionMaximumLifetime - base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  tracker()->RemoveObserver(&observer);
}
