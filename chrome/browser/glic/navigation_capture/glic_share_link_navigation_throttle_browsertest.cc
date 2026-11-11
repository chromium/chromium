// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/navigation_capture/glic_share_link_navigation_throttle.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

class TestGlicShareLinkNavigationThrottle
    : public GlicShareLinkNavigationThrottle {
 public:
  explicit TestGlicShareLinkNavigationThrottle(
      content::NavigationThrottleRegistry& registry)
      : GlicShareLinkNavigationThrottle(registry) {}

  int invoke_count() const { return invoke_count_; }

 protected:
  void InvokeGlic() override { ++invoke_count_; }

 private:
  int invoke_count_ = 0;
};

class GlicShareLinkNavigationThrottleBrowserTest : public InProcessBrowserTest {
 public:
  GlicShareLinkNavigationThrottleBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicConversationSharing,
          {{features::kGlicConversationSharingCaptureUrls.name,
            "https://example.com/share|https://other.example.com/conv/"}}},
         {features::kGlic, {}}},
        {});
    glic_test_environment_.SetForceSigninAndModelExecutionCapability(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGlicDev);
  }

 protected:
  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool TryCreateThrottle(const GURL& url,
                         bool is_in_primary_main_frame = true) {
    content::MockNavigationHandle handle(
        url, active_web_contents()->GetPrimaryMainFrame());
    handle.set_is_in_primary_main_frame(is_in_primary_main_frame);
    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    GlicShareLinkNavigationThrottle::MaybeCreateAndAdd(registry);
    if (registry.throttles().empty()) {
      return false;
    }
    EXPECT_EQ(1u, registry.throttles().size());
    return true;
  }

  bool WillStartRequestMatches(const GURL& url) {
    EXPECT_TRUE(TryCreateThrottle(url));
    content::MockNavigationHandle handle(
        url, active_web_contents()->GetPrimaryMainFrame());
    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    TestGlicShareLinkNavigationThrottle throttle(registry);
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              throttle.WillStartRequest().action());
    return throttle.invoke_count() == 1;
  }

 private:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicShareLinkNavigationThrottleBrowserTest,
                       MatchesConfiguredUrls) {
  // First pattern: https://example.com/share
  EXPECT_TRUE(WillStartRequestMatches(GURL("https://example.com/share")));
  EXPECT_TRUE(WillStartRequestMatches(GURL("https://example.com/share/123")));
  EXPECT_TRUE(
      WillStartRequestMatches(GURL("https://example.com/share?id=123")));

  // Second pattern: https://other.example.com/conv/
  EXPECT_TRUE(WillStartRequestMatches(GURL("https://other.example.com/conv/")));
  EXPECT_TRUE(
      WillStartRequestMatches(GURL("https://other.example.com/conv/abc")));
}

IN_PROC_BROWSER_TEST_F(GlicShareLinkNavigationThrottleBrowserTest,
                       DoesNotMatchDifferentOriginOrNonPrefixPath) {
  // Different origin (host, scheme, port).
  EXPECT_FALSE(
      WillStartRequestMatches(GURL("https://wrong.example.com/share/123")));
  EXPECT_FALSE(WillStartRequestMatches(GURL("http://example.com/share/123")));
  EXPECT_FALSE(
      WillStartRequestMatches(GURL("https://example.com:8443/share/123")));

  // Path does not have pattern path as prefix.
  EXPECT_FALSE(WillStartRequestMatches(GURL("https://example.com/other")));
  EXPECT_FALSE(WillStartRequestMatches(GURL("https://other.example.com/conv")));
}

IN_PROC_BROWSER_TEST_F(GlicShareLinkNavigationThrottleBrowserTest,
                       ChecksUrlOnRedirect) {
  content::MockNavigationHandle handle(
      GURL("https://example.com/other"),
      active_web_contents()->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  GlicShareLinkNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_EQ(1u, registry.throttles().size());

  TestGlicShareLinkNavigationThrottle throttle(registry);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle.WillStartRequest().action());
  EXPECT_EQ(0, throttle.invoke_count());

  // Redirect to a matching URL.
  handle.set_url(GURL("https://example.com/share/123"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle.WillRedirectRequest().action());
  EXPECT_EQ(1, throttle.invoke_count());

  // Redirect to a non-matching URL.
  handle.set_url(GURL("https://example.com/login"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle.WillRedirectRequest().action());
  EXPECT_EQ(1, throttle.invoke_count());
}

IN_PROC_BROWSER_TEST_F(GlicShareLinkNavigationThrottleBrowserTest,
                       DoesNotRegisterForNonPrimaryMainFrame) {
  EXPECT_FALSE(TryCreateThrottle(GURL("https://example.com/share/123"),
                                 /*is_in_primary_main_frame=*/false));
}

IN_PROC_BROWSER_TEST_F(GlicShareLinkNavigationThrottleBrowserTest,
                       DoesNotRegisterWhenDisabledByPolicy) {
  browser()->GetProfile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  EXPECT_FALSE(TryCreateThrottle(GURL("https://example.com/share/123")));
}

IN_PROC_BROWSER_TEST_F(GlicShareLinkNavigationThrottleBrowserTest,
                       DoesNotRegisterWhenSystemRequirementNotMet) {
  GlicEnabling::SetSystemRequirementMetForTesting(false);
  EXPECT_FALSE(TryCreateThrottle(GURL("https://example.com/share/123")));
  GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
}

class GlicShareLinkNavigationThrottleDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  GlicShareLinkNavigationThrottleDisabledBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(GlicShareLinkNavigationThrottleDisabledBrowserTest,
                       DisabledByDefault) {
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(features::kGlicConversationSharing));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::MockNavigationHandle handle(GURL("https://example.com/share/123"),
                                       web_contents->GetPrimaryMainFrame());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  GlicShareLinkNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_TRUE(registry.throttles().empty());
}

}  // namespace
}  // namespace glic
