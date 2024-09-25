// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/extras/shared_dictionary/shared_dictionary_usage_info.h"
#include "net/shared_dictionary/shared_dictionary_constants.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/url_constants.h"

namespace {

constexpr std::string_view kTestDictionaryString = "A dictionary";

constexpr std::string_view kCompressedDataOriginalString =
    "This is compressed test data using a test dictionary";

// kBrotliCompressedData is generated using the following commands:
// $ echo -n "A dictionary" > /tmp/dict
// $ echo -n "This is compressed test data using a test dictionary" > /tmp/data
// $ echo -en '\xffDCB' > /tmp/out.dcb
// $ openssl dgst -sha256 -binary /tmp/dict >> /tmp/out.dcb
// $ brotli --stdout -D /tmp/dict /tmp/data >> /tmp/out.dcb
// $ xxd -i  /tmp/out.dcb
constexpr uint8_t kBrotliCompressedData[] = {
    0xff, 0x44, 0x43, 0x42, 0x0a, 0xa3, 0x69, 0x01, 0x4f, 0x7f, 0xab,
    0x37, 0x0b, 0xe9, 0x40, 0x74, 0x69, 0x85, 0x45, 0xc7, 0xbb, 0x93,
    0x2e, 0xc4, 0x61, 0x25, 0x27, 0x8f, 0x37, 0xbf, 0x34, 0xab, 0x02,
    0xa3, 0x5a, 0xec, 0xa1, 0x98, 0x01, 0x80, 0x22, 0xe0, 0x26, 0x4b,
    0x95, 0x5c, 0x19, 0x18, 0x9d, 0xc1, 0xc3, 0x44, 0x0e, 0x5c, 0x6a,
    0x09, 0x9d, 0xf0, 0xb0, 0x01, 0x47, 0x14, 0x87, 0x14, 0x6d, 0xfb,
    0x60, 0x96, 0xdb, 0xae, 0x9e, 0x79, 0x54, 0xe3, 0x69, 0x03, 0x29};

// NOLINTNEXTLINE(runtime/string)
const std::string kBrotliCompressedDataString =
    std::string(reinterpret_cast<const char*>(kBrotliCompressedData),
                sizeof(kBrotliCompressedData));

// kZstdCompressedData is generated using the following commands:
// $ echo -n "A dictionary" > /tmp/dict
// $ echo -n "This is compressed test data using a test dictionary" > /tmp/data
// $ echo -en '\x5e\x2a\x4d\x18\x20\x00\x00\x00' > /tmp/out.dcz
// $ openssl dgst -sha256 -binary /tmp/dict >> /tmp/out.dcz
// $ zstd -D /tmp/dict -f -o /tmp/tmp.zstd /tmp/data
// $ cat /tmp/tmp.zstd >> /tmp/out.dcz
// $ xxd -i /tmp/out.dcz
constexpr uint8_t kZstdCompressedData[] = {
    0x5e, 0x2a, 0x4d, 0x18, 0x20, 0x00, 0x00, 0x00, 0x0a, 0xa3, 0x69, 0x01,
    0x4f, 0x7f, 0xab, 0x37, 0x0b, 0xe9, 0x40, 0x74, 0x69, 0x85, 0x45, 0xc7,
    0xbb, 0x93, 0x2e, 0xc4, 0x61, 0x25, 0x27, 0x8f, 0x37, 0xbf, 0x34, 0xab,
    0x02, 0xa3, 0x5a, 0xec, 0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x34, 0xa1, 0x01,
    0x00, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x63, 0x6f, 0x6d,
    0x70, 0x72, 0x65, 0x73, 0x73, 0x65, 0x64, 0x20, 0x74, 0x65, 0x73, 0x74,
    0x20, 0x64, 0x61, 0x74, 0x61, 0x20, 0x75, 0x73, 0x69, 0x6e, 0x67, 0x20,
    0x61, 0x20, 0x74, 0x65, 0x73, 0x74, 0x20, 0x64, 0x69, 0x63, 0x74, 0x69,
    0x6f, 0x6e, 0x61, 0x72, 0x79, 0x9e, 0x99, 0xf2, 0xbc};

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

std::optional<std::string> GetAvailableDictionary(
    const net::test_server::HttpRequest::HeaderMap& headers) {
      auto it = headers.find("available-dictionary");
      return it == headers.end() ? std::nullopt
                                 : std::make_optional(it->second);
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

std::string FetchUrlScript(const GURL& url) {
  return content::JsReplace(R"(
          (async () => {
            try {
              await fetch($1);
            } catch (e) {
            }
          })();
        )",
                            url);
}

std::string FetchUrlWithNoCorsModeScript(const GURL& url) {
  return content::JsReplace(R"(
          (async () => {
            try {
              await fetch($1, {mode: 'no-cors'});
            } catch (e) {
            }
          })();
        )",
                            url);
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
      response->AddCustomHeader("cache-control", "max-age=3600");
      response->set_content(kTestDictionaryString);
      return response;
    } else if (request.relative_url == "/path/check_header") {
      response->set_content_type("text/plain");
      std::optional<std::string> dict_hash =
          GetAvailableDictionary(request.headers);
      response->set_content(dict_hash ? "Dictionary header available"
                                      : "Dictionary header not available");
      return response;
    } else if (request.relative_url == "/path/check_header1.html" ||
               request.relative_url == "/path/check_header2.html") {
      response->set_content_type("text/html");
      std::optional<std::string> dict_hash =
          GetAvailableDictionary(request.headers);
      response->set_content(dict_hash ? "Dictionary header available"
                                      : "Dictionary header not available");
      return response;
    } else if (request.relative_url == "/path/brotli_compressed") {
      CHECK(GetAvailableDictionary(request.headers));
      response->set_content_type("text/html");
      response->AddCustomHeader(
          "content-encoding",
          net::shared_dictionary::kSharedBrotliContentEncodingName);
      response->set_content(kBrotliCompressedDataString);
      return response;
    } else if (request.relative_url == "/path/zstd_compressed") {
      CHECK(GetAvailableDictionary(request.headers));
      response->set_content_type("text/html");
      response->AddCustomHeader(
          "content-encoding",
          net::shared_dictionary::kSharedZstdContentEncodingName);
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

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest, SiteDataCount) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  base::RunLoop loop;
  std::vector<network::mojom::SharedDictionaryInfoPtr> dictionaries;
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetSharedDictionaryInfo(
          net::SharedDictionaryIsolationKey(url::Origin::Create(url),
                                            net::SchemefulSite(url)),
          base::BindLambdaForTesting(
              [&](std::vector<network::mojom::SharedDictionaryInfoPtr> result) {
                EXPECT_FALSE(result.empty());
                dictionaries = std::move(result);
                loop.Quit();
              }));
  loop.Run();
  ASSERT_EQ(dictionaries.size(), 1u);
  ASSERT_TRUE(dictionaries[0]);
  const base::Time response_time = dictionaries[0]->response_time;
  EXPECT_EQ(0,
            GetSiteDataCount(response_time - base::Seconds(1), response_time));
  EXPECT_EQ(1,
            GetSiteDataCount(response_time, response_time + base::Seconds(1)));
}

