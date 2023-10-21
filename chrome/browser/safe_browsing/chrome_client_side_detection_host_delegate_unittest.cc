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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ChromeClientSideDetectionHostDelegateTest
    : public BrowserWithTestWindowTest {
 public:
  ChromeClientSideDetectionHostDelegateTest() = default;

  ChromeClientSideDetectionHostDelegateTest(
      const ChromeClientSideDetectionHostDelegateTest&) = delete;
  ChromeClientSideDetectionHostDelegateTest& operator=(
      const ChromeClientSideDetectionHostDelegateTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("http://foo/0"));
    Profile* profile = Profile::FromBrowserContext(
        browser()->tab_strip_model()->GetWebContentsAt(0)->GetBrowserContext());
    navigation_observer_manager_ =
        SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
            profile);
    navigation_observer_ = std::make_unique<SafeBrowsingNavigationObserver>(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        HostContentSettingsMapFactory::GetForProfile(profile),
        navigation_observer_manager_);
  }

  void TearDown() override {
    navigation_observer_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  NavigationEventList* navigation_event_list() {
    return navigation_observer_manager_->navigation_event_list();
  }

 protected:
  raw_ptr<SafeBrowsingNavigationObserverManager, DanglingUntriaged>
      navigation_observer_manager_;
  std::unique_ptr<SafeBrowsingNavigationObserver> navigation_observer_;
};

TEST_F(ChromeClientSideDetectionHostDelegateTest, GetReferrerChain) {
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
          browser()->tab_strip_model()->GetWebContentsAt(0));
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

TEST_F(ChromeClientSideDetectionHostDelegateTest, NoNavigationObserverManager) {
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
          browser()->tab_strip_model()->GetWebContentsAt(0));
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  csd_host_delegate->AddReferrerChain(verdict.get(), GURL("http://b.com/"),
                                      content::GlobalRenderFrameHostId());
  ReferrerChain referrer_chain = verdict->referrer_chain();

  EXPECT_EQ(0, referrer_chain.size());
}
}  // namespace safe_browsing
