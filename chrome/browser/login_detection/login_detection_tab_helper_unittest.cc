// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_tab_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"

namespace login_detection {

class LoginDetectionTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kLoginDetection);
    ChromeRenderViewHostTestHarness::SetUp();
    ResetHistogramTester();
    LoginDetectionTabHelper::MaybeCreateForWebContents(web_contents());
  }

  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  base::HistogramTester* histogram_tester() const {
    return histogram_tester_.get();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(LoginDetectionTabHelperTest, NoLogin) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin, 1);
}

TEST_F(LoginDetectionTabHelperTest, SimpleOAuthLogin) {
  // OAuth login start
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin, 1);

  // OAuth login complete
  ResetHistogramTester();
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kOauthFirstTimeLoginFlow, 1);

  // Subsequent navigations to OAuth signed-in site.
  ResetHistogramTester();
  NavigateAndCommit(GURL("https://images.foo.com/page.html"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kOauthLogin, 1);
}

TEST_F(LoginDetectionTabHelperTest, NavigationToOAuthLoggedInSite) {
  // Trigger OAuth login so that the site is saved in prefs.
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));

  // Subsequent navigations to OAuth signed-in site.
  ResetHistogramTester();
  NavigateAndCommit(GURL("https://images.foo.com/page.html"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kOauthLogin, 1);

  // Navigation to a non logged-in site.
  ResetHistogramTester();
  NavigateAndCommit(GURL("https://bar.com/page.html"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin, 1);
}

TEST_F(LoginDetectionTabHelperTest, OAuthLoginViaRedirect) {
  // OAuth login start and complete via redirects.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://foo.com/oauth_signin"), web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  simulator->Redirect(GURL("https://oauth.com/authenticate?client_id=123"));
  simulator->Redirect(GURL("https://oauth.com/user_login"));
  simulator->Redirect(GURL("https://oauth.com/?username=user&password=123"));
  simulator->Redirect(GURL("https://foo.com/redirect?code=secret"));
  simulator->Commit();

  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kOauthFirstTimeLoginFlow, 1);

  // Subsequent navigations to OAuth signed-in site.
  ResetHistogramTester();
  NavigateAndCommit(GURL("https://images.foo.com/page.html"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kOauthLogin, 1);
}

// Test that OAuth login is not detected when there are intermediate navigations
// to other sites.
TEST_F(LoginDetectionTabHelperTest, InvalidOAuthLogins) {
  // OAuth login start
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin, 1);

  // Invalid intermediate navigation
  ResetHistogramTester();
  NavigateAndCommit(GURL("https://bar.com/page.html"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin, 1);

  // OAuth login complete will not be detected
  ResetHistogramTester();
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin, 1);

  // Subsequent navigations is also no login.
  ResetHistogramTester();
  NavigateAndCommit(GURL("https://images.foo.com/page.html"));
  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin, 1);
}

// Test that OAuth login is not detected when there are intermediate redirect
// navigations to other sites.
TEST_F(LoginDetectionTabHelperTest, InvalidOAuthLoginsWithRedirect) {
  // OAuth login start and complete via redirects.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://foo.com/oauth_signin"), web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  simulator->Redirect(GURL("https://oauth.com/authenticate?client_id=123"));
  simulator->Redirect(GURL("https://oauth.com/user_login"));
  simulator->Redirect(GURL("https://oauth.com/?username=user&password=123"));
  simulator->Redirect(GURL("https://bar.com/page.html"));
  simulator->Redirect(GURL("https://foo.com/redirect?code=secret"));
  simulator->Commit();

  histogram_tester()->ExpectUniqueSample(
      "Login.PageLoad.DetectionType",
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin, 1);
}

}  // namespace login_detection