class SharedDictionaryDevToolsBrowserTest
    : public InProcessBrowserTest,
      public content::TestDevToolsProtocolClient {
 public:
  explicit SharedDictionaryDevToolsBrowserTest(bool enable_feature = true) {
    if (enable_feature) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {network::features::kCompressionDictionaryTransportBackend,
           network::features::kCompressionDictionaryTransport},
          /*disabled_features=*/
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {network::features::kCompressionDictionaryTransportBackend},
          /*disabled_features=*/
          {network::features::kCompressionDictionaryTransport});
    }
  }
  ~SharedDictionaryDevToolsBrowserTest() override = default;
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "content/test/data");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        network::switches::kDisableSharedDictionaryStorageCleanupForTesting);
  }
  void TearDownOnMainThread() override {
    DetachProtocolClient();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::RenderFrameHost* GetPrimaryMainFrame() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }
  void NavigateAndEnableAudits(const GURL& url) {
    EXPECT_TRUE(NavigateToURL(
        browser()->tab_strip_model()->GetActiveWebContents(), url));
    AttachToWebContents(browser()->tab_strip_model()->GetActiveWebContents());
    SendCommandSync("Network.enable");
    SendCommandSync("Audits.enable");
  }
  base::Value::Dict WaitForSharedDictionaryIssueAdded(
      const std::string& expected_error_type) {
    auto matcher = [](const base::Value::Dict& params) {
      const std::string* maybe_issue_code =
          params.FindStringByDottedPath("issue.code");
      return maybe_issue_code && *maybe_issue_code == "SharedDictionaryIssue";
    };
    base::Value::Dict notification = WaitForMatchingNotification(
        "Audits.issueAdded", base::BindRepeating(matcher));
    EXPECT_EQ(*notification.FindStringByDottedPath("issue.code"),
              "SharedDictionaryIssue");
    const std::string* maybe_error_type = notification.FindStringByDottedPath(
        "issue.details.sharedDictionaryIssueDetails.sharedDictionaryError");
    CHECK(maybe_error_type);
    EXPECT_EQ(*maybe_error_type, expected_error_type);
    return notification;
  }
  void RunCustomHeaderTest(
      const std::string& expected_erorr_type,
      const std::string& use_as_dictionary_header,
      const std::string& cache_control_header = "max-age=3600") {
    embedded_https_test_server().RegisterRequestHandler(
        base::BindLambdaForTesting(
            [&](const net::test_server::HttpRequest& request)
                -> std::unique_ptr<net::test_server::HttpResponse> {
              if (request.relative_url == "/test.dict") {
                auto response =
                    std::make_unique<net::test_server::BasicHttpResponse>();
                response->AddCustomHeader("use-as-dictionary",
                                          use_as_dictionary_header);
                response->AddCustomHeader("cache-control",
                                          cache_control_header);
                response->set_content("dict");
                return response;
              }
              return nullptr;
            }));
    auto handle = embedded_https_test_server().StartAndReturnHandle();
    ASSERT_TRUE(handle);
    NavigateAndEnableAudits(
        embedded_https_test_server().GetURL("/shared_dictionary/blank.html"));
    EXPECT_TRUE(ExecJs(
        GetPrimaryMainFrame(),
        FetchUrlScript(embedded_https_test_server().GetURL("/test.dict"))));
    WaitForSharedDictionaryIssueAdded(expected_erorr_type);
  }

  std::vector<net::SharedDictionaryUsageInfo> GetSharedDictionaryUsageInfo() {
    base::test::TestFuture<const std::vector<net::SharedDictionaryUsageInfo>&>
        result;
    browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetSharedDictionaryUsageInfo(result.GetCallback());
    return result.Get();
  }

  void WaitUntilDictionaryRegistered() {
    while (GetSharedDictionaryUsageInfo().empty()) {
      base::PlatformThread::Sleep(base::Milliseconds(10));
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DevToolsSharedDictionaryFeatureDisabledBrowserTest
    : public SharedDictionaryDevToolsBrowserTest {
 public:
  DevToolsSharedDictionaryFeatureDisabledBrowserTest()
      : SharedDictionaryDevToolsBrowserTest(/*enable_feature=*/false) {}
  ~DevToolsSharedDictionaryFeatureDisabledBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       UseErrorCrossOriginNoCorsRequest) {
  const std::string kHostName = "www.example.com";
  const std::string kCrossOriginHostName = "other.example.com";
  embedded_https_test_server().SetCertHostnames(
      {kHostName, kCrossOriginHostName});
  ASSERT_TRUE(embedded_https_test_server().Start());
  NavigateAndEnableAudits(embedded_https_test_server().GetURL(
      kHostName, "/shared_dictionary/blank.html"));
  content::RenderFrameHost* rfh = GetPrimaryMainFrame();
  EXPECT_TRUE(
      ExecJs(rfh, FetchUrlScript(embedded_https_test_server().GetURL(
                      kCrossOriginHostName, "/shared_dictionary/test.dict"))));
  WaitUntilDictionaryRegistered();
  EXPECT_TRUE(ExecJs(
      rfh, FetchUrlWithNoCorsModeScript(embedded_https_test_server().GetURL(
               kCrossOriginHostName, "/shared_dictionary/path/target"))));
  WaitForSharedDictionaryIssueAdded("UseErrorCrossOriginNoCorsRequest");
}

// Can't cause the dictionary load failure by deletaing the disk cache directory
// on Windows.
#if !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       UseErrorDictionaryLoadFailure) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndEnableAudits(
      embedded_test_server()->GetURL("/shared_dictionary/blank.html"));
  content::RenderFrameHost* rfh = GetPrimaryMainFrame();
  EXPECT_TRUE(ExecJs(rfh, FetchUrlScript(embedded_test_server()->GetURL(
                              "/shared_dictionary/test.dict"))));
  WaitUntilDictionaryRegistered();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::DeletePathRecursively(
        browser()->profile()->GetDefaultStoragePartition()->GetPath().Append(
            FILE_PATH_LITERAL("Shared Dictionary/cache/"))));
  }
  EXPECT_TRUE(ExecJs(rfh, FetchUrlScript(embedded_test_server()->GetURL(
                              "/shared_dictionary/path/compressed.data"))));
  WaitForSharedDictionaryIssueAdded("UseErrorDictionaryLoadFailure");
}
#endif  // !BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       UseErrorMatchingDictionaryNotUsed) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/shared_dictionary/path/target") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_content("data");
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndEnableAudits(
      embedded_test_server()->GetURL("/shared_dictionary/blank.html"));
  content::RenderFrameHost* rfh = GetPrimaryMainFrame();
  EXPECT_TRUE(ExecJs(rfh, FetchUrlScript(embedded_test_server()->GetURL(
                              "/shared_dictionary/test.dict"))));
  WaitUntilDictionaryRegistered();
  EXPECT_TRUE(ExecJs(rfh, FetchUrlScript(embedded_test_server()->GetURL(
                              "/shared_dictionary/path/target"))));
  WaitForSharedDictionaryIssueAdded("UseErrorMatchingDictionaryNotUsed");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       UseErrorUnexpectedContentDictionaryHeader) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/shared_dictionary/path/target") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->AddCustomHeader("Content-Encoding", "dcb");
          std::string data =
              std::string(base::as_string_view(kBrotliCompressedData));
          // Change the first byte of the compressed data to trigger
          // UNEXPECTED_CONTENT_DICTIONARY_HEADER error.
          ++data[0];
          response->set_content(data);
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndEnableAudits(
      embedded_test_server()->GetURL("/shared_dictionary/blank.html"));
  content::RenderFrameHost* rfh = GetPrimaryMainFrame();
  EXPECT_TRUE(ExecJs(rfh, FetchUrlScript(embedded_test_server()->GetURL(
                              "/shared_dictionary/test.dict"))));
  WaitUntilDictionaryRegistered();
  EXPECT_TRUE(ExecJs(rfh, FetchUrlScript(embedded_test_server()->GetURL(
                              "/shared_dictionary/path/target"))));
  WaitForSharedDictionaryIssueAdded(
      "UseErrorUnexpectedContentDictionaryHeader");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorCossOriginNoCorsRequest) {
  const std::string kCrossOriginHostName = "example.com";
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndEnableAudits(
      embedded_test_server()->GetURL("/shared_dictionary/blank.html"));
  EXPECT_TRUE(
      ExecJs(GetPrimaryMainFrame(),
             FetchUrlWithNoCorsModeScript(embedded_test_server()->GetURL(
                 kCrossOriginHostName, "/shared_dictionary/test.dict"))));
  WaitForSharedDictionaryIssueAdded("WriteErrorCossOriginNoCorsRequest");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorDisallowedBySettings) {
  const std::string kHostName = "www.example.com";
  const std::string kCrossOriginHostName = "other.example.com";
  embedded_https_test_server().SetCertHostnames(
      {kHostName, kCrossOriginHostName});
  ASSERT_TRUE(embedded_https_test_server().Start());

  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(
      embedded_https_test_server().GetURL(kCrossOriginHostName, "/"),
      CONTENT_SETTING_BLOCK);

  NavigateAndEnableAudits(embedded_https_test_server().GetURL(
      kHostName, "/shared_dictionary/blank.html"));
  EXPECT_TRUE(
      ExecJs(browser()
                 ->tab_strip_model()
                 ->GetActiveWebContents()
                 ->GetPrimaryMainFrame(),
             FetchUrlScript(embedded_https_test_server().GetURL(
                 kCrossOriginHostName, "/shared_dictionary/test.dict"))));
  WaitForSharedDictionaryIssueAdded("WriteErrorDisallowedBySettings");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorExpiredResponse) {
  RunCustomHeaderTest("WriteErrorExpiredResponse",
                      /*use_as_dictionary_header=*/"match=\"/test/*\"",
                      /*cache_control_header=*/"no-store");
}

