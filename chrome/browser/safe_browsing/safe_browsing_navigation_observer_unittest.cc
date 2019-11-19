// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

class SBNavigationObserverTest : public BrowserWithTestWindowTest {
 public:
  SBNavigationObserverTest() {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("http://foo/0"));
    navigation_observer_manager_ = new SafeBrowsingNavigationObserverManager();
    navigation_observer_ = new SafeBrowsingNavigationObserver(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        navigation_observer_manager_);
  }
  void TearDown() override {
    delete navigation_observer_;
    BrowserWithTestWindowTest::TearDown();
  }
  void VerifyNavigationEvent(
      const GURL& expected_source_url,
      const GURL& expected_source_main_frame_url,
      const GURL& expected_original_request_url,
      const GURL& expected_destination_url,
      SessionID expected_source_tab,
      SessionID expected_target_tab,
      ReferrerChainEntry::NavigationInitiation expected_nav_initiation,
      bool expected_has_committed,
      bool expected_has_server_redirect,
      NavigationEvent* actual_nav_event) {
    EXPECT_EQ(expected_source_url, actual_nav_event->source_url);
    EXPECT_EQ(expected_source_main_frame_url,
              actual_nav_event->source_main_frame_url);
    EXPECT_EQ(expected_original_request_url,
              actual_nav_event->original_request_url);
    EXPECT_EQ(expected_destination_url, actual_nav_event->GetDestinationUrl());
    EXPECT_EQ(expected_source_tab, actual_nav_event->source_tab_id);
    EXPECT_EQ(expected_target_tab, actual_nav_event->target_tab_id);
    EXPECT_EQ(expected_nav_initiation, actual_nav_event->navigation_initiation);
    EXPECT_EQ(expected_has_committed, actual_nav_event->has_committed);
    EXPECT_EQ(expected_has_server_redirect,
              !actual_nav_event->server_redirect_urls.empty());
  }

  NavigationEventList* navigation_event_list() {
    return navigation_observer_manager_->navigation_event_list();
  }

  SafeBrowsingNavigationObserverManager::UserGestureMap* user_gesture_map() {
    return &navigation_observer_manager_->user_gesture_map_;
  }

  SafeBrowsingNavigationObserverManager::HostToIpMap* host_to_ip_map() {
    return &navigation_observer_manager_->host_to_ip_map_;
  }

  void RecordHostToIpMapping(const std::string& host, const std::string& ip) {
    navigation_observer_manager_->RecordHostToIpMapping(host, ip);
  }

  std::unique_ptr<NavigationEvent> CreateNavigationEventUniquePtr(
      const GURL& destination_url,
      const base::Time& timestamp) {
    std::unique_ptr<NavigationEvent> nav_event_ptr =
        std::make_unique<NavigationEvent>();
    nav_event_ptr->original_request_url = destination_url;
    nav_event_ptr->source_url = GURL("http://dummy.com");
    nav_event_ptr->last_updated = timestamp;
    return nav_event_ptr;
  }

  void CleanUpNavigationEvents() {
    navigation_observer_manager_->CleanUpNavigationEvents();
  }

  void CleanUpIpAddresses() {
    navigation_observer_manager_->CleanUpIpAddresses();
  }

  void CleanUpUserGestures() {
    navigation_observer_manager_->CleanUpUserGestures();
  }

 protected:
  SafeBrowsingNavigationObserverManager* navigation_observer_manager_;
  SafeBrowsingNavigationObserver* navigation_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SBNavigationObserverTest);
};

TEST_F(SBNavigationObserverTest, TestNavigationEventList) {
  NavigationEventList events(3);

  EXPECT_EQ(nullptr, events.FindNavigationEvent(
                         base::Time::Now(), GURL("http://invalid.com"), GURL(),
                         SessionID::InvalidValue()));
  EXPECT_EQ(0U, events.CleanUpNavigationEvents());
  EXPECT_EQ(0U, events.Size());

  // Add 2 events to the list.
  base::Time now = base::Time::Now();
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo1.com"), one_hour_ago));
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo1.com"), now));
  EXPECT_EQ(2U, events.Size());
  // FindNavigationEvent should return the latest matching event.
  EXPECT_EQ(now,
            events
                .FindNavigationEvent(base::Time::Now(), GURL("http://foo1.com"),
                                     GURL(), SessionID::InvalidValue())
                ->last_updated);
  // One event should get removed.
  EXPECT_EQ(1U, events.CleanUpNavigationEvents());
  EXPECT_EQ(1U, events.Size());

  // Add 3 more events, previously recorded events should be overridden.
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo3.com"), one_hour_ago));
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo4.com"), one_hour_ago));
  events.RecordNavigationEvent(
      CreateNavigationEventUniquePtr(GURL("http://foo5.com"), now));
  ASSERT_EQ(3U, events.Size());
  EXPECT_EQ(GURL("http://foo3.com"), events.Get(0)->original_request_url);
  EXPECT_EQ(GURL("http://foo4.com"), events.Get(1)->original_request_url);
  EXPECT_EQ(GURL("http://foo5.com"), events.Get(2)->original_request_url);
  EXPECT_EQ(2U, events.CleanUpNavigationEvents());
  EXPECT_EQ(1U, events.Size());
}

