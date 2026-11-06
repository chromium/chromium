// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/navigation_observer/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/content/browser/navigation_observer/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/navigation_observer/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#endif

namespace safe_browsing {

class ChromeClientSideDetectionHostDelegateTest : public InProcessBrowserTest {
 public:
  ChromeClientSideDetectionHostDelegateTest() = default;

  ChromeClientSideDetectionHostDelegateTest(
      const ChromeClientSideDetectionHostDelegateTest&) = delete;
  ChromeClientSideDetectionHostDelegateTest& operator=(
      const ChromeClientSideDetectionHostDelegateTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://foo/0"),
        ui::mojom::WindowOpenDisposition::kNewForegroundTab,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    Profile* profile = browser()->GetProfile();
    navigation_observer_manager_ =
        SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
            profile);
    ASSERT_TRUE(browser()->GetTabStripModel()->GetActiveWebContents());
    navigation_observer_ = std::make_unique<SafeBrowsingNavigationObserver>(
        browser()->GetTabStripModel()->GetActiveWebContents(),
        HostContentSettingsMapFactory::GetForProfile(profile),
        navigation_observer_manager_);
  }

  void TearDownOnMainThread() override {
    navigation_observer_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  NavigationEventList* navigation_event_list() {
    return navigation_observer_manager_->navigation_event_list();
  }

 protected:
  raw_ptr<SafeBrowsingNavigationObserverManager, DanglingUntriaged>
      navigation_observer_manager_;
  std::unique_ptr<SafeBrowsingNavigationObserver> navigation_observer_;
};

IN_PROC_BROWSER_TEST_F(ChromeClientSideDetectionHostDelegateTest,
                       GetReferrerChain) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);

  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://a.com/");
  first_navigation->last_updated = one_second_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<NavigationEvent> second_navigation =
      std::make_unique<NavigationEvent>();
  second_navigation->source_url = GURL("http://a.com/");
  second_navigation->original_request_url = GURL("http://b.com/");
  second_navigation->last_updated = now;
  second_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(second_navigation));

  std::unique_ptr<ChromeClientSideDetectionHostDelegate> csd_host_delegate =
      std::make_unique<ChromeClientSideDetectionHostDelegate>(
          browser()->GetTabStripModel()->GetActiveWebContents());
  csd_host_delegate->SetNavigationObserverManagerForTesting(
      navigation_observer_manager_);
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  csd_host_delegate->AddReferrerChain(verdict.get(), GURL("http://b.com/"),
                                      content::GlobalRenderFrameHostId());
  ReferrerChain referrer_chain = verdict->referrer_chain();

  EXPECT_EQ(2, referrer_chain.size());

  EXPECT_EQ("http://b.com/", referrer_chain[0].url());
  EXPECT_EQ("http://a.com/", referrer_chain[0].referrer_url());
}

IN_PROC_BROWSER_TEST_F(ChromeClientSideDetectionHostDelegateTest,
                       NoNavigationObserverManager) {
  base::Time now = base::Time::Now();
  base::Time one_second_ago = base::Time::FromSecondsSinceUnixEpoch(
      now.InSecondsFSinceUnixEpoch() - 1.0);

  std::unique_ptr<NavigationEvent> first_navigation =
      std::make_unique<NavigationEvent>();
  first_navigation->original_request_url = GURL("http://a.com/");
  first_navigation->last_updated = one_second_ago;
  first_navigation->navigation_initiation =
      ReferrerChainEntry::BROWSER_INITIATED;
  navigation_event_list()->RecordNavigationEvent(std::move(first_navigation));

  std::unique_ptr<ChromeClientSideDetectionHostDelegate> csd_host_delegate =
      std::make_unique<ChromeClientSideDetectionHostDelegate>(
          browser()->GetTabStripModel()->GetActiveWebContents());
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  csd_host_delegate->AddReferrerChain(verdict.get(), GURL("http://b.com/"),
                                      content::GlobalRenderFrameHostId());
  ReferrerChain referrer_chain = verdict->referrer_chain();

  EXPECT_EQ(0, referrer_chain.size());
}

