// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
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
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::MockNavigationHandle;
using content::NavigationThrottle;

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace browser_switcher {

namespace {

class MockBrowserSwitcherSitelist : public BrowserSwitcherSitelist {
 public:
  MockBrowserSwitcherSitelist() = default;
  ~MockBrowserSwitcherSitelist() override = default;

  MOCK_CONST_METHOD1(GetDecision, Decision(const GURL&));
  MOCK_METHOD1(SetIeemSitelist, void(RawRuleSet&&));
  MOCK_METHOD1(SetExternalSitelist, void(RawRuleSet&&));
  MOCK_METHOD1(SetExternalGreylist, void(RawRuleSet&&));
  MOCK_CONST_METHOD0(GetIeemSitelist, const RuleSet*());
  MOCK_CONST_METHOD0(GetExternalSitelist, const RuleSet*());
  MOCK_CONST_METHOD0(GetExternalGreylist, const RuleSet*());
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

  void TearDown() override {
    sitelist_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<MockNavigationHandle> CreateMockNavigationHandle(
      const GURL& url) {
    return std::make_unique<NiceMock<MockNavigationHandle>>(url, main_rfh());
  }

  void CreateNavigationThrottle(content::NavigationThrottleRegistry& registry) {
    BrowserSwitcherNavigationThrottle::MaybeCreateAndAdd(registry);
  }

  MockBrowserSwitcherSitelist* sitelist() { return sitelist_; }

  Decision stay() { return {kStay, kDefault, nullptr}; }

  Decision go() { return {kGo, kSitelist, bogus_rule_.get()}; }

 private:
  raw_ptr<MockBrowserSwitcherSitelist> sitelist_ = nullptr;

  std::unique_ptr<Rule> bogus_rule_ =
      CanonicalizeRule("//example.com/", ParsingMode::kDefault);
};

TEST_F(BrowserSwitcherNavigationThrottleTest, ShouldIgnoreNavigation) {
  EXPECT_CALL(*sitelist(), GetDecision(_)).WillOnce(Return(stay()));
  std::unique_ptr<MockNavigationHandle> handle =
      CreateMockNavigationHandle(GURL("https://example.com/"));
  content::MockNavigationThrottleRegistry registry(
      handle.get(),
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  CreateNavigationThrottle(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  EXPECT_EQ(NavigationThrottle::PROCEED,
            registry.throttles()[0]->WillStartRequest());
}

TEST_F(BrowserSwitcherNavigationThrottleTest, LaunchesOnStartRequest) {
  EXPECT_CALL(*sitelist(), GetDecision(_)).WillOnce(Return(go()));
  std::unique_ptr<MockNavigationHandle> handle =
      CreateMockNavigationHandle(GURL("https://example.com/"));
  content::MockNavigationThrottleRegistry registry(
      handle.get(),
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  CreateNavigationThrottle(registry);
  ASSERT_EQ(1u, registry.throttles().size());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            registry.throttles()[0]->WillStartRequest());
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowserSwitcherNavigationThrottleTest, LaunchesOnRedirectRequest) {
  EXPECT_CALL(*sitelist(), GetDecision(_))
      .WillOnce(Return(stay()))
      .WillOnce(Return(go()));
  std::unique_ptr<MockNavigationHandle> handle =
      CreateMockNavigationHandle(GURL("https://yahoo.com/"));
  ON_CALL(*handle, WasServerRedirect()).WillByDefault(Return(false));

  content::MockNavigationThrottleRegistry registry(
      handle.get(),
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  CreateNavigationThrottle(registry);
  EXPECT_EQ(1u, registry.throttles().size());
  EXPECT_EQ(NavigationThrottle::PROCEED,
            registry.throttles()[0]->WillStartRequest());

  ON_CALL(*handle, WasServerRedirect()).WillByDefault(Return(true));

  handle->set_url(GURL("https://bing.com/"));
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            registry.throttles()[0]->WillRedirectRequest());
  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowserSwitcherNavigationThrottleTest,
       DoNotCreateThrottleOnNonPrimaryMainFrame) {
  std::unique_ptr<MockNavigationHandle> handle =
      CreateMockNavigationHandle(GURL("https://fencedframe.com/"));
  handle->set_has_committed(true);
  handle->set_is_in_primary_main_frame(false);

  content::MockNavigationThrottleRegistry registry(
      handle.get(),
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  CreateNavigationThrottle(registry);
  EXPECT_EQ(0u, registry.throttles().size());

  handle->set_is_in_primary_main_frame(true);
  CreateNavigationThrottle(registry);
  EXPECT_EQ(1u, registry.throttles().size());
}

}  // namespace browser_switcher
