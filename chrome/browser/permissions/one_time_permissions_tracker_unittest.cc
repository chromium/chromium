// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_tracker.h"

#include <cstdint>

#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_helper.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_context_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

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

  void OnLastPageFromOriginClosed(const url::Origin& origin) override {
    ++notified_count_last_page_closed_;
    last_notified_origin_ = origin;
  }

  void OnCapturingVideoExpired(const url::Origin& origin) override {
    ++notified_count_capturing_video_expired_;
    last_notified_origin_ = origin;
  }

  void OnCapturingAudioExpired(const url::Origin& origin) override {
    ++notified_count_capturing_audio_expired_;
    last_notified_origin_ = origin;
  }

  uint32_t NotifiedCountShortTimeout() { return notified_count_short_timeout_; }
  uint32_t NotifiedCountLongTimeout() { return notified_count_long_timeout_; }
  uint32_t NotifiedCountLastPageClosed() {
    return notified_count_last_page_closed_;
  }
  uint32_t NotifiedCountCapturingVideoExpired() {
    return notified_count_capturing_video_expired_;
  }
  uint32_t NotifiedCountCapturingAudioExpired() {
    return notified_count_capturing_audio_expired_;
  }
  const url::Origin& LastNotifiedOrigin() { return last_notified_origin_; }

 private:
  uint32_t notified_count_short_timeout_ = 0;
  uint32_t notified_count_long_timeout_ = 0;
  uint32_t notified_count_last_page_closed_ = 0;
  uint32_t notified_count_capturing_video_expired_ = 0;
  uint32_t notified_count_capturing_audio_expired_ = 0;
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
  EXPECT_FALSE(OneTimePermissionsTrackerHelper::ShouldIgnoreOriginForTesting(
      isolated_web_app_origin));
}

TEST_F(OneTimePermissionsTrackerTest, ShouldIgnoreOrigin_OpaqueOrigin) {
  url::Origin opaque_origin;
  ASSERT_TRUE(opaque_origin.opaque());
  EXPECT_TRUE(OneTimePermissionsTrackerHelper::ShouldIgnoreOriginForTesting(
      opaque_origin));
}

TEST_F(OneTimePermissionsTrackerTest, ShouldIgnoreOrigin_ChromePages) {
  GURL page_1("chrome://newtab/");
  url::Origin page_1_origin = url::Origin::Create(page_1);
  EXPECT_TRUE(OneTimePermissionsTrackerHelper::ShouldIgnoreOriginForTesting(
      page_1_origin));

  GURL page_2("chrome://new-tab-page/");
  url::Origin page_2_origin = url::Origin::Create(page_2);
  EXPECT_TRUE(OneTimePermissionsTrackerHelper::ShouldIgnoreOriginForTesting(
      page_2_origin));
}

