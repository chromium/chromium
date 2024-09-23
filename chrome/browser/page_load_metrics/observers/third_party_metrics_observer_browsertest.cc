// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/page_load_metrics/browser/observers/third_party_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kReadCookieHistogram[] =
    "PageLoad.Clients.ThirdParty.Origins.CookieRead2";
const char kSubframeFCPHistogram[] =
    "PageLoad.Clients.ThirdParty.Frames.NavigationToFirstContentfulPaint3";
const char kOpaqueSubframeFCPHistogram[] =
    "PageLoad.Clients.ThirdParty.Frames.Opaque."
    "NavigationToFirstContentfulPaint";
const char kOpaqueSubframeLCPHistogram[] =
    "PageLoad.Clients.ThirdParty.Frames.Opaque."
    "NavigationToLargestContentfulPaint";

void InvokeStorageAccessOnFrame(content::RenderFrameHost* frame,
                                blink::mojom::WebFeature storage_feature) {
  switch (storage_feature) {
    case blink::mojom::WebFeature::kThirdPartyLocalStorage:
      EXPECT_TRUE(content::ExecJs(frame, "window.localStorage"));
      break;
    case blink::mojom::WebFeature::kThirdPartySessionStorage:
      EXPECT_TRUE(content::ExecJs(frame, "window.sessionStorage"));
      break;
    case blink::mojom::WebFeature::kThirdPartyFileSystem:
      EXPECT_EQ(true, content::EvalJs(
                          frame,
                          "new Promise((resolve) => { "
                          " window.webkitRequestFileSystem(window.TEMPORARY,"
                          " 5*1024, () => resolve(true),"
                          " () => resolve(false));"
                          "});"));
      break;
    case blink::mojom::WebFeature::kV8StorageManager_GetDirectory_Method:
      EXPECT_EQ(
          true,
          content::EvalJs(
              frame, "navigator.storage.getDirectory().then(() => true);"));
      break;
    case blink::mojom::WebFeature::kThirdPartyIndexedDb:
      EXPECT_EQ(true,
                content::EvalJs(
                    frame,
                    "new Promise((resolve) => {"
                    " var request = window.indexedDB.open(\"testdb\", 3); "
                    " request.onsuccess = () => resolve(true);"
                    " request.onerror = () => resolve(false);"
                    "});"));
      break;
    case blink::mojom::WebFeature::kThirdPartyCacheStorage:
      EXPECT_EQ(true, content::EvalJs(
                          frame,
                          "new Promise((resolve) => {"
                          " caches.open(\"testcache\").then("
                          " () => resolve(true)).catch(() => resolve(false))"
                          "});"));
      break;
    default:
      // Only invoke storage access for web features associated with a third
      // party storage access type.
      NOTREACHED_IN_MIGRATION();
  }
}

blink::mojom::WebFeature MetricForTestCase(blink::mojom::WebFeature test_case) {
  if (test_case ==
      blink::mojom::WebFeature::kV8StorageManager_GetDirectory_Method) {
    return blink::mojom::WebFeature::kThirdPartyFileSystem;
  }
  return test_case;
}

class ThirdPartyMetricsObserverBrowserTest : public InProcessBrowserTest {
 protected:
  ThirdPartyMetricsObserverBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ThirdPartyMetricsObserverBrowserTest(
      const ThirdPartyMetricsObserverBrowserTest&) = delete;
  ThirdPartyMetricsObserverBrowserTest& operator=(
      const ThirdPartyMetricsObserverBrowserTest&) = delete;

  ~ThirdPartyMetricsObserverBrowserTest() override = default;

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

  void NavigateToUntrackedUrl() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server()->GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateToPageWithFrameAndWaitForFrame(
      const std::string& host,
      page_load_metrics::PageLoadMetricsTestWaiter* waiter) {
    GURL main_url(https_server()->GetURL(host, "/iframe.html"));

    waiter->AddSubframeNavigationExpectation();
    NavigateToPageWithFrame(host);
    waiter->Wait();
  }

