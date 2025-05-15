// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/webstore/chrome_web_store_navigation_throttle.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/web_contents_tester.h"

using content::NavigationThrottle;
using ::testing::_;

namespace enterprise_webstore {

namespace {
constexpr char kBrowserDmTokenHeader[] = "X-Browser-Dm-Token";
constexpr char kDeviceIdHeader[] = "X-Client-Device-Id";
constexpr char kChromeWebStoreUrl[] = "https://chromewebstore.google.com/";
}  // namespace

class ChromeWebStoreNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeWebStoreNavigationThrottleTest() = default;

  content::NavigationThrottle* CreateThrottle(
      content::MockNavigationThrottleRegistry& registry) {
    registry.AddThrottle(
        std::make_unique<ChromeWebStoreNavigationThrottle>(registry));
    CHECK_EQ(registry.throttles().size(), 1u);
    return registry.throttles().back().get();
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    browser_dm_token_storage_.SetClientId("client_id");
    browser_dm_token_storage_.SetEnrollmentToken("enrollment_token");
    browser_dm_token_storage_.SetDMToken("dm_token");
  }

  policy::FakeBrowserDMTokenStorage* browser_dm_token_storage() {
    return &browser_dm_token_storage_;
  }

 private:
  policy::FakeBrowserDMTokenStorage browser_dm_token_storage_;
};  // namespace enterprise_webstore

// Navigating and redirecting to Chrome Web Store, set headers.
TEST_F(ChromeWebStoreNavigationThrottleTest, ChromeWebStoreSetHeaders) {
  content::MockNavigationHandle test_handle(GURL(kChromeWebStoreUrl),
                                            main_rfh());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  raw_ptr<content::NavigationThrottle> throttle = CreateThrottle(test_registry);

  EXPECT_CALL(test_handle, SetRequestHeader(kBrowserDmTokenHeader, "dm_token"));
  EXPECT_CALL(test_handle, SetRequestHeader(kDeviceIdHeader, "client_id"));

  EXPECT_STREQ(throttle->GetNameForLogging(),
               "ChromeWebStoreNavigationThrottle");
  EXPECT_EQ(throttle->WillStartRequest().action(), NavigationThrottle::PROCEED);

  test_handle.set_url(GURL(kChromeWebStoreUrl));

  EXPECT_CALL(test_handle, SetRequestHeader(kBrowserDmTokenHeader, "dm_token"));
  EXPECT_CALL(test_handle, SetRequestHeader(kDeviceIdHeader, "client_id"));
  EXPECT_EQ(throttle->WillRedirectRequest().action(),
            NavigationThrottle::PROCEED);
}

// Navigating and redirecting to non-Chrome Web Store, do not set headers.
TEST_F(ChromeWebStoreNavigationThrottleTest, NonChromeWebStoreDoNotSetHeaders) {
  content::MockNavigationHandle test_handle(
      GURL("https://www.notchromewebstore.test/"), main_rfh());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  raw_ptr<content::NavigationThrottle> throttle = CreateThrottle(test_registry);

  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);
  EXPECT_STREQ(throttle->GetNameForLogging(),
               "ChromeWebStoreNavigationThrottle");
  EXPECT_EQ(throttle->WillStartRequest().action(), NavigationThrottle::PROCEED);

  test_handle.set_url(GURL("https://www.anothernotchromewebstore.test/"));

  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);
  EXPECT_EQ(throttle->WillRedirectRequest().action(),
            NavigationThrottle::PROCEED);
}

// Navigating to Chrome Web Store on incognito mode, do not set headers.
TEST_F(ChromeWebStoreNavigationThrottleTest, IncognitoMode) {
  TestingProfile::Builder profile_builder;
  TestingProfile* incognito_profile = profile_builder.BuildIncognito(profile());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(incognito_profile,
                                                        nullptr);
  content::MockNavigationHandle test_handle(
      GURL(kChromeWebStoreUrl), web_contents->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  raw_ptr<content::NavigationThrottle> throttle = CreateThrottle(test_registry);

  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);
  EXPECT_STREQ(throttle->GetNameForLogging(),
               "ChromeWebStoreNavigationThrottle");
  EXPECT_EQ(throttle->WillStartRequest().action(), NavigationThrottle::PROCEED);
}

