// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/url_constants.h"

namespace {

constexpr base::StringPiece kTestDictionaryString = "A dictionary";

constexpr base::StringPiece kCompressedDataOriginalString =
    "This is compressed test data using a test dictionary";

// kBrotliCompressedData is generated using the following commands:
//  $ echo -n "A dictionary" > /tmp/dict
//  $ echo -n "This is compressed test data using a test dictionary" > /tmp/data
//  $ ./brotli -o /tmp/out.sbr -D /tmp/dict /tmp/data
//  $ xxd -i  /tmp/out.sbr
constexpr uint8_t kBrotliCompressedData[] = {
    0xa1, 0x98, 0x01, 0x80, 0x22, 0xe0, 0x26, 0x4b, 0x95, 0x5c, 0x19,
    0x18, 0x9d, 0xc1, 0xc3, 0x44, 0x0e, 0x5c, 0x6a, 0x09, 0x9d, 0xf0,
    0xb0, 0x01, 0x47, 0x14, 0x87, 0x14, 0x6d, 0xfb, 0x60, 0x96, 0xdb,
    0xae, 0x9e, 0x79, 0x54, 0xe3, 0x69, 0x03, 0x29};

// NOLINTNEXTLINE(runtime/string)
const std::string kBrotliCompressedDataString =
    std::string(reinterpret_cast<const char*>(kBrotliCompressedData),
                sizeof(kBrotliCompressedData));

// kZstdCompressedData is generated using the following commands:
//  $ echo -n "A dictionary" > /tmp/dict
//  $ echo -n "This is compressed test data using a test dictionary" > /tmp/data
//  $ zstd -o /tmp/out.szstd -D /tmp/dict /tmp/data
//  $ xxd -i  /tmp/out.szstd
constexpr uint8_t kZstdCompressedData[] = {
    0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x34, 0xa1, 0x01, 0x00, 0x54, 0x68,
    0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x63, 0x6f, 0x6d, 0x70, 0x72,
    0x65, 0x73, 0x73, 0x65, 0x64, 0x20, 0x74, 0x65, 0x73, 0x74, 0x20,
    0x64, 0x61, 0x74, 0x61, 0x20, 0x75, 0x73, 0x69, 0x6e, 0x67, 0x20,
    0x61, 0x20, 0x74, 0x65, 0x73, 0x74, 0x20, 0x64, 0x69, 0x63, 0x74,
    0x69, 0x6f, 0x6e, 0x61, 0x72, 0x79, 0x9e, 0x99, 0xf2, 0xbc};

// NOLINTNEXTLINE(runtime/string)
const std::string kZstdCompressedDataString =
    std::string(reinterpret_cast<const char*>(kZstdCompressedData),
                sizeof(kZstdCompressedData));

class SharedDictionaryAccessObserver : public content::WebContentsObserver {
 public:
  SharedDictionaryAccessObserver(content::WebContents* web_contents,
                                 base::RepeatingClosure on_accessed_callback)
      : WebContentsObserver(web_contents),
        on_accessed_callback_(std::move(on_accessed_callback)) {}

  const network::mojom::SharedDictionaryAccessDetailsPtr& details() const {
    return details_;
  }

 private:
  // WebContentsObserver overrides:
  void OnSharedDictionaryAccessed(
      content::NavigationHandle* navigation,
      const network::mojom::SharedDictionaryAccessDetails& details) override {
    details_ = details.Clone();
    on_accessed_callback_.Run();
  }
  void OnSharedDictionaryAccessed(
      content::RenderFrameHost* rfh,
      const network::mojom::SharedDictionaryAccessDetails& details) override {
    details_ = details.Clone();
    on_accessed_callback_.Run();
  }