  // TODO(ericrobinson) The following functions all have an assumed frame.
  // Prefer passing in a frame to make the tests clearer and extendable.

  void NavigateFrameAndWaitForFCP(
      const std::string& host,
      const std::string& path,
      page_load_metrics::PageLoadMetricsTestWaiter* waiter) {
    GURL page = https_server()->GetURL(host, path);
    NavigateFrameAndWaitForFCP(page, waiter);
  }

  void NavigateFrameAndWaitForFCP(
      const GURL& url,
      page_load_metrics::PageLoadMetricsTestWaiter* waiter) {
    // Waiting for the frame to navigate ensures that any previous RFHs for this
    // frame have been deleted and therefore won't pollute any future frame
    // expectations (such as FCP).
    waiter->AddSubframeNavigationExpectation();
    NavigateFrameToUrl(url);
    waiter->Wait();

    waiter->AddSubFrameExpectation(
        page_load_metrics::PageLoadMetricsTestWaiter::TimingField::
            kFirstContentfulPaint);
    waiter->Wait();
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = https_server()->GetURL(host, path);
    NavigateFrameToUrl(page);
  }

  void NavigateFrameToUrl(const GURL& url) {
    EXPECT_TRUE(NavigateIframeToURL(web_contents(), "test", url));
  }

  void TriggerFrameActivation() {
    // Activate one frame by executing a dummy script.
    content::RenderFrameHost* ad_frame =
        ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
    const std::string no_op_script = "// No-op script";
    EXPECT_TRUE(ExecJs(ad_frame, no_op_script));
  }