TEST_F(OneTimePermissionsTrackerTest, NotifyAfterShortTimeout) {
  const url::Origin origin = url::Origin::Create(
      GURL("isolated-app://"
           "cpt62davrxj4yzauslsummydorzgy2kcnhbayaziceuqlzhaue7qaaic/"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  auto fg_tracker = tracker()->NewForegroundPage(origin);
  fg_tracker.reset();

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
  auto fg_tracker = tracker()->NewForegroundPage(origin);
  fg_tracker.reset();

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
  auto fg_tracker = tracker()->NewForegroundPage(origin);
  fg_tracker.reset();

  ASSERT_EQ(observer.NotifiedCountShortTimeout(), 0u);
  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(permissions::kOneTimePermissionTimeout -
                                    base::Seconds(1));
  ASSERT_EQ(observer.NotifiedCountShortTimeout(), 0u);

  // Unbackground and background the page to simulate usage.
  fg_tracker = tracker()->NewForegroundPage(origin);
  fg_tracker.reset();

  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(permissions::kOneTimePermissionTimeout -
                                    base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountShortTimeout(), 0u);
  tracker()->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, NotifyAfterLongTimeout) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  auto fg_tracker = tracker()->NewForegroundPage(origin);
  fg_tracker.reset();

  ASSERT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  // Fast forward time by more than the timeout.
  task_environment()->FastForwardBy(
      permissions::kOneTimePermissionMaximumLifetime + base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountLongTimeout(), 1u);
  EXPECT_EQ(observer.LastNotifiedOrigin(), origin);
  tracker()->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, DoNotNotifyBeforeLongTimeout) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  auto fg_tracker = tracker()->NewForegroundPage(origin);
  fg_tracker.reset();

  ASSERT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(
      permissions::kOneTimePermissionMaximumLifetime - base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  tracker()->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, LongTimerResetOnUnbackgrounded) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  OneTimePermissionsTrackerObserverForTesting observer;
  tracker()->AddObserver(&observer);
  auto fg_tracker = tracker()->NewForegroundPage(origin);
  fg_tracker.reset();

  ASSERT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(
      permissions::kOneTimePermissionMaximumLifetime - base::Seconds(1));
  ASSERT_EQ(observer.NotifiedCountLongTimeout(), 0u);

  // Unbackground and background the page to simulate usage.
  fg_tracker = tracker()->NewForegroundPage(origin);
  fg_tracker.reset();

  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(
      permissions::kOneTimePermissionMaximumLifetime - base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountLongTimeout(), 0u);
  tracker()->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, PageTrackerLifecycle) {
  OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents());

  const GURL origin_url("https://example.com");
  const url::Origin origin = url::Origin::Create(origin_url);

  OneTimePermissionsTrackerObserverForTesting observer;
  auto* factory_tracker =
      OneTimePermissionsTrackerFactory::GetForBrowserContext(profile());
  factory_tracker->AddObserver(&observer);

  NavigateAndCommit(origin_url);
  EXPECT_EQ(observer.NotifiedCountLastPageClosed(), 0u);

  // Navigating to a different origin should deactivate the old page and fire
  // OnLastPageFromOriginClosed for example.com.
  const GURL other_url("https://other.com");
  NavigateAndCommit(other_url);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return observer.NotifiedCountLastPageClosed() == 1u; }));
  EXPECT_EQ(observer.LastNotifiedOrigin(), origin);

  factory_tracker->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, PageTrackerSameOriginNavigation) {
  OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents());

  const GURL origin_url1("https://example.com/page1.html");
  const GURL origin_url2("https://example.com/page2.html");
  const url::Origin origin = url::Origin::Create(origin_url1);

  OneTimePermissionsTrackerObserverForTesting observer;
  auto* factory_tracker =
      OneTimePermissionsTrackerFactory::GetForBrowserContext(profile());
  factory_tracker->AddObserver(&observer);

  NavigateAndCommit(origin_url1);
  EXPECT_EQ(observer.NotifiedCountLastPageClosed(), 0u);

  // Navigating same-origin should NOT fire OnLastPageFromOriginClosed.
  NavigateAndCommit(origin_url2);
  EXPECT_EQ(observer.NotifiedCountLastPageClosed(), 0u);

  factory_tracker->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, PageTrackerDiscard) {
  OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents());

  const GURL origin_url("https://example.com");
  const url::Origin origin = url::Origin::Create(origin_url);

  OneTimePermissionsTrackerObserverForTesting observer;
  auto* factory_tracker =
      OneTimePermissionsTrackerFactory::GetForBrowserContext(profile());
  factory_tracker->AddObserver(&observer);

  NavigateAndCommit(origin_url);
  EXPECT_EQ(observer.NotifiedCountLastPageClosed(), 0u);

  // Discarding the WebContents should delete the PageTracker and fire
  // OnLastPageFromOriginClosed for example.com.
  web_contents()->SetWasDiscarded(true);
  web_contents()->NotifyWasDiscarded();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return observer.NotifiedCountLastPageClosed() == 1u; }));
  EXPECT_EQ(observer.LastNotifiedOrigin(), origin);

  factory_tracker->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, PageTrackerMediaCapture) {
  OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents());
  auto* helper =
      OneTimePermissionsTrackerHelper::FromWebContents(web_contents());

  const GURL origin_url("https://example.com");
  const url::Origin origin = url::Origin::Create(origin_url);

  OneTimePermissionsTrackerObserverForTesting observer;
  auto* factory_tracker =
      OneTimePermissionsTrackerFactory::GetForBrowserContext(profile());
  factory_tracker->AddObserver(&observer);

  NavigateAndCommit(origin_url);
  helper->OnVisibilityChanged(content::Visibility::HIDDEN);

  helper->OnIsCapturingVideoChanged(web_contents(), true);
  helper->OnIsCapturingVideoChanged(web_contents(), false);

  EXPECT_EQ(observer.NotifiedCountCapturingVideoExpired(), 0u);
  task_environment()->FastForwardBy(permissions::kOneTimePermissionTimeout +
                                    base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountCapturingVideoExpired(), 1u);
  EXPECT_EQ(observer.LastNotifiedOrigin(), origin);

  factory_tracker->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, PageTrackerPageCreatedInBackground) {
  web_contents()->WasHidden();
  OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents());

  const GURL origin_url("https://example.com");
  const url::Origin origin = url::Origin::Create(origin_url);

  OneTimePermissionsTrackerObserverForTesting observer;
  auto* factory_tracker =
      OneTimePermissionsTrackerFactory::GetForBrowserContext(profile());
  factory_tracker->AddObserver(&observer);

  NavigateAndCommit(origin_url);

  EXPECT_EQ(observer.NotifiedCountShortTimeout(), 0u);
  // Fast forward time by less than the timeout.
  task_environment()->FastForwardBy(permissions::kOneTimePermissionTimeout -
                                    base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountShortTimeout(), 0u);

  // Fast forward time by more than the timeout.
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_EQ(observer.NotifiedCountShortTimeout(), 1u);
  EXPECT_EQ(observer.LastNotifiedOrigin(), origin);

  factory_tracker->RemoveObserver(&observer);
}

TEST_F(OneTimePermissionsTrackerTest, PageTrackerAudioCapture) {
  OneTimePermissionsTrackerHelper::CreateForWebContents(web_contents());
  auto* helper =
      OneTimePermissionsTrackerHelper::FromWebContents(web_contents());

  const GURL origin_url("https://example.com");
  const url::Origin origin = url::Origin::Create(origin_url);

  OneTimePermissionsTrackerObserverForTesting observer;
  auto* factory_tracker =
      OneTimePermissionsTrackerFactory::GetForBrowserContext(profile());
  factory_tracker->AddObserver(&observer);

  NavigateAndCommit(origin_url);
  helper->OnVisibilityChanged(content::Visibility::HIDDEN);

  helper->OnIsCapturingAudioChanged(web_contents(), true);
  helper->OnIsCapturingAudioChanged(web_contents(), false);

  EXPECT_EQ(observer.NotifiedCountCapturingAudioExpired(), 0u);
  task_environment()->FastForwardBy(permissions::kOneTimePermissionTimeout +
                                    base::Seconds(1));
  EXPECT_EQ(observer.NotifiedCountCapturingAudioExpired(), 1u);
  EXPECT_EQ(observer.LastNotifiedOrigin(), origin);

  factory_tracker->RemoveObserver(&observer);
}
