// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_switcher/alternative_browser_launcher.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::NavigationThrottle;
using content::NavigationHandle;

using ::testing::Return;
using ::testing::_;

namespace browser_switcher {

namespace {

class MockAlternativeBrowserLauncher : public AlternativeBrowserLauncher {
 public:
  MockAlternativeBrowserLauncher() {}
  ~MockAlternativeBrowserLauncher() override = default;

  MOCK_CONST_METHOD1(Launch, bool(const GURL&));
};

class MockBrowserSwitcherSitelist : public BrowserSwitcherSitelist {
 public:
  MockBrowserSwitcherSitelist() = default;
  ~MockBrowserSwitcherSitelist() override = default;

  MOCK_CONST_METHOD1(ShouldSwitch, bool(const GURL&));
  MOCK_METHOD1(SetIeemSitelist, void(ParsedXml&&));
  MOCK_METHOD1(SetExternalSitelist, void(ParsedXml&&));
};

class MockBrowserClient : public content::ContentBrowserClient {
 public:
  MockBrowserClient() = default;
  ~MockBrowserClient() override = default;

  // Only construct a BrowserSwitcherNavigationThrottle so that we can test it
  // in isolation.
  std::vector<std::unique_ptr<NavigationThrottle>> CreateThrottlesForNavigation(
      NavigationHandle* handle) override {
    std::vector<std::unique_ptr<NavigationThrottle>> throttles;
    throttles.push_back(
        BrowserSwitcherNavigationThrottle::MaybeCreateThrottleFor(handle));
    return throttles;
  }
};

}  // namespace

class BrowserSwitcherNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  BrowserSwitcherNavigationThrottleTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    original_client_ = content::SetBrowserClientForTesting(&client_);

    BrowserSwitcherService* service =
        BrowserSwitcherServiceFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext());

    std::unique_ptr<MockAlternativeBrowserLauncher> launcher =
        std::make_unique<MockAlternativeBrowserLauncher>();
    launcher_ = launcher.get();
    service->SetLauncherForTesting(std::move(launcher));

    std::unique_ptr<MockBrowserSwitcherSitelist> sitelist =
        std::make_unique<MockBrowserSwitcherSitelist>();
    sitelist_ = sitelist.get();
    service->SetSitelistForTesting(std::move(sitelist));
  }

  void TearDown() override {
    content::SetBrowserClientForTesting(original_client_);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<NavigationHandle> CreateNavigationHandle(const GURL& url) {
    return NavigationHandle::CreateNavigationHandleForTesting(url, main_rfh());
  }

  MockAlternativeBrowserLauncher* launcher() { return launcher_; }
  MockBrowserSwitcherSitelist* sitelist() { return sitelist_; }

 private:
  MockBrowserClient client_;
  content::ContentBrowserClient* original_client_;

  MockAlternativeBrowserLauncher* launcher_;
  MockBrowserSwitcherSitelist* sitelist_;
};

TEST_F(BrowserSwitcherNavigationThrottleTest, ShouldIgnoreNavigation) {
  EXPECT_CALL(*sitelist(), ShouldSwitch(_)).WillOnce(Return(false));
  std::unique_ptr<NavigationHandle> handle =
      CreateNavigationHandle(GURL("https://example.com/"));
  EXPECT_EQ(NavigationThrottle::PROCEED,
            handle->CallWillStartRequestForTesting());
}

TEST_F(BrowserSwitcherNavigationThrottleTest, LaunchesOnStartRequest) {
  EXPECT_CALL(*sitelist(), ShouldSwitch(_)).WillOnce(Return(true));
  EXPECT_CALL(*launcher(), Launch(_)).WillOnce(Return(true));
  std::unique_ptr<NavigationHandle> handle =
      CreateNavigationHandle(GURL("https://example.com/"));
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            handle->CallWillStartRequestForTesting());
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowserSwitcherNavigationThrottleTest, LaunchesOnRedirectRequest) {
  EXPECT_CALL(*sitelist(), ShouldSwitch(_))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*launcher(), Launch(_)).WillOnce(Return(true));
  std::unique_ptr<NavigationHandle> handle =
      CreateNavigationHandle(GURL("https://yahoo.com/"));
  EXPECT_EQ(NavigationThrottle::PROCEED,
            handle->CallWillStartRequestForTesting());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            handle->CallWillRedirectRequestForTesting(
                GURL("https://bing.com/"), /* new_method_is_post */ false,
                GURL("https://yahoo.com/"),
                /* new_is_external_protocol */ false));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowserSwitcherNavigationThrottleTest, FallsBackToLoadingNormally) {
  EXPECT_CALL(*sitelist(), ShouldSwitch(_)).WillOnce(Return(true));
  EXPECT_CALL(*launcher(), Launch(_)).WillOnce(Return(false));
  std::unique_ptr<NavigationHandle> handle =
      CreateNavigationHandle(GURL("https://yahoo.com/"));
  EXPECT_EQ(NavigationThrottle::PROCEED,
            handle->CallWillStartRequestForTesting());
}

}  // namespace browser_switcher
