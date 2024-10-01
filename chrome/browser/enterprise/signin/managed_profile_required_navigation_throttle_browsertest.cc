// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/managed_profile_required_navigation_throttle.h"

#include <memory>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_controller_client.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_page.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
bool Equals(content::NavigationThrottle::ThrottleCheckResult& expected,
            content::NavigationThrottle::ThrottleCheckResult&& actual) {
  return expected.action() == actual.action() &&
         expected.net_error_code() == actual.net_error_code() &&
         expected.error_page_content() == actual.error_page_content();
}
}  // namespace

class ManagedProfileRequiredNavigationThrottleFeatureDisabledTest
    : public InProcessBrowserTest {
 public:
  ManagedProfileRequiredNavigationThrottleFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(
        features::kEnterpriseUpdatedProfileCreationScreen);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ManagedProfileRequiredNavigationThrottleFeatureDisabledTest,
    NoThrottle) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::MockNavigationHandle mock_nav_handle(web_contents);

  EXPECT_FALSE(ManagedProfileRequiredNavigationThrottle::MaybeCreateThrottleFor(
      &mock_nav_handle));
}

class ManagedProfileRequiredNavigationThrottleTest
    : public InProcessBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kEnterpriseUpdatedProfileCreationScreen};
};

IN_PROC_BROWSER_TEST_F(ManagedProfileRequiredNavigationThrottleTest,
                       ProceedsWhenNoForcedInterception) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::MockNavigationHandle mock_nav_handle(web_contents);

  auto throttle =
      ManagedProfileRequiredNavigationThrottle::MaybeCreateThrottleFor(
          &mock_nav_handle);
  ASSERT_TRUE(throttle);
  EXPECT_EQ(content::NavigationThrottle::ThrottleAction::PROCEED,
            throttle->WillStartRequest());
  EXPECT_EQ(content::NavigationThrottle::ThrottleAction::PROCEED,
            throttle->WillRedirectRequest());
  EXPECT_EQ(content::NavigationThrottle::ThrottleAction::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_EQ(content::NavigationThrottle::ThrottleAction::PROCEED,
            throttle->WillFailRequest());
}

IN_PROC_BROWSER_TEST_F(ManagedProfileRequiredNavigationThrottleTest,
                       CancelsWithInterstitialWhenForcedInterception) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::MockNavigationHandle mock_nav_handle(web_contents);

  auto* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(browser()->profile());

  interceptor->Reset();
  interceptor->state_->interception_type_ =
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced;
  interceptor->state_->web_contents_ = web_contents->GetWeakPtr();

  auto managed_profile_required = std::make_unique<ManagedProfileRequiredPage>(
      mock_nav_handle.GetWebContents(), mock_nav_handle.GetURL(), u"Manager",
      base::UTF8ToUTF16(interceptor->intercepted_account_info().email),
      std::make_unique<ManagedProfileRequiredControllerClient>(
          mock_nav_handle.GetWebContents(), mock_nav_handle.GetURL()));
  std::string error_page_content = managed_profile_required->GetHTMLContents();
  content::NavigationThrottle::ThrottleCheckResult expected_result(
      content::NavigationThrottle::ThrottleAction::CANCEL,
      net::ERR_BLOCKED_BY_CLIENT, error_page_content);

  auto throttle =
      ManagedProfileRequiredNavigationThrottle::MaybeCreateThrottleFor(
          &mock_nav_handle);
  ASSERT_TRUE(throttle);
  throttle->SetManagerForTesting(u"Manager");
  EXPECT_TRUE(Equals(expected_result, throttle->WillStartRequest()));
  EXPECT_TRUE(Equals(expected_result, throttle->WillRedirectRequest()));
  EXPECT_TRUE(Equals(expected_result, throttle->WillProcessResponse()));
  EXPECT_TRUE(Equals(expected_result, throttle->WillFailRequest()));
}
