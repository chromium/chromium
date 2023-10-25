// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/experiment/tpcd_pref_names.h"
#include "chrome/browser/tpcd/experiment/tpcd_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kThirdPartyCookieAccessBlockedHistogram[] =
    "PageLoad.Clients.ThirdPartyCookieAccessBlockedByExperiment";

}  // namespace

class ThirdPartyCookieDeprecationObserverBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ThirdPartyCookieDeprecationObserverBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        is_experiment_cookies_disabled_(std::get<0>(GetParam())),
        is_client_eligible_(std::get<1>(GetParam())) {}

  ThirdPartyCookieDeprecationObserverBrowserTest(
      const ThirdPartyCookieDeprecationObserverBrowserTest&) = delete;
  ThirdPartyCookieDeprecationObserverBrowserTest& operator=(
      const ThirdPartyCookieDeprecationObserverBrowserTest&) = delete;

  ~ThirdPartyCookieDeprecationObserverBrowserTest() override = default;

  void SetUp() override {
    SetUpThirdPartyCookieExperiment();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for 127.0.0.1 or localhost, so this
    // is needed to load pages from other hosts (b.com, c.com) without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpThirdPartyCookieExperiment() {
    // Experiment feature param requests 3PCs blocked.
    tpcd_experiment_feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{tpcd::experiment::kDisable3PCookiesName,
          is_experiment_cookies_disabled_ ? "true" : "false"}});
  }

  void SetUpThirdPartyCookieExperimentWithClientState() {
    Wait();
    g_browser_process->local_state()->SetInteger(
        tpcd::experiment::prefs::kTPCDExperimentClientState,
        static_cast<int>(
            is_client_eligible_
                ? tpcd::experiment::utils::ExperimentState::kEligible
                : tpcd::experiment::utils::ExperimentState::kIneligible));
  }

  void NavigateToUntrackedUrl() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server()->GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = https_server()->GetURL(host, path);
    NavigateFrameToUrl(page);
  }

  void NavigateFrameToUrl(const GURL& url) {
    EXPECT_TRUE(NavigateIframeToURL(web_contents(), "test", url));
  }

  bool IsRecordThirdPartyCookiesExperimentMetrics() {
    return is_experiment_cookies_disabled_ && is_client_eligible_;
  }

  void Wait() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        tpcd::experiment::kDecisionDelayTime.Get());
    run_loop.Run();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  // This is needed because third party cookies must be marked SameSite=None and
  // Secure, so they must be accessed over HTTPS.
  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList tpcd_experiment_feature_list_;
  bool is_experiment_cookies_disabled_;
  bool is_client_eligible_;
};

INSTANTIATE_TEST_SUITE_P(,
                         ThirdPartyCookieDeprecationObserverBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       FirstPartyCookiesReadAndWrite) {
  SetUpThirdPartyCookieExperimentWithClientState();

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Should read a same-origin cookie.
  NavigateFrameTo("a.com", "/set-cookie?same-origin");  // same-origin write
  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 0);

  // Expect no third party metrics records for first party cases.
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 0);
  histogram_tester.ExpectBucketCount(kThirdPartyCookieAccessBlockedHistogram,
                                     false, 0);
  histogram_tester.ExpectBucketCount(kThirdPartyCookieAccessBlockedHistogram,
                                     true, 0);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyCookiesReadAndWrite) {
  SetUpThirdPartyCookieExperimentWithClientState();

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  NavigateFrameTo("b.com", "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo("b.com", "/");
  observer.Wait();
  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 1);

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 1);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieAccessBlockedHistogram,
                                        true, 2);
  } else {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 0);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieAccessBlockedHistogram,
                                        false, 2);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       FirstPartyJavaScriptCookieReadAndWrite) {
  SetUpThirdPartyCookieExperimentWithClientState();

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("a.com", "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  // Write a first-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';"));

  // Read a first-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 0);

  // Expect no third party metrics records for first party cases.
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 0);
  histogram_tester.ExpectBucketCount(kThirdPartyCookieAccessBlockedHistogram,
                                     false, 0);
  histogram_tester.ExpectBucketCount(kThirdPartyCookieAccessBlockedHistogram,
                                     true, 0);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyJavaScriptCookieReadAndWrite) {
  SetUpThirdPartyCookieExperimentWithClientState();

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("b.com", "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(
      frame, "document.cookie = 'foo=bar;SameSite=None;Secure';"));

  // Read a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  observer.Wait();
  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 1);

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 1);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieAccessBlockedHistogram,
                                        true, 2);
  } else {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment, 0);
    histogram_tester.ExpectUniqueSample(kThirdPartyCookieAccessBlockedHistogram,
                                        false, 2);
  }
}