TEST_F(SBNavigationObserverTest, BasicNavigationAndCommit) {
  // Navigation in current tab.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  browser()->OpenURL(
      content::OpenURLParams(GURL("http://foo/1"), content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));
  CommitPendingLoad(controller);
  SessionID tab_id = SessionTabHelper::IdForTab(controller->GetWebContents());
  auto* nav_list = navigation_event_list();
  ASSERT_EQ(1U, nav_list->Size());
  VerifyNavigationEvent(GURL(),                // source_url
                        GURL(),                // source_main_frame_url
                        GURL("http://foo/1"),  // original_request_url
                        GURL("http://foo/1"),  // destination_url
                        tab_id,                // source_tab_id
                        tab_id,                // target_tab_id
                        ReferrerChainEntry::BROWSER_INITIATED,
                        true,   // has_committed
                        false,  // has_server_redirect
                        nav_list->Get(0U));
}

TEST_F(SBNavigationObserverTest, ServerRedirect) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://foo/3"),
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
  navigation->Start();
  navigation->Redirect(GURL("http://redirect/1"));
  navigation->Commit();
  SessionID tab_id = SessionTabHelper::IdForTab(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  auto* nav_list = navigation_event_list();
  ASSERT_EQ(1U, nav_list->Size());
  VerifyNavigationEvent(
      GURL("http://foo/0"),       // source_url
      GURL("http://foo/0"),       // source_main_frame_url
      GURL("http://foo/3"),       // original_request_url
      GURL("http://redirect/1"),  // destination_url
      tab_id,                     // source_tab_id
      tab_id,                     // target_tab_id
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      true,  // has_committed
      true,  // has_server_redirect
      nav_list->Get(0U));
}

TEST_F(SBNavigationObserverTest, TestCleanUpStaleNavigationEvents) {
  // Sets up navigation_event_list() such that it includes fresh, stale and
  // invalid
  // navigation events.
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);  // Stale
  base::Time one_minute_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0);  // Fresh
  base::Time in_an_hour =
      base::Time::FromDoubleT(now.ToDoubleT() + 60.0 * 60.0);  // Invalid
  GURL url_0("http://foo/0");
  GURL url_1("http://foo/1");
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_0, in_an_hour));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_0, one_hour_ago));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_1, one_hour_ago));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_1, one_hour_ago));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_0, one_minute_ago));
  navigation_event_list()->RecordNavigationEvent(
      CreateNavigationEventUniquePtr(url_0, now));
  ASSERT_EQ(6U, navigation_event_list()->Size());

  // Cleans up navigation events.
  CleanUpNavigationEvents();

  // Verifies all stale and invalid navigation events are removed.
  ASSERT_EQ(2U, navigation_event_list()->Size());
  EXPECT_EQ(nullptr,
            navigation_event_list()->FindNavigationEvent(
                base::Time::Now(), url_1, GURL(), SessionID::InvalidValue()));
}

TEST_F(SBNavigationObserverTest, TestCleanUpStaleUserGestures) {
  // Sets up user_gesture_map() such that it includes fresh, stale and invalid
  // user gestures.
  base::Time now = base::Time::Now();  // Fresh
  base::Time three_minutes_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 3);  // Stale
  base::Time in_an_hour =
      base::Time::FromDoubleT(now.ToDoubleT() + 60.0 * 60.0);  // Invalid
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));
  content::WebContents* content0 =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* content1 =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  content::WebContents* content2 =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  user_gesture_map()->insert(std::make_pair(content0, now));
  user_gesture_map()->insert(std::make_pair(content1, three_minutes_ago));
  user_gesture_map()->insert(std::make_pair(content2, in_an_hour));
  ASSERT_EQ(3U, user_gesture_map()->size());

  // Cleans up user_gesture_map()
  CleanUpUserGestures();

  // Verifies all stale and invalid user gestures are removed.
  ASSERT_EQ(1U, user_gesture_map()->size());
  EXPECT_NE(user_gesture_map()->end(), user_gesture_map()->find(content0));
  EXPECT_EQ(now, (*user_gesture_map())[content0]);
}