class ChromeClientSideDetectionHostDelegateInnerTextTest
    : public InProcessBrowserTest {
 public:
  ChromeClientSideDetectionHostDelegateInnerTextTest() = default;
  ~ChromeClientSideDetectionHostDelegateInnerTextTest() override = default;

  ChromeClientSideDetectionHostDelegateInnerTextTest(
      const ChromeClientSideDetectionHostDelegateInnerTextTest&) = delete;
  ChromeClientSideDetectionHostDelegateInnerTextTest& operator=(
      const ChromeClientSideDetectionHostDelegateInnerTextTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ChromeClientSideDetectionHostDelegateInnerTextTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  std::string ExtractInnerText() {
    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
    ChromeClientSideDetectionHostDelegate delegate(web_contents);
    base::test::TestFuture<std::string> future;
    delegate.GetInnerText(future.GetCallback());
    return future.Take();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const GURL request_url = request.GetURL();
    const std::string_view path = request_url.path();
    std::string html;
    if (path == "/plain.html") {
      html = "<html><body><p>Plain page text</p></body></html>";
    } else if (path == "/cross_origin_parent.html") {
      const GURL b_frame =
          embedded_test_server()->GetURL("b.com", "/frame_first.html");
      const GURL c_frame =
          embedded_test_server()->GetURL("c.com", "/frame_second.html");
      html = base::StrCat({"<html><body><p>Main frame text</p><iframe src=\"",
                           b_frame.spec(), "\"></iframe><iframe src=\"",
                           c_frame.spec(), "\"></iframe></body></html>"});
    } else if (path == "/complex_iframes.html") {
      const GURL same_origin_child =
          embedded_test_server()->GetURL("a.com", "/same_origin_child.html");
      const GURL empty_cross_origin =
          embedded_test_server()->GetURL("c.com", "/empty_frame.html");
      html = base::StrCat(
          {"<html><body><p>Main text</p>", "<iframe src=\"",
           same_origin_child.spec(), "\"></iframe>",
           "<iframe sandbox srcdoc=\"<p>Sandboxed scam text</p>\"></iframe>",
           "<iframe src=\"data:text/html,<p>Data URL scam text</p>\"></iframe>",
           "<iframe src=\"", empty_cross_origin.spec(), "\"></iframe>",
           "</body></html>"});
    } else if (path == "/same_origin_child.html") {
      const GURL nested_cross_origin =
          embedded_test_server()->GetURL("b.com", "/nested_cross_origin.html");
      html = base::StrCat({"<html><body><p>Same-origin text</p><iframe src=\"",
                           nested_cross_origin.spec(),
                           "\"></iframe></body></html>"});
    } else if (path == "/frame_first.html") {
      html = "<html><body><p>First iframe text</p></body></html>";
    } else if (path == "/frame_second.html") {
      html = "<html><body><p>Second iframe text</p></body></html>";
    } else if (path == "/nested_cross_origin.html") {
      html = "<html><body><p>Nested cross-origin text</p></body></html>";
    } else if (path == "/empty_frame.html") {
      html = "<html><body>   </body></html>";
    } else if (path == "/many_iframes.html") {
      std::string count_str;
      int count = 0;
      if (net::GetValueForKeyInQuery(request_url, "count", &count_str)) {
        base::StringToInt(count_str, &count);
      }
      html = "<html><body><p>Main text</p>";
      for (int i = 0; i < count; ++i) {
        html += base::StrCat({"<iframe sandbox srcdoc=\"<p>Frame ",
                              base::NumberToString(i), "</p>\"></iframe>"});
      }
      html += "</body></html>";
    } else if (path == "/hung.html") {
      return std::make_unique<net::test_server::HungResponse>();
    } else {
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content(std::move(html));
    return response;
  }
};

class ChromeClientSideDetectionHostDelegateIframeFeatureDisabledTest
    : public ChromeClientSideDetectionHostDelegateInnerTextTest {
 public:
  ChromeClientSideDetectionHostDelegateIframeFeatureDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        kClientSideDetectionIframeInnerText);
  }
};

IN_PROC_BROWSER_TEST_F(
    ChromeClientSideDetectionHostDelegateIframeFeatureDisabledTest,
    ExcludesCrossOriginIframes) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("a.com", "/cross_origin_parent.html")));

  EXPECT_EQ("Main frame text", ExtractInnerText());
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.IntelligentScanGetInnerTextDuration", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.IntelligentScanIframeCount", 0);
  histogram_tester.ExpectTotalCount(
      "SBClientPhishing.IntelligentScanIframeInnerTextSize", 0);
}

class ChromeClientSideDetectionHostDelegateIframeFeatureEnabledTest
    : public ChromeClientSideDetectionHostDelegateInnerTextTest {
 public:
  ChromeClientSideDetectionHostDelegateIframeFeatureEnabledTest() {
    // Use a generous timeout in browser tests to avoid flakiness on slow
    // sanitizer/debug bots when extracting from many out-of-process iframes.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kClientSideDetectionIframeInnerText,
        {{kClientSideDetectionIframeInnerTextTimeout.name, "10s"}});
  }
};

