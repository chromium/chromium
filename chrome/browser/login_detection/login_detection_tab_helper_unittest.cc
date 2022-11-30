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
#include "services/metrics/public/cpp/ukm_builders.h"

namespace login_detection {

class LoginDetectionTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  explicit LoginDetectionTabHelperTest(
      const std::string& field_trial_logged_in_sites = "")
      : field_trial_logged_in_sites_(field_trial_logged_in_sites) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kLoginDetection, {{"logged_in_sites", field_trial_logged_in_sites_}});
    ChromeRenderViewHostTestHarness::SetUp();
    ResetMetricsTesters();
    LoginDetectionTabHelper::MaybeCreateForWebContents(web_contents());
  }

  void ResetMetricsTesters() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Verifies the UMA and UKM metrics for the given login detection type to be
  // recorded.
  void VerifyLoginDetectionTypeMetrics(LoginDetectionType type) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          1);

    const auto& entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::LoginDetection::kEntryName);
    ASSERT_EQ(1U, entries.size());
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entries[0], ukm::builders::LoginDetection::kPage_LoginTypeName,
        static_cast<int64_t>(type));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  // List of sites that are treated as logged-in from field trial.
  const std::string field_trial_logged_in_sites_;
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

  // Subsequent navigations to OAuth signed-in site.
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://images.foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthLogin);
}

TEST_F(LoginDetectionTabHelperTest, NavigationToOAuthLoggedInSite) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // Trigger OAuth login so that the site is saved in prefs.
  NavigateAndCommit(GURL("https://oauth.com/authenticate?client_id=123"));
  NavigateAndCommit(GURL("https://foo.com/redirect?code=secret"));

  // Subsequent navigations to OAuth signed-in site.
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://images.foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthLogin);

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

  // Subsequent navigations to OAuth signed-in site.
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://images.foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthLogin);
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

  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://images.foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthLogin);
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

class LoginDetectionTabHelperTestWithFieldTrialLoggedInList
    : public LoginDetectionTabHelperTest {
 public:
  // Set up foo.com and bar.com as logged-in.
  LoginDetectionTabHelperTestWithFieldTrialLoggedInList()
      : LoginDetectionTabHelperTest("https://foo.com,https://bar.com/") {}
};

TEST_F(LoginDetectionTabHelperTestWithFieldTrialLoggedInList, FooLogin) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kFieldTrialLoggedInSite);

  // Other subdomains of foo.com should be treated as logged-in too.
  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://www.foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kFieldTrialLoggedInSite);

  ResetMetricsTesters();
  NavigateAndCommit(GURL("https://m.foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kFieldTrialLoggedInSite);
}

TEST_F(LoginDetectionTabHelperTestWithFieldTrialLoggedInList, BarLogin) {
  NavigateAndCommit(GURL("https://bar.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kFieldTrialLoggedInSite);
}

TEST_F(LoginDetectionTabHelperTestWithFieldTrialLoggedInList, NoLogin) {
  NavigateAndCommit(GURL("https://qux.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
}

}  // namespace login_detection