  void Enable3pcForUrl(GURL url) {
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetCookieSetting(url, CONTENT_SETTING_ALLOW);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // This is needed because third party cookies must be marked SameSite=None and
  // Secure, so they must be accessed over HTTPS.
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       OneFirstPartyFrame_NoTimingRecorded) {
  base::HistogramTester histogram_tester;
  page_load_metrics::PageLoadMetricsTestWaiter waiter(
      browser()->tab_strip_model()->GetActiveWebContents());
  NavigateToPageWithFrameAndWaitForFrame("a.com", &waiter);

  // Navigate the frame to a first-party.
  NavigateFrameAndWaitForFCP("a.com", "/select.html", &waiter);
  histogram_tester.ExpectTotalCount(kSubframeFCPHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       OneThirdPartyFrame_OneTimingRecorded) {
  base::HistogramTester histogram_tester;

  page_load_metrics::PageLoadMetricsTestWaiter waiter(
      browser()->tab_strip_model()->GetActiveWebContents());
  NavigateToPageWithFrameAndWaitForFrame("a.com", &waiter);

  // Navigate the frame to a third-party.
  NavigateFrameAndWaitForFCP("b.com", "/select.html", &waiter);
  histogram_tester.ExpectTotalCount(kSubframeFCPHistogram, 1);
}

// TODO(crbug.com/334416161): Re-enble this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ThreeThirdPartyFrames_ThreeTimingsRecorded \
  DISABLED_ThreeThirdPartyFrames_ThreeTimingsRecorded
#else
#define MAYBE_ThreeThirdPartyFrames_ThreeTimingsRecorded \
  ThreeThirdPartyFrames_ThreeTimingsRecorded
#endif
IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       MAYBE_ThreeThirdPartyFrames_ThreeTimingsRecorded) {
  base::HistogramTester histogram_tester;

  page_load_metrics::PageLoadMetricsTestWaiter waiter(
      browser()->tab_strip_model()->GetActiveWebContents());
  NavigateToPageWithFrameAndWaitForFrame("a.com", &waiter);

  // Navigate the frame to a third-party.
  NavigateFrameAndWaitForFCP("b.com", "/select.html", &waiter);

  // Navigate the frame to a different third-party.
  NavigateFrameAndWaitForFCP("c.com", "/select.html", &waiter);

  // Navigate the frame to a repeat third-party.
  NavigateFrameAndWaitForFCP("b.com", "/select.html", &waiter);

  // Navigate the frame to first-party.
  NavigateFrameAndWaitForFCP("a.com", "/select.html", &waiter);
  histogram_tester.ExpectTotalCount(kSubframeFCPHistogram, 3);
}

// TODO(crbug.com/334416161): Re-enble this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_OpaqueOriginSubframe DISABLED_OpaqueOriginSubframe
#else
#define MAYBE_OpaqueOriginSubframe OpaqueOriginSubframe
#endif
IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       MAYBE_OpaqueOriginSubframe) {
  base::HistogramTester histogram_tester;

  page_load_metrics::PageLoadMetricsTestWaiter waiter(
      browser()->tab_strip_model()->GetActiveWebContents());
  NavigateToPageWithFrameAndWaitForFrame("a.com", &waiter);

  // Navigate the frame to a third-party page.
  NavigateFrameAndWaitForFCP("b.com", "/select.html", &waiter);

  // Navigate the frame to an opaque origin URL.
  NavigateFrameAndWaitForFCP(GURL("data:,hello"), &waiter);

  content::RenderFrameHost* subframe_rfh =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), /*index=*/0);
  ASSERT_TRUE(subframe_rfh->GetLastCommittedOrigin().opaque());

  histogram_tester.ExpectTotalCount(kSubframeFCPHistogram, 2);
  histogram_tester.ExpectTotalCount(kOpaqueSubframeFCPHistogram, 1);
  histogram_tester.ExpectTotalCount(kOpaqueSubframeLCPHistogram, 1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest, NoStorageEvent) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyLocalStorage, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartySessionStorage, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyFileSystem, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyIndexedDb, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCacheStorage, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      0);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       FirstPartyCookiesReadAndWrite) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Should read a same-origin cookie.
  NavigateFrameTo("a.com", "/set-cookie?same-origin");  // same-origin write
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      0);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyCookiesReadAndWrite) {
  Enable3pcForUrl(https_server()->GetURL("b.com", "/"));
  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  NavigateFrameTo("b.com", "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo("b.com", "/");
  observer.Wait();
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyCookiesIPAddress) {
  GURL url =
      https_server()->GetURL("/set-cookie?thirdparty=1;SameSite=None;Secure");
  Enable3pcForUrl(url);
  content::CookieChangeObserver observer(web_contents(), 2);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // Hostname is an IP address.
  ASSERT_EQ(
      "",
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  NavigateFrameToUrl(url);           // 3p cookie write
  NavigateFrameTo(url.host(), "/");  // 3p cookie read
  observer.Wait();
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       MultipleThirdPartyCookiesReadAndWrite) {
  Enable3pcForUrl(https_server()->GetURL("b.com", "/"));
  Enable3pcForUrl(https_server()->GetURL("c.com", "/"));
  content::CookieChangeObserver observer(web_contents(), 4);
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  // 3p cookie write
  NavigateFrameTo("b.com", "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo("b.com", "/");
  // 3p cookie write
  NavigateFrameTo("c.com", "/set-cookie?thirdparty=1;SameSite=None;Secure");
  // 3p cookie read
  NavigateFrameTo("c.com", "/");
  observer.Wait();
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 2, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       FirstPartyDocCookieReadAndWrite) {
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

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      0);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyDocCookieReadAndWrite) {
  Enable3pcForUrl(https_server()->GetURL("b.com", "/"));
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

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 1, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyDocCookieReadNoWrite) {
  Enable3pcForUrl(https_server()->GetURL("b.com", "/"));
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("b.com", "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  // Read a third-party cookie.
  EXPECT_TRUE(content::ExecJs(frame, "let x = document.cookie;"));
  NavigateToUntrackedUrl();

  // No read is counted since no cookie has previously been set.
  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      0);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyDocCookieWriteNoRead) {
  Enable3pcForUrl(https_server()->GetURL("b.com", "/"));
  content::CookieChangeObserver observer(web_contents());
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");  // Same origin cookie read.
  NavigateFrameTo("b.com", "/empty.html");
  content::RenderFrameHost* frame =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  // Write a third-party cookie.
  EXPECT_TRUE(content::ExecJs(
      frame, "document.cookie = 'foo=bar;SameSite=None;Secure';"));
  observer.Wait();
  NavigateToUntrackedUrl();

  histogram_tester.ExpectUniqueSample(kReadCookieHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieRead, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyCookieWrite, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      1);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       FirstPartyStorageAccess_UseCounterNotRecorded) {
  std::vector<blink::mojom::WebFeature> test_cases = {
      blink::mojom::WebFeature::kThirdPartyLocalStorage,
      blink::mojom::WebFeature::kThirdPartySessionStorage,
      blink::mojom::WebFeature::kThirdPartyFileSystem,
      blink::mojom::WebFeature::kThirdPartyIndexedDb,
      blink::mojom::WebFeature::kThirdPartyCacheStorage,
      blink::mojom::WebFeature::kV8StorageManager_GetDirectory_Method};

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    NavigateToPageWithFrame("a.com");
    NavigateFrameTo("a.com", "/empty.html");
    InvokeStorageAccessOnFrame(
        ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0), test_case);
    NavigateToUntrackedUrl();

    histogram_tester.ExpectBucketCount("Blink.UseCounter.Features",
                                       MetricForTestCase(test_case), 0);
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyAccess, 0);
  }
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyStorageAccess_UseCounterRecorded) {
  std::vector<blink::mojom::WebFeature> test_cases = {
      blink::mojom::WebFeature::kThirdPartyLocalStorage,
      blink::mojom::WebFeature::kThirdPartySessionStorage,
      blink::mojom::WebFeature::kThirdPartyFileSystem,
      blink::mojom::WebFeature::kThirdPartyIndexedDb,
      blink::mojom::WebFeature::kThirdPartyCacheStorage,
      blink::mojom::WebFeature::kV8StorageManager_GetDirectory_Method};

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    NavigateToPageWithFrame("a.com");
    NavigateFrameTo("b.com", "/empty.html");
    InvokeStorageAccessOnFrame(
        ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0), test_case);
    NavigateToUntrackedUrl();

    histogram_tester.ExpectBucketCount("Blink.UseCounter.Features",
                                       MetricForTestCase(test_case), 1);
    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kThirdPartyAccess, 1);
  }
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       ThirdPartyFrameWithActivationReported) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/");
  TriggerFrameActivation();
  NavigateToUntrackedUrl();
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyActivation, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyAccessAndActivation, 0);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyMetricsObserverBrowserTest,
                       FirstPartyFrameWithActivationNotReported) {
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("a.com", "/");
  TriggerFrameActivation();
  NavigateToUntrackedUrl();
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyActivation, 0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      0);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyAccessAndActivation, 0);
}

IN_PROC_BROWSER_TEST_F(
    ThirdPartyMetricsObserverBrowserTest,
    ThirdPartyFrameWithAccessAndActivationOnDifferentThirdParties) {
  Enable3pcForUrl(https_server()->GetURL("b.com", "/"));
  Enable3pcForUrl(https_server()->GetURL("c.com", "/"));
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/");
  TriggerFrameActivation();
  NavigateFrameTo("c.com", "/set-cookie?thirdparty=1;SameSite=None;Secure");
  NavigateToUntrackedUrl();
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyActivation, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyAccessAndActivation, 0);
}

IN_PROC_BROWSER_TEST_F(
    ThirdPartyMetricsObserverBrowserTest,
    ThirdPartyFrameWithAccessAndActivationOnSameThirdParties) {
  Enable3pcForUrl(https_server()->GetURL("b.com", "/"));
  base::HistogramTester histogram_tester;
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/set-cookie?thirdparty=1;SameSite=None;Secure");
  TriggerFrameActivation();
  NavigateToUntrackedUrl();
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyActivation, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features", blink::mojom::WebFeature::kThirdPartyAccess,
      1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kThirdPartyAccessAndActivation, 1);
}

}  // namespace