IN_PROC_BROWSER_TEST_F(
    ChromeClientSideDetectionHostDelegateIframeFeatureEnabledTest,
    IncludesCrossOriginAndCloakedIframesWithoutDuplicatingSameOrigin) {
  // Verify cross-origin grandchild nested inside a same-origin child, cloaked
  // (`sandbox srcdoc` and `data:`) iframes, and whitespace-only empty iframe.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("a.com", "/complex_iframes.html")));

    // `content_extraction::CreateInnerTextResult()` concatenates same-origin
    // child frame segments directly into the main frame's `inner_text`, while
    // our traversal extracts the sandboxed, `data:`, and nested cross-origin
    // grandchild frames once (in breadth-first frame-tree order) and skips the
    // whitespace-only empty iframe.
    EXPECT_EQ(
        "Main textSame-origin text\nSandboxed scam text\n"
        "Data URL scam text\nNested cross-origin text",
        ExtractInnerText());
    // 4 non-same-origin subframes queried: sandboxed srcdoc, data: URL, empty
    // c.com child, and nested b.com grandchild.
    histogram_tester.ExpectTotalCount(
        "SBClientPhishing.IntelligentScanGetInnerTextDuration", 1);
    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.IntelligentScanIframeCount", 4, 1);
    const std::string expected_iframe_concat =
        "Sandboxed scam textData URL scam textNested cross-origin text";
    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.IntelligentScanIframeInnerTextSize",
        expected_iframe_concat.size(), 1);
    histogram_tester.ExpectTotalCount(
        "SBClientPhishing.IntelligentScanEmptyMainFrameWithLoadingIframes", 0);
  }

  // Verify pages with no iframes log 0 count and omit iframe-only histograms.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", "/plain.html")));

    EXPECT_EQ("Plain page text", ExtractInnerText());
    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.IntelligentScanIframeCount", 0, 1);
    histogram_tester.ExpectTotalCount(
        "SBClientPhishing.IntelligentScanIframeInnerTextSize", 0);
    histogram_tester.ExpectTotalCount(
        "SBClientPhishing.IntelligentScanEmptyMainFrameWithLoadingIframes", 0);
  }

  // Verify extraction caps the number of queried subframes at
  // `kClientSideDetectionMaxIframesToExtract`.
  {
    base::HistogramTester histogram_tester;
    const int max_iframes = kClientSideDetectionMaxIframesToExtract.Get();
    const int total_iframes = max_iframes + 2;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "a.com", base::StrCat({"/many_iframes.html?count=",
                                   base::NumberToString(total_iframes)}))));

    std::string expected_text = "Main text";
    for (int i = 0; i < max_iframes; ++i) {
      expected_text += base::StrCat({"\nFrame ", base::NumberToString(i)});
    }
    EXPECT_EQ(expected_text, ExtractInnerText());
    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.IntelligentScanIframeCount", max_iframes, 1);
  }

  // Verify an empty main frame with an in-flight loading iframe logs
  // `SBClientPhishing.IntelligentScanEmptyMainFrameWithLoadingIframes`.
  {
    base::HistogramTester histogram_tester;
    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
    const GURL hung_url = embedded_test_server()->GetURL("b.com", "/hung.html");
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        content::JsReplace("document.body.innerHTML = '';"
                           "const f = document.createElement('iframe');"
                           "f.src = $1;"
                           "document.body.appendChild(f);",
                           hung_url)));

    EXPECT_EQ("", ExtractInnerText());
    histogram_tester.ExpectUniqueSample(
        "SBClientPhishing.IntelligentScanEmptyMainFrameWithLoadingIframes",
        true, 1);
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(
    ChromeClientSideDetectionHostDelegateIframeFeatureEnabledTest,
    ExcludesExtensionInjectedIframes) {
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Test",
    "version": "1",
    "manifest_version": 3,
    "web_accessible_resources": [{
      "resources": ["frame.html"],
      "matches": ["<all_urls>"]
    }]
  })");
  test_dir.WriteFile(FILE_PATH_LITERAL("frame.html"), "<p>Extension text</p>");

  extensions::ChromeTestExtensionLoader loader(browser()->GetProfile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("a.com", "/cross_origin_parent.html")));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  const GURL ext_frame_url = extension->GetResourceURL("frame.html");
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      content::JsReplace("new Promise(r => {"
                         "  const f = document.createElement('iframe');"
                         "  f.src = $1;"
                         "  f.onload = r;"
                         "  document.body.appendChild(f);"
                         "});",
                         ext_frame_url)));

  base::HistogramTester histogram_tester;
  EXPECT_EQ("Main frame text\nFirst iframe text\nSecond iframe text",
            ExtractInnerText());
  histogram_tester.ExpectUniqueSample(
      "SBClientPhishing.IntelligentScanIframeCount", 2, 1);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace safe_browsing
