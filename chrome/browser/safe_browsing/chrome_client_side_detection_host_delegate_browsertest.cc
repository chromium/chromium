// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ChromeClientSideDetectionHostDelegateTest : public InProcessBrowserTest {
 public:
  ChromeClientSideDetectionHostDelegateTest() = default;

  ChromeClientSideDetectionHostDelegateTest(
      const ChromeClientSideDetectionHostDelegateTest&) = delete;
  ChromeClientSideDetectionHostDelegateTest& operator=(
      const ChromeClientSideDetectionHostDelegateTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://foo/0"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    Profile* profile = browser()->profile();
    navigation_observer_manager_ =
        SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
            profile);
    ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents());
    navigation_observer_ = std::make_unique<SafeBrowsingNavigationObserver>(
        browser()->tab_strip_model()->GetActiveWebContents(),
        HostContentSettingsMapFactory::GetForProfile(profile),
        navigation_observer_manager_);
  }

  void TearDownOnMainThread() override {
    navigation_observer_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  NavigationEventList* navigation_event_list() {
    return navigation_observer_manager_->navigation_event_list();
  }

 protected:
  raw_ptr<SafeBrowsingNavigationObserverManager, DanglingUntriaged>
      navigation_observer_manager_;
  std::unique_ptr<SafeBrowsingNavigationObserver> navigation_observer_;
};

IN_PROC_BROWSER_TEST_F(ChromeClientSideDetectionHostDelegateTest,
                       GetReferrerChain) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);

  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://a.com/");
  first_navigation->last_updated = one_second_ago;
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

  std::unique_ptr<ChromeClientSideDetectionHostDelegate> csd_host_delegate =
      std::make_unique<ChromeClientSideDetectionHostDelegate>(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host_delegate->SetNavigationObserverManagerForTesting(
      navigation_observer_manager_);
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  csd_host_delegate->AddReferrerChain(verdict.get(), GURL("http://b.com/"),
                                      content::GlobalRenderFrameHostId());
  ReferrerChain referrer_chain = verdict->referrer_chain();

  EXPECT_EQ(2, referrer_chain.size());

  EXPECT_EQ("http://b.com/", referrer_chain[0].url());
  EXPECT_EQ("http://a.com/", referrer_chain[0].referrer_url());
}

IN_PROC_BROWSER_TEST_F(ChromeClientSideDetectionHostDelegateTest,
                       NoNavigationObserverManager) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);

  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://a.com/");
  first_navigation->last_updated = one_second_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<ChromeClientSideDetectionHostDelegate> csd_host_delegate =
      std::make_unique<ChromeClientSideDetectionHostDelegate>(
          browser()->tab_strip_model()->GetActiveWebContents());
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  csd_host_delegate->AddReferrerChain(verdict.get(), GURL("http://b.com/"),
                                      content::GlobalRenderFrameHostId());
  ReferrerChain referrer_chain = verdict->referrer_chain();

  EXPECT_EQ(0, referrer_chain.size());
}
}  // namespace safe_browsing