  base::RepeatingClosure on_accessed_callback_;
  network::mojom::SharedDictionaryAccessDetailsPtr details_;
};

absl::optional<std::string> GetSecAvailableDictionary(
    const net::test_server::HttpRequest::HeaderMap& headers) {
  auto it = headers.find("sec-available-dictionary");
  if (it == headers.end()) {
    return absl::nullopt;
  }
  return it->second;
}

void CheckSharedDictionaryUseCounter(
    base::HistogramTester& histograms,
    int expected_used_count_with_sbr,
    int expected_used_count_with_zstd_d,
    int expected_used_for_navigation_count,
    int expected_used_for_main_frame_navigation_count,
    int expected_used_for_sub_frame_navigation_count,
    int expected_used_for_subresource_count) {
  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSharedDictionaryUsed,
      expected_used_count_with_sbr + expected_used_count_with_zstd_d);
  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSharedDictionaryUsedForNavigation,
      expected_used_for_navigation_count);
  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSharedDictionaryUsedForMainFrameNavigation,
      expected_used_for_main_frame_navigation_count);
  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSharedDictionaryUsedForSubFrameNavigation,
      expected_used_for_sub_frame_navigation_count);
  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSharedDictionaryUsedForSubresource,
      expected_used_for_subresource_count);
  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSharedDictionaryUsedWithSharedBrotli,
      expected_used_count_with_sbr);
  histograms.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kSharedDictionaryUsedWithSharedZstd,
      expected_used_count_with_zstd_d);
}

}  // namespace

