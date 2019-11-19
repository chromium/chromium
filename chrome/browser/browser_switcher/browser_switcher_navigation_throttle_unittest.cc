// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::MockNavigationHandle;
using content::NavigationThrottle;

using ::testing::Return;
using ::testing::_;

namespace browser_switcher {

namespace {

class MockBrowserSwitcherSitelist : public BrowserSwitcherSitelist {
 public:
  MockBrowserSwitcherSitelist() = default;
  ~MockBrowserSwitcherSitelist() override = default;

  MOCK_CONST_METHOD1(GetDecision, Decision(const GURL&));
  MOCK_METHOD1(SetIeemSitelist, void(ParsedXml&&));
  MOCK_METHOD1(SetExternalSitelist, void(ParsedXml&&));
  MOCK_METHOD1(SetExternalGreylist, void(ParsedXml&&));
  MOCK_CONST_METHOD0(GetIeemSitelist, const RuleSet*());
  MOCK_CONST_METHOD0(GetExternalSitelist, const RuleSet*());
};

}  // namespace

class BrowserSwitcherNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  BrowserSwitcherNavigationThrottleTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    BrowserSwitcherService* service =
        BrowserSwitcherServiceFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext());

    std::unique_ptr<MockBrowserSwitcherSitelist> sitelist =
        std::make_unique<MockBrowserSwitcherSitelist>();
    sitelist_ = sitelist.get();
    service->SetSitelistForTesting(std::move(sitelist));
  }

  std::unique_ptr<MockNavigationHandle> CreateMockNavigationHandle(
      const GURL& url) {
    return std::make_unique<MockNavigationHandle>(url, main_rfh());
  }

  std::unique_ptr<NavigationThrottle> CreateNavigationThrottle(
      content::NavigationHandle* handle) {
    return BrowserSwitcherNavigationThrottle::MaybeCreateThrottleFor(handle);
  }

  MockBrowserSwitcherSitelist* sitelist() { return sitelist_; }

 private:
  MockBrowserSwitcherSitelist* sitelist_;
};

Decision STAY = {kStay, kDefault, ""};
Decision GO = {kGo, kSitelist, "example.com"};

TEST_F(BrowserSwitcherNavigationThrottleTest, ShouldIgnoreNavigation) {
  EXPECT_CALL(*sitelist(), GetDecision(_)).WillOnce(Return(STAY));
  std::unique_ptr<MockNavigationHandle> handle =
      CreateMockNavigationHandle(GURL("https://example.com/"));
  std::unique_ptr<NavigationThrottle> throttle =
      CreateNavigationThrottle(handle.get());
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest());
}

TEST_F(BrowserSwitcherNavigationThrottleTest, LaunchesOnStartRequest) {
  EXPECT_CALL(*sitelist(), GetDecision(_)).WillOnce(Return(GO));
  std::unique_ptr<MockNavigationHandle> handle =
      CreateMockNavigationHandle(GURL("https://example.com/"));
  std::unique_ptr<NavigationThrottle> throttle =
      CreateNavigationThrottle(handle.get());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest());
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowserSwitcherNavigationThrottleTest, LaunchesOnRedirectRequest) {
  EXPECT_CALL(*sitelist(), GetDecision(_))
      .WillOnce(Return(STAY))
      .WillOnce(Return(GO));
  std::unique_ptr<MockNavigationHandle> handle =
      CreateMockNavigationHandle(GURL("https://yahoo.com/"));
  std::unique_ptr<NavigationThrottle> throttle =
      CreateNavigationThrottle(handle.get());
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest());
  handle->set_url(GURL("https://bing.com/"));
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillRedirectRequest());
  base::RunLoop().RunUntilIdle();
}

}  // namespace browser_switcher