// Navigating to Chrome Web Store on guest profile, do not set headers.
TEST_F(ChromeWebStoreNavigationThrottleTest, GuestProfile) {
  TestingProfile::Builder guest_profile_builder;
  guest_profile_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = guest_profile_builder.Build();
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(guest_profile.get(),
                                                        nullptr);
  content::MockNavigationHandle test_handle(
      GURL(kChromeWebStoreUrl), web_contents->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  raw_ptr<content::NavigationThrottle> throttle = CreateThrottle(test_registry);

  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);
  EXPECT_STREQ(throttle->GetNameForLogging(),
               "ChromeWebStoreNavigationThrottle");
  EXPECT_EQ(throttle->WillStartRequest().action(), NavigationThrottle::PROCEED);
}

// Invalid DM token, do not set headers.
TEST_F(ChromeWebStoreNavigationThrottleTest, InvalidDmToken) {
  browser_dm_token_storage()->SetDMToken("");
  content::MockNavigationHandle test_handle(GURL(kChromeWebStoreUrl),
                                            main_rfh());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  raw_ptr<content::NavigationThrottle> throttle = CreateThrottle(test_registry);

  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);
  EXPECT_STREQ(throttle->GetNameForLogging(),
               "ChromeWebStoreNavigationThrottle");
  EXPECT_EQ(throttle->WillStartRequest().action(), NavigationThrottle::PROCEED);
}

// Redirecting from non-Chrome Web Store to Chrome Web Store, set headers.
TEST_F(ChromeWebStoreNavigationThrottleTest,
       RedirectToChromeWebStoreSetHeaders) {
  content::MockNavigationHandle test_handle(
      GURL("https://www.notchromewebstore.test/"), main_rfh());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  raw_ptr<content::NavigationThrottle> throttle = CreateThrottle(test_registry);
  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);
  EXPECT_STREQ(throttle->GetNameForLogging(),
               "ChromeWebStoreNavigationThrottle");
  EXPECT_EQ(throttle->WillStartRequest().action(), NavigationThrottle::PROCEED);

  test_handle.set_url(GURL(kChromeWebStoreUrl));

  EXPECT_CALL(test_handle, SetRequestHeader(kBrowserDmTokenHeader, "dm_token"));
  EXPECT_CALL(test_handle, SetRequestHeader(kDeviceIdHeader, "client_id"));
  EXPECT_EQ(throttle->WillRedirectRequest().action(),
            NavigationThrottle::PROCEED);
}

// Redirecting from Chrome Web Store to non-Chrome Web Store, do not set
// headers.
TEST_F(ChromeWebStoreNavigationThrottleTest,
       RedirectToNonChromeWebStoreDoNotSetHeaders) {
  content::MockNavigationHandle test_handle(GURL(kChromeWebStoreUrl),
                                            main_rfh());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  raw_ptr<content::NavigationThrottle> throttle = CreateThrottle(test_registry);
  EXPECT_CALL(test_handle, SetRequestHeader(kBrowserDmTokenHeader, "dm_token"));
  EXPECT_CALL(test_handle, SetRequestHeader(kDeviceIdHeader, "client_id"));
  EXPECT_STREQ(throttle->GetNameForLogging(),
               "ChromeWebStoreNavigationThrottle");
  EXPECT_EQ(throttle->WillStartRequest().action(), NavigationThrottle::PROCEED);

  test_handle.set_url(GURL("https://www.notchromewebstore.test/"));

  EXPECT_CALL(test_handle, SetRequestHeader(_, _)).Times(0);
  EXPECT_EQ(throttle->WillRedirectRequest().action(),
            NavigationThrottle::PROCEED);
}

}  // namespace enterprise_webstore