// `ChromeSharedDictionaryBrowserTest` is required to test Chrome
// specific code such as Site Settings.
// See `SharedDictionaryBrowserTest` for content's version of tests.
class ChromeSharedDictionaryBrowserTest : public InProcessBrowserTest {
 public:
  ChromeSharedDictionaryBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {network::features::kCompressionDictionaryTransportBackend,
         network::features::kCompressionDictionaryTransport,
         network::features::kSharedZstd},
        /*disabled_features=*/{});

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&ChromeSharedDictionaryBrowserTest::RequestHandler,
                            base::Unretained(this)));
    EXPECT_TRUE(embedded_test_server()->Start());

    cross_origin_server_ = std::make_unique<net::EmbeddedTestServer>();
    cross_origin_server()->RegisterRequestHandler(
        base::BindRepeating(&ChromeSharedDictionaryBrowserTest::RequestHandler,
                            base::Unretained(this)));
    CHECK(cross_origin_server()->Start());
  }

  ChromeSharedDictionaryBrowserTest(const ChromeSharedDictionaryBrowserTest&) =
      delete;
  ChromeSharedDictionaryBrowserTest& operator=(
      const ChromeSharedDictionaryBrowserTest&) = delete;

 protected:
  net::EmbeddedTestServer* cross_origin_server() {
    return cross_origin_server_.get();
  }

  std::string FetchStringInActiveContents(const GURL& url) {
    return EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                  content::JsReplace(R"(
          (async () => {
            return await (await fetch($1)).text();
          })();
        )",
                                     url))
        .ExtractString();
  }

  bool TryRegisterDictionary(const net::EmbeddedTestServer& server) {
    base::RunLoop loop;
    SharedDictionaryAccessObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        loop.QuitClosure());

    EXPECT_EQ(kTestDictionaryString,
              FetchStringInActiveContents(server.GetURL("/dictionary")));
    loop.Run();
    CHECK(observer.details());
    EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kWrite,
              observer.details()->type);
    return !observer.details()->is_blocked;
  }

  bool CheckDictionaryHeader(const net::EmbeddedTestServer& server,
                             bool expect_blocked) {
    base::RunLoop loop;
    SharedDictionaryAccessObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        loop.QuitClosure());
    return CheckResultOfDictionaryHeaderAndObserver(
        FetchStringInActiveContents(server.GetURL("/path/check_header")), loop,
        observer, expect_blocked);
  }

  bool CheckDictionaryHeaderByNavigation(const GURL& url, bool expect_blocked) {
    base::RunLoop loop;
    SharedDictionaryAccessObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        loop.QuitClosure());

    CHECK(ui_test_utils::NavigateToURL(browser(), url));

    return CheckResultOfDictionaryHeaderAndObserver(
        EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
               "document.body.innerText")
            .ExtractString(),
        loop, observer, expect_blocked);
  }

  bool CheckDictionaryHeaderByIframeNavigation(const GURL& url,
                                               bool expect_blocked) {
    base::RunLoop loop;
    SharedDictionaryAccessObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        loop.QuitClosure());

    return CheckResultOfDictionaryHeaderAndObserver(
        EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
               content::JsReplace(R"(
  (async () => {
    const iframe = document.createElement('iframe');
    iframe.src = $1;
    const promise =
        new Promise(resolve => { iframe.addEventListener('load', resolve); });
    document.body.appendChild(iframe);
    await promise;
    return iframe.contentDocument.body.innerText;
  })()
           )",
                                  url))
            .ExtractString(),
        loop, observer, expect_blocked);
  }

  bool CheckResultOfDictionaryHeaderAndObserver(
      const std::string& check_result,
      base::RunLoop& loop,
      SharedDictionaryAccessObserver& observer,
      bool expect_blocked) {
    if (check_result == "Dictionary header available") {
      loop.Run();
      CHECK(observer.details());
      EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
                observer.details()->type);
      EXPECT_FALSE(observer.details()->is_blocked);
      EXPECT_FALSE(expect_blocked);
      return true;
    }
    if (expect_blocked) {
      loop.Run();
      CHECK(observer.details());
      EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
                observer.details()->type);
      EXPECT_TRUE(observer.details()->is_blocked);
    }
    return false;
  }

  void WaitForDictionaryReady(net::EmbeddedTestServer& server) {
    // Need the polling because writing the dictionary to the storage and the
    // database may take time.
    while (!CheckDictionaryHeader(server, /*expect_blocked=*/false)) {
      base::PlatformThread::Sleep(base::Milliseconds(5));
    }
  }

  int GetSiteDataCount(base::Time begin_time, base::Time end_time) {
    base::test::TestFuture<int> result;
    auto* helper = new SiteDataCountingHelper(browser()->profile(), begin_time,
                                              end_time, result.GetCallback());
    helper->CountAndDestroySelfWhenFinished();
    return result.Get();
    ;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->AddCustomHeader("access-control-allow-origin", "*");
    if (request.relative_url == "/dictionary") {
      response->set_content_type("text/plain");
      response->AddCustomHeader("use-as-dictionary", "match=\"/path/*\"");
      response->set_content(kTestDictionaryString);
      return response;
    } else if (request.relative_url == "/path/check_header") {
      response->set_content_type("text/plain");
      absl::optional<std::string> dict_hash =
          GetSecAvailableDictionary(request.headers);
      response->set_content(dict_hash ? "Dictionary header available"
                                      : "Dictionary header not available");
      return response;
    } else if (request.relative_url == "/path/check_header1.html" ||
               request.relative_url == "/path/check_header2.html") {
      response->set_content_type("text/html");
      absl::optional<std::string> dict_hash =
          GetSecAvailableDictionary(request.headers);
      response->set_content(dict_hash ? "Dictionary header available"
                                      : "Dictionary header not available");
      return response;
    } else if (request.relative_url == "/path/brotli_compressed") {
      CHECK(GetSecAvailableDictionary(request.headers));
      response->set_content_type("text/html");
      response->AddCustomHeader("content-encoding", "sbr");
      response->set_content(kBrotliCompressedDataString);
      return response;
    } else if (request.relative_url == "/path/zstd_compressed") {
      CHECK(GetSecAvailableDictionary(request.headers));
      response->set_content_type("text/html");
      response->AddCustomHeader("content-encoding", "zstd-d");
      response->set_content(kZstdCompressedDataString);
      return response;
    }
    return nullptr;
  }
  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest, BlockWriting) {
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(embedded_test_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_FALSE(TryRegisterDictionary(*embedded_test_server()));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       BlockWritingCrossOrigin) {
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(cross_origin_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_FALSE(TryRegisterDictionary(*cross_origin_server()));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest, BlockReading) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(embedded_test_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html?")));
  EXPECT_FALSE(
      CheckDictionaryHeader(*embedded_test_server(), /*expect_blocked=*/true));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       BlockReadingCrossOrigin) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*cross_origin_server()));
  WaitForDictionaryReady(*cross_origin_server());

  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(cross_origin_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html?")));
  EXPECT_FALSE(
      CheckDictionaryHeader(*cross_origin_server(), /*expect_blocked=*/true));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       BlockReadingWhileNavigation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  EXPECT_TRUE(CheckDictionaryHeaderByNavigation(
      embedded_test_server()->GetURL("/path/check_header1.html"),
      /*expect_blocked=*/false));

  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(embedded_test_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(CheckDictionaryHeaderByNavigation(
      embedded_test_server()->GetURL("/path/check_header2.html"),
      /*expect_blocked=*/true));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       BlockReadingWhileIframeNavigation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  EXPECT_TRUE(CheckDictionaryHeaderByIframeNavigation(
      embedded_test_server()->GetURL("/path/check_header1.html"),
      /*expect_blocked=*/false));

  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(embedded_test_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(CheckDictionaryHeaderByIframeNavigation(
      embedded_test_server()->GetURL("/path/check_header2.html"),
      /*expect_blocked=*/true));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       UseCounterMainFrameNavigation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  base::HistogramTester histograms;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/path/brotli_compressed")));
  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   "document.body.innerText")
                .ExtractString());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  CheckSharedDictionaryUseCounter(
      histograms,
      /*expected_used_count_with_sbr=*/1,
      /*expected_used_count_with_zstd_d=*/0,
      /*expected_used_for_navigation_count=*/1,
      /*expected_used_for_main_frame_navigation_count=*/1,
      /*expected_used_for_sub_frame_navigation_count=*/0,
      /*expected_used_for_subresource_count=*/0);
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       UseCounterSubFrameNavigation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  base::HistogramTester histograms;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   R"(
  (async () => {
    const iframe = document.createElement('iframe');
    iframe.src = '/path/brotli_compressed';
    const promise =
        new Promise(resolve => { iframe.addEventListener('load', resolve); });
    document.body.appendChild(iframe);
    await promise;
    return iframe.contentDocument.body.innerText;
  })()
                   )")
                .ExtractString());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  CheckSharedDictionaryUseCounter(
      histograms,
      /*expected_used_count_with_sbr=*/1,
      /*expected_used_count_with_zstd_d=*/0,
      /*expected_used_for_navigation_count=*/1,
      /*expected_used_for_main_frame_navigation_count=*/0,
      /*expected_used_for_sub_frame_navigation_count=*/1,
      /*expected_used_for_subresource_count=*/0);
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       UseCounterSubresource) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  base::HistogramTester histograms;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   R"(
  (async () => {
    return await (await fetch('path/brotli_compressed')).text();
  })()
                   )")
                .ExtractString());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  CheckSharedDictionaryUseCounter(
      histograms,
      /*expected_used_count_with_sbr=*/1,
      /*expected_used_count_with_zstd_d=*/0,
      /*expected_used_for_navigation_count=*/0,
      /*expected_used_for_main_frame_navigation_count=*/0,
      /*expected_used_for_sub_frame_navigation_count=*/0,
      /*expected_used_for_subresource_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       UseCounterZstdMainFrameNavigation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  base::HistogramTester histograms;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/path/zstd_compressed")));
  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   "document.body.innerText")
                .ExtractString());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  CheckSharedDictionaryUseCounter(
      histograms,
      /*expected_used_count_with_sbr=*/0,
      /*expected_used_count_with_zstd_d=*/1,
      /*expected_used_for_navigation_count=*/1,
      /*expected_used_for_main_frame_navigation_count=*/1,
      /*expected_used_for_sub_frame_navigation_count=*/0,
      /*expected_used_for_subresource_count=*/0);
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       UseCounterZstdSubresource) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  base::HistogramTester histograms;

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  EXPECT_EQ(kCompressedDataOriginalString,
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   R"(
  (async () => {
    return await (await fetch('path/zstd_compressed')).text();
  })()
                   )")
                .ExtractString());

  // Navigate away in order to flush use counters.
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  CheckSharedDictionaryUseCounter(
      histograms,
      /*expected_used_count_with_sbr=*/0,
      /*expected_used_count_with_zstd_d=*/1,
      /*expected_used_for_navigation_count=*/0,
      /*expected_used_for_main_frame_navigation_count=*/0,
      /*expected_used_for_sub_frame_navigation_count=*/0,
      /*expected_used_for_subresource_count=*/1);
}

// TODO(crbug.com/1472445): Fix flakiness in test and enable.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SiteDataCount DISABLED_SiteDataCount
#else
#define MAYBE_SiteDataCount SiteDataCount
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest, MAYBE_SiteDataCount) {
  base::Time time1 = base::Time::Now();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  base::Time time2 = base::Time::Now();
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());
  base::Time time3 = base::Time::Now();

  EXPECT_EQ(0, GetSiteDataCount(time1, time2));
  EXPECT_EQ(1, GetSiteDataCount(time2, time3));
}