TEST_F(SBNavigationObserverTest, TestCleanUpStaleIPAddresses) {
  // Sets up host_to_ip_map() such that it includes fresh, stale and invalid
  // user gestures.
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);  // Stale
  base::Time in_an_hour =
      base::Time::FromDoubleT(now.ToDoubleT() + 60.0 * 60.0);  // Invalid
  std::string host_0 = GURL("http://foo/0").host();
  std::string host_1 = GURL("http://bar/1").host();
  host_to_ip_map()->insert(
      std::make_pair(host_0, std::vector<ResolvedIPAddress>()));
  (*host_to_ip_map())[host_0].push_back(ResolvedIPAddress(now, "1.1.1.1"));
  (*host_to_ip_map())[host_0].push_back(
      ResolvedIPAddress(one_hour_ago, "2.2.2.2"));
  host_to_ip_map()->insert(
      std::make_pair(host_1, std::vector<ResolvedIPAddress>()));
  (*host_to_ip_map())[host_1].push_back(
      ResolvedIPAddress(in_an_hour, "3.3.3.3"));
  ASSERT_EQ(2U, host_to_ip_map()->size());

  // Cleans up host_to_ip_map()
  CleanUpIpAddresses();

  // Verifies all stale and invalid IP addresses are removed.
  ASSERT_EQ(1U, host_to_ip_map()->size());
  EXPECT_EQ(host_to_ip_map()->end(), host_to_ip_map()->find(host_1));
  ASSERT_EQ(1U, (*host_to_ip_map())[host_0].size());
  EXPECT_EQ(now, (*host_to_ip_map())[host_0].front().timestamp);
}

TEST_F(SBNavigationObserverTest, TestRecordHostToIpMapping) {
  // Setup host_to_ip_map().
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);  // Stale
  std::string host_0 = GURL("http://foo/0").host();
  host_to_ip_map()->insert(
      std::make_pair(host_0, std::vector<ResolvedIPAddress>()));
  (*host_to_ip_map())[host_0].push_back(ResolvedIPAddress(now, "1.1.1.1"));
  (*host_to_ip_map())[host_0].push_back(
      ResolvedIPAddress(one_hour_ago, "2.2.2.2"));

  // Record a host-IP pair, where host is already in the map, and IP has
  // never been seen before.
  RecordHostToIpMapping(host_0, "3.3.3.3");
  ASSERT_EQ(1U, host_to_ip_map()->size());
  EXPECT_EQ(3U, (*host_to_ip_map())[host_0].size());
  EXPECT_EQ("3.3.3.3", (*host_to_ip_map())[host_0][2].ip);

  // Record a host-IP pair which is already in the map. It should simply update
  // its timestamp.
  ASSERT_EQ(now, (*host_to_ip_map())[host_0][0].timestamp);
  RecordHostToIpMapping(host_0, "1.1.1.1");
  ASSERT_EQ(1U, host_to_ip_map()->size());
  EXPECT_EQ(3U, (*host_to_ip_map())[host_0].size());
  EXPECT_LT(now, (*host_to_ip_map())[host_0][2].timestamp);

  // Record a host-ip pair, neither of which has been seen before.
  std::string host_1 = GURL("http://bar/1").host();
  RecordHostToIpMapping(host_1, "9.9.9.9");
  ASSERT_EQ(2U, host_to_ip_map()->size());
  EXPECT_EQ(3U, (*host_to_ip_map())[host_0].size());
  EXPECT_EQ(1U, (*host_to_ip_map())[host_1].size());
  EXPECT_EQ("9.9.9.9", (*host_to_ip_map())[host_1][0].ip);
}

TEST_F(SBNavigationObserverTest, TestContentSettingChange) {
  user_gesture_map()->clear();
  ASSERT_EQ(0U, user_gesture_map()->size());

  content::WebContents* web_content =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // Simulate content setting change via page info UI.
  navigation_observer_->OnContentSettingChanged(
      ContentSettingsPattern::FromURL(web_content->GetLastCommittedURL()),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      std::string());

  // A user gesture should be recorded.
  ASSERT_EQ(1U, user_gesture_map()->size());
  EXPECT_NE(user_gesture_map()->end(), user_gesture_map()->find(web_content));

  user_gesture_map()->clear();
  ASSERT_EQ(0U, user_gesture_map()->size());

  // Simulate content setting change that cannot be changed via page info UI.
  navigation_observer_->OnContentSettingChanged(
      ContentSettingsPattern::FromURL(web_content->GetLastCommittedURL()),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::SITE_ENGAGEMENT,
      std::string());
  // No user gesture should be recorded.
  EXPECT_EQ(0U, user_gesture_map()->size());
}

