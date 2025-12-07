// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_tab_helper.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "services/metrics/public/cpp/ukm_builders.h"

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
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Verifies the metrics for the given login detection type to be recorded.
  void VerifyLoginDetectionTypeMetrics(LoginDetectionType type) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          1);
    VerifyLoginDetectionUkm(type, ukm::builders::LoginDetectionV2::kEntryName);
    VerifyLoginDetectionUkm(
        type, ukm::builders::LoginDetectionV2IdentityProvider::kEntryName);
  }

  void VerifyLoginDetectionUkm(LoginDetectionType type,
                               const char* entry_name) {
    auto entries = ukm_recorder_->GetEntriesByName(entry_name);
    if (type == LoginDetectionType::kNoLogin) {
      EXPECT_TRUE(entries.empty());
      return;
    }

    ASSERT_EQ(1u, entries.size());
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entries[0], ukm::builders::LoginDetectionV2::kPage_LoginTypeName,
        static_cast<int64_t>(type));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

TEST_F(LoginDetectionTabHelperTest, NoLogin) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
}

TEST_F(LoginDetectionTabHelperTest,
       NoLogin_BrowserAssistedLoginHistogramNotRecorded) {
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  histogram_tester_->ExpectTotalCount(
      "PasswordManager.BrowserAssistedLogin.Type", 0);
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
  histogram_tester_->ExpectUniqueSample(
      "PasswordManager.BrowserAssistedLogin.Type",
      password_manager::metrics_util::BrowserAssistedLoginType::kNonFedCmOAuth,
      1);
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
  base::RunLoop().RunUntilIdle();
  simulator->Redirect(GURL("https://oauth.com/authenticate?client_id=123"));
  base::RunLoop().RunUntilIdle();
  simulator->Redirect(GURL("https://oauth.com/user_login"));
  base::RunLoop().RunUntilIdle();
  simulator->Redirect(GURL("https://oauth.com/?username=user&password=123"));
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
  simulator->Redirect(GURL("https://oauth.com/authenticate?client_id=123"));
  base::RunLoop().RunUntilIdle();
  simulator->Redirect(GURL("https://oauth.com/user_login"));
  base::RunLoop().RunUntilIdle();
  simulator->Redirect(GURL("https://oauth.com/?username=user&password=123"));
  base::RunLoop().RunUntilIdle();
  simulator->Redirect(GURL("https://bar.com/page.html"));
  base::RunLoop().RunUntilIdle();
  simulator->Redirect(GURL("https://foo.com/redirect?code=secret"));
  simulator->Commit();

  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow);
}

TEST_F(LoginDetectionTabHelperTest, PopUpOAuthLogin) {
  // Opener page.
  NavigateAndCommit(GURL("https://foo.com/page.html"));
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // Simulate opening a popup.
  std::unique_ptr<content::WebContents> popup_web_contents =
      CreateTestWebContents();
  LoginDetectionTabHelper::MaybeCreateForWebContents(popup_web_contents.get());
  LoginDetectionTabHelper* opener_tab_helper =
      LoginDetectionTabHelper::FromWebContents(web_contents());
  opener_tab_helper->DidOpenRequestedURL(
      popup_web_contents.get(), web_contents()->GetPrimaryMainFrame(),
      GURL("https://oauth.com/authenticate?client_id=123"), content::Referrer(),
      WindowOpenDisposition::NEW_POPUP, ui::PAGE_TRANSITION_LINK, false, true);

  // Navigate the popup.
  auto popup_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://oauth.com/authenticate?client_id=123"),
      popup_web_contents.get());
  popup_simulator->Commit();

  // The popup navigation should not record any login metric.
  VerifyLoginDetectionTypeMetrics(LoginDetectionType::kNoLogin);
  ResetMetricsTesters();

  // Destroying the popup should trigger login detection.
  popup_web_contents.reset();
  VerifyLoginDetectionTypeMetrics(
      LoginDetectionType::kOauthPopUpFirstTimeLoginFlow);
}

}  // namespace login_detection
