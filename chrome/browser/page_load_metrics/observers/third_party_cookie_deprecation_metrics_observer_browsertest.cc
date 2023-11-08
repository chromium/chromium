// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/experiment/tpcd_pref_names.h"
#include "chrome/browser/tpcd/experiment/tpcd_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kThirdPartyCookieAccessBlockedHistogram[] =
    "PageLoad.Clients.ThirdPartyCookieAccessBlockedByExperiment";

}  // namespace

class ThirdPartyCookieDeprecationObserverBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
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
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server()->Start());
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("isad=1")});
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

  void FetchCookies(const std::string& host, const std::string& path) {
    // Fetch a subresrouce.
    std::string fetch_subresource_script = R"(
        const imgElement = document.createElement('img');
        imgElement.src = $1;
        document.body.appendChild(imgElement);
  )";

    content::CookieChangeObserver observer(web_contents());
    std::ignore =
        ExecJs(web_contents(),
               content::JsReplace(fetch_subresource_script,
                                  https_server()->GetURL(host, path).spec()));
    observer.Wait();
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

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdCookieRead) {
  SetUpThirdPartyCookieExperimentWithClientState();

  content::CookieChangeObserver observer(web_contents());
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  FetchCookies("b.test",
               "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");
  // 3p cookie read
  FetchCookies("b.test", "/empty.html?isad=1");

  NavigateToUntrackedUrl();

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd", true,
        1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment", true, 1);
  } else {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        0);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment", false, 1);
    histogram_tester.ExpectTotalCount(
        "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyNonAdCookieRead) {
  SetUpThirdPartyCookieExperimentWithClientState();

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  FetchCookies("b.test",
               "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=0");
  // 3p cookie read
  FetchCookies("b.test", "/empty.html?isad=0");

  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment, 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment", 0);

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd",
        false, 1);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       FirstPartyAdCookieRead) {
  SetUpThirdPartyCookieExperimentWithClientState();

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.test");  // Same origin cookie read.
  // 1p cookie write
  FetchCookies("a.test",
               "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=0");
  FetchCookies("a.test",
               "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");
  // 1p cookie read
  FetchCookies("a.test", "/empty.html?isad=0");
  FetchCookies("a.test", "/empty.html?isad=1");

  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment, 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment", 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd", 0);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdCookieReadSubframe) {
  SetUpThirdPartyCookieExperimentWithClientState();

  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  NavigateFrameTo("b.com",
                  "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");
  // 3p cookie read
  NavigateFrameTo("b.com", "/empty.html?isad=1");
  observer.Wait();

  NavigateToUntrackedUrl();

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment, 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment", 0);
  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd",
        false, 1);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdCookieReadOnRedirect) {
  SetUpThirdPartyCookieExperimentWithClientState();

  content::CookieChangeObserver observer(web_contents());
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  FetchCookies("b.test",
               "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");
  // 3p cookie read
  FetchCookies(
      "c.test",
      "/server-redirect?" +
          https_server()->GetURL("b.test", "/empty.html?isad=1").spec());

  NavigateToUntrackedUrl();

  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd", true,
        1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment", true, 1);
  } else {
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment,
        0);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment", false, 1);
    histogram_tester.ExpectTotalCount(
        "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd", 0);
  }
}

IN_PROC_BROWSER_TEST_P(ThirdPartyCookieDeprecationObserverBrowserTest,
                       ThirdPartyAdCookieReadOnNonAdRedirect) {
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/do-redirect");
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server->AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server->Start());

  SetUpThirdPartyCookieExperimentWithClientState();

  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  FetchCookies("b.test",
               "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");

  // Fetch the controllable response manually and set the location header,
  // /set-redirect cannot be used as the ad suffix will still be tagged.
  std::string fetch_subresource_script = R"(
        const imgElement = document.createElement('img');
        imgElement.src = $1;
        document.body.appendChild(imgElement);
  )";

  content::CookieChangeObserver observer(web_contents());
  std::ignore =
      ExecJs(web_contents(),
             content::JsReplace(
                 fetch_subresource_script,
                 https_server->GetURL("c.test", "/do-redirect").spec()));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(
      "Location", https_server->GetURL("b.test", "/empty.html?isad=1").spec());
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  observer.Wait();

  NavigateToUntrackedUrl();

  // For redirect chains, cookie access is tagged as an ad only depending on the
  // ad status of the initial request in the redirect chain, so this cookie
  // access will count as a non-ad cookie access.
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieAdAccessBlockByExperiment, 0);
  histogram_tester.ExpectTotalCount(
      "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment", 0);
  if (IsRecordThirdPartyCookiesExperimentMetrics()) {
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd",
        false, 1);
  }
}