TEST_F(SBNavigationObserverTest, TimestampIsDecreasing) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);
  base::Time two_hours_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 2 * 60.0 * 60.0);

  // Add three navigations. The first is BROWSER_INITIATED to A. Then from A to
  // B, and then from B back to A.
  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://A.com");
  first_navigation->last_updated = two_hours_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->source_url = GURL("http://A.com");
  second_navigation->original_request_url = GURL("http://B.com");
  second_navigation->last_updated = one_hour_ago;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  std::unique_ptr<NavigationEvent> third_navigation =
      std::make_unique<NavigationEvent>();
  third_navigation->source_url = GURL("http://B.com");
  third_navigation->original_request_url = GURL("http://A.com");
  third_navigation->last_updated = now;
  third_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  navigation_event_list()->RecordNavigationEvent(std::move(third_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://A.com"), SessionID::InvalidValue(), 10, &referrer_chain);

  ASSERT_EQ(3, referrer_chain.size());

  EXPECT_GE(referrer_chain[0].navigation_time_msec(),
            referrer_chain[1].navigation_time_msec());
  EXPECT_GE(referrer_chain[1].navigation_time_msec(),
            referrer_chain[2].navigation_time_msec());
}

TEST_F(SBNavigationObserverTest, ChainWorksThroughNewTab) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);

  SessionID source_tab = SessionID::NewUnique();
  SessionID target_tab = SessionID::NewUnique();

  // Add two navigations. The first is renderer initiated and retargeting from A
  // to B. The second navigates the new tab to B.
  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->source_url = GURL("http://a.com/");
  first_navigation->original_request_url = GURL("http://b.com/");
  first_navigation->last_updated = one_hour_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  first_navigation->source_tab_id = source_tab;
  first_navigation->target_tab_id = target_tab;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->original_request_url = GURL("http://b.com/");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  second_navigation->source_tab_id = target_tab;
  second_navigation->target_tab_id = target_tab;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://b.com/"), SessionID::InvalidValue(), 10, &referrer_chain);

  ASSERT_EQ(1, referrer_chain.size());

  EXPECT_EQ("http://b.com/",referrer_chain[0].url());
  EXPECT_EQ("http://a.com/",referrer_chain[0].referrer_url());
  EXPECT_TRUE(referrer_chain[0].is_retargeting());
}

TEST_F(SBNavigationObserverTest, ChainContinuesThroughBrowserInitiated) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);

  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://a.com/");
  first_navigation->last_updated = one_hour_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->source_url = GURL("http://a.com/");
  second_navigation->original_request_url = GURL("http://b.com/");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://b.com/"), SessionID::InvalidValue(), 10, &referrer_chain);

  EXPECT_EQ(2, referrer_chain.size());
}

TEST_F(SBNavigationObserverTest,
       CanceledRetargetingNavigationHasCorrectEventUrl) {
  base::Time now = base::Time::Now();
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);

  SessionID source_tab = SessionID::NewUnique();
  SessionID target_tab = SessionID::NewUnique();

  // Add two navigations. A initially opens a new tab with url B, but cancels
  // that before it completes. It then navigates the new tab to C. We expect
  // that asking for the referrer chain for C has C as the event url.
  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->source_url = GURL("http://example.com/a");
  first_navigation->original_request_url = GURL("http://example.com/b");
  first_navigation->last_updated = one_hour_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
  first_navigation->source_tab_id = source_tab;
  first_navigation->target_tab_id = target_tab;
  first_navigation->has_committed = false;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->original_request_url = GURL("http://example.com/c");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  second_navigation->source_tab_id = target_tab;
  second_navigation->target_tab_id = target_tab;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  ReferrerChain referrer_chain;
  navigation_observer_manager_->IdentifyReferrerChainByEventURL(
      GURL("http://example.com/c"), SessionID::InvalidValue(), 10,
      &referrer_chain);

  ASSERT_EQ(1, referrer_chain.size());

  EXPECT_EQ("http://example.com/c", referrer_chain[0].url());
  EXPECT_EQ("http://example.com/a", referrer_chain[0].referrer_url());
  EXPECT_TRUE(referrer_chain[0].is_retargeting());
}

}  // namespace safe_browsing
