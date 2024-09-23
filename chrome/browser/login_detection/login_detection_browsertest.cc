// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/site_isolation/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace login_detection {

class LoginDetectionBrowserTest : public InProcessBrowserTest {
 public:
  LoginDetectionBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kLoginDetection, {}},
            {site_isolation::features::kSiteIsolationForPasswordSites, {}},
        },
        {});
  }

  void SetUpOnMainThread() override {
    https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    host_resolver()->AddRule("*", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    ASSERT_TRUE(https_test_server_.Start());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    cert_verifier_.TearDownInProcessBrowserTestFixture();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Verifies the histograms for the given login detection type to be recorded.
  void ExpectLoginDetectionTypeMetric(LoginDetectionType type) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          1);
  }

  // Verifies the histograms for the given login detection type to be recorded
  // with the given expected bucket count for the prerendering test.
  void ExpectLoginDetectionTypeMetric(
      LoginDetectionType type,
      base::HistogramBase::Count expected_bucket_count) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          expected_bucket_count);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  net::EmbeddedTestServer https_test_server_;
  content::ContentMockCertVerifier cert_verifier_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest, PopUpBasedOAuthLoginFlow) {
  // Navigate to the OAuth requestor.
  ASSERT_TRUE(content::NavigateToURL(
      GetWebContents(),
      https_test_server_.GetURL("www.foo.com", "/title1.html")));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // Create a popup for the navigation flow.
  content::WebContentsAddedObserver web_contents_added_observer;
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace(
          "window.open($1, 'oauth_window', 'width=10,height=10');",
          https_test_server_.GetURL("www.oauthprovider.com",
                                    "/title2.html?client_id=123"))));
  auto* popup_contents = web_contents_added_observer.GetWebContents();
  content::TestNavigationObserver observer(popup_contents);
  observer.WaitForNavigationFinished();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  // This popup navigation is treated as not logged-in too.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // When the popup is closed, it will be detected as OAuth login.
  content::WebContentsDestroyedWatcher destroyed_watcher(popup_contents);
  EXPECT_TRUE(ExecJs(popup_contents, "window.close()"));
  destroyed_watcher.Wait();
  ExpectLoginDetectionTypeMetric(
      LoginDetectionType::kOauthPopUpFirstTimeLoginFlow);
}

class LoginDetectionPrerenderBrowserTest : public LoginDetectionBrowserTest {
 public:
  LoginDetectionPrerenderBrowserTest() {
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(&LoginDetectionPrerenderBrowserTest::GetWebContents,
                            base::Unretained(this)));
  }
  ~LoginDetectionPrerenderBrowserTest() override = default;
  LoginDetectionPrerenderBrowserTest(
      const LoginDetectionPrerenderBrowserTest&) = delete;

  LoginDetectionPrerenderBrowserTest& operator=(
      const LoginDetectionPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_->RegisterServerRequestMonitor(embedded_test_server());
    LoginDetectionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    LoginDetectionBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return *prerender_helper_;
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(LoginDetectionPrerenderBrowserTest,
                       PrerenderingShouldNotRecordLoginDetectionMetrics) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), initial_url));
  ResetHistogramTester();

  const content::FrameTreeNodeId host_id =
      prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  // Login detection metric should not be recorded in prerendering.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin, 0);

  // Activate the prerendered page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  // The page should be activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  // Login detection metric should be recorded after activating.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin, 1);
}

class LoginDetectionFencedFrameBrowserTest : public LoginDetectionBrowserTest {
 public:
  LoginDetectionFencedFrameBrowserTest() = default;
  ~LoginDetectionFencedFrameBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    LoginDetectionBrowserTest::SetUpOnMainThread();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(LoginDetectionFencedFrameBrowserTest,
                       FencedFrameShouldNotRecordLoginDetectionMetrics) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), initial_url));
  ResetHistogramTester();

  // Create a fenced frame to ensure that it doesn't record the login detection
  // metrics.
  GURL fenced_frame_url(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin, 0);
}

}  // namespace login_detection