IN_PROC_BROWSER_TEST_F(DevToolsSharedDictionaryFeatureDisabledBrowserTest,
                       WriteErrorFeatureDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndEnableAudits(
      embedded_test_server()->GetURL("/shared_dictionary/blank.html"));
  EXPECT_TRUE(ExecJs(GetPrimaryMainFrame(),
                     FetchUrlScript(embedded_test_server()->GetURL(
                         "/shared_dictionary/test.dict"))));
  WaitForSharedDictionaryIssueAdded("WriteErrorFeatureDisabled");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorInvalidMatchField) {
  RunCustomHeaderTest("WriteErrorInvalidMatchField", "match=\"(\"");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorInvalidStructuredHeader) {
  RunCustomHeaderTest("WriteErrorInvalidStructuredHeader", "match=\"");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorNavigationRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndEnableAudits(
      embedded_test_server()->GetURL("/shared_dictionary/blank.html"));
  EXPECT_TRUE(NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(),
      embedded_test_server()->GetURL("/shared_dictionary/test_dict.html")));
  WaitForSharedDictionaryIssueAdded("WriteErrorNavigationRequest");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorNoMatchField) {
  RunCustomHeaderTest("WriteErrorNoMatchField", "id=\"dict_id\"");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorNonListMatchDestField) {
  RunCustomHeaderTest("WriteErrorNonListMatchDestField",
                      "match=\"/test/*\", match-dest=document");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorNonSecureContext) {
  const std::string kHostName = "example.com";
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndEnableAudits(embedded_test_server()->GetURL(
      kHostName, "/shared_dictionary/blank.html"));
  EXPECT_TRUE(ExecJs(GetPrimaryMainFrame(),
                     FetchUrlScript(embedded_test_server()->GetURL(
                         kHostName, "/shared_dictionary/test.dict"))));
  WaitForSharedDictionaryIssueAdded("WriteErrorNonSecureContext");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorNonStringIdField) {
  RunCustomHeaderTest("WriteErrorNonStringIdField",
                      "match=\"/test/*\", id=dict_id");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorNonStringInMatchDestList) {
  RunCustomHeaderTest("WriteErrorNonStringInMatchDestList",
                      "match=\"/test/*\", match-dest=(document)");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorNonStringMatchField) {
  RunCustomHeaderTest("WriteErrorNonStringMatchField", "match=test");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorNonTokenTypeField) {
  RunCustomHeaderTest("WriteErrorNonTokenTypeField",
                      "match=\"/test/*\", type=\"raw\"");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorRequestAborted) {
  auto dictionary_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/test.dict");
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndEnableAudits(
      embedded_test_server()->GetURL("/shared_dictionary/blank.html"));
  content::RenderFrameHost* rfh = GetPrimaryMainFrame();
  EXPECT_TRUE(ExecJs(
      rfh, content::JsReplace(R"(
          (() => {
            const controller = new AbortController();
            window.FETCH_ABORT_CONTROLLER = controller;
            window.FETCH_RESULT = fetch($1, {signal: controller.signal});
          })();
        )",
                              embedded_test_server()->GetURL("/test.dict"))));
  dictionary_response->WaitForRequest();
  dictionary_response->Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Use-As-Dictionary: match=\"/test/*\"\r\n"
      "Cache-Control: max-age=3600\r\n"
      "\r\n"
      "dict");
  EXPECT_TRUE(ExecJs(rfh, R"(
          (async () => {
            try {
              const response = await window.FETCH_RESULT;
              const reader = response.body.getReader();
              const result = await reader.read();
              window.FETCH_ABORT_CONTROLLER.abort();
            } catch (e) {
            }
          })();
        )"));
  WaitForSharedDictionaryIssueAdded("WriteErrorRequestAborted");
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorTooLongIdField) {
  RunCustomHeaderTest(
      "WriteErrorTooLongIdField",
      base::StrCat({"match=\"/test/*\", id=\"", std::string(1025, 'a'), "\""}));
}

IN_PROC_BROWSER_TEST_F(SharedDictionaryDevToolsBrowserTest,
                       WriteErrorUnsupportedType) {
  RunCustomHeaderTest("WriteErrorUnsupportedType",
                      "match=\"/test/*\", type=unsupported");
}
