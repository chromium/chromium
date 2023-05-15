// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_tab_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"

namespace login_detection {

class LoginDetectionTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  LoginDetectionTabHelperTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kLoginDetection);
    ChromeRenderViewHostTestHarness::SetUp();
    ResetMetricsTesters();
    LoginDetectionTabHelper::MaybeCreateForWebContents(web_contents());
  }

  void ResetMetricsTesters() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Verifies the UMA metrics for the given login detection type to be
  // recorded.
  void VerifyLoginDetectionTypeMetrics(LoginDetectionType type) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          1);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(LoginDetectionTabHelperTest, NoLogin) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
}

TEST_F(LoginDetectionTabHelperTest, SimpleOAuthLogin) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // OAuth login start
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);

  // OAuth login complete
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow);
}

TEST_F(LoginDetectionTabHelperTest, NavigationToOAuthLoggedInSite) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // Trigger OAuth login so that the site is saved in prefs.
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));

  // Navigation to a non logged-in site.
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://bar.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
}

TEST_F(LoginDetectionTabHelperTest, OAuthLoginViaRedirect) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

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

  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow);
}

// Test that OAuth login is still detected when there are intermediate
// navigations to other sites.
TEST_F(LoginDetectionTabHelperTest, InvalidOAuthLogins) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // OAuth login start
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);

  // Intermediate navigation just ignored
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://bar.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);

  // OAuth login complete will be detected
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow);
}

// Test that OAuth login is still detected when there are intermediate redirect
// navigations to other sites.
TEST_F(LoginDetectionTabHelperTest, InvalidOAuthLoginsWithRedirect) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

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

  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow);
}

}  // namespace login_detection
