// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/metrics/accept_language_and_content_language_usage.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/origin.h"

namespace {

using ::content::URLLoaderInterceptor;
using ::net::test_server::EmbeddedTestServer;

struct RecordLanguageMetricTestOptions {
  bool has_content_language_in_parent = true;
  bool has_content_language_in_child = true;
  std::string parent_content_language_value;
  std::string child_content_language_value;
  bool has_xml_lang = false;
  bool has_html_lang = false;
  std::string xml_lang_value;
  std::string html_lang_value;
};

const char kLargeLanguages[] =
    "zh,zh-CN,en-US,en,af,sq,am,ar,an,hy,ast,az,bn,bs,be,eu,br,bg,nl,da,cs,hr,"
    "co,en-AU,en-CA,en-IN,en-NZ,en-ZA,en-GB-oxendict,en-GB,eo,et,fo,fil,fi,fr,"
    "fr-FR,fr-CA,fr-CH,gl,ka,de,gu,gn,el,de-CH,de-LI,de-DE,ht,is,hu,hmn,hi,he,"
    "haw,ig,ja,it-CH,it-IT,it,ga,jv,kn,kk,km,rw,ko,ku,ky,lo,mk,lb,lt,ln,lv,mg,"
    "ms,no,ne,mn,mr,mi,mt,nb,or,oc,ny,nn,pl,fa,ps,om,pt,pt-BR,my,ca,ckb,chr,"
    "ceb,zh-HK,zh-TW,la,ia,id,ha,de-AT,ml,pt-PT,sd,sn,sh,sr,gd,sm,ru,rm,mo,ro,"
    "qu,pa,es-VE,es-UY,es-US,es-ES,es-419,es-MX,es-PE,es-HN,es-CR,es-AR,es,st,"
    "so,sl,sk,si,wa,vi,uz,ug,uk,ur,yi,xh,wo,fy,cy,yo,zu,es-CL,es-CO,su,ta,sv,"
    "sw,tg,tn,to,ti,th,te,tt,tr,tk,tw";

// As translate agent reports the metrics can be later than
// MergeHistogramDeltasForTesting, retries fetching |histogram_name| until it
// contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    int total = 0;
    for (const auto& bucket : buckets)
      total += bucket.count;

    if (total >= count)
      return total;

    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

// Browser tests to verify whether language metrics has been recorded correctly.
// Since render writes the metrics in DocumentLoader, it will only pick
// the top-most content language and set as the document content language. From
// navigate request, we will only see the top-most content language, but it's
// probably enough to check whether the parent frame has the same content
// language setting as child frame.
// https://httpwg.org/specs/rfc7231.html#header.content-language
class RecordLanguagesMetricsBrowserTest : public InProcessBrowserTest {
 public:
  RecordLanguagesMetricsBrowserTest() {
    // TODO(crbug.com/334954143) This tests is used to verify the metrics before
    // we launch ReduceAcceptLanguage. Fix the tests when turning on the reduce
    // accept-language feature.
    scoped_feature_list_.InitWithFeatures(
        {}, {network::features::kReduceAcceptLanguage});
  }

  static constexpr const char kOriginUrl[] = "https://127.0.0.1:44444";
  static constexpr const char kLanguageHistorgramName[] =
      "LanguageUsage.AcceptLanguageAndContentLanguageUsage";

  static constexpr const char kXmlHtmlLanguageHistorgramName[] =
      "LanguageUsage.AcceptLanguageAndXmlHtmlLangUsage";

  void SetUpOnMainThread() override {
    // We use a URLLoaderInterceptor, we also can use the EmbeddedTestServer.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            &RecordLanguagesMetricsBrowserTest::InterceptRequest,
            base::Unretained(this)));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetTestOptions(const RecordLanguageMetricTestOptions& test_options,
                      const std::set<GURL>& expected_request_urls) {
    test_options_ = test_options;
    expected_request_urls_ = expected_request_urls;
  }

  GURL content_language_url() const {
    return GURL(base::StrCat({kOriginUrl, "/content_language.html"}));
  }

  GURL xml_html_language_url() const {
    return GURL(base::StrCat({kOriginUrl, "/xml_html_language.html"}));
  }

  GURL content_language_with_iframe_url() const {
    return GURL(
        base::StrCat({kOriginUrl, "/content_language_with_iframe.html"}));
  }

  GURL simple_request_url() const {
    return GURL(base::StrCat({kOriginUrl, "/simple.html"}));
  }

  GURL last_request_url() const {
    return url_loader_interceptor_->GetLastRequestURL();
  }

  void NavigateAndVerifyHistogramsMetric(
      const GURL& url,
      const blink::AcceptLanguageAndContentLanguageUsage expect_metric_type,
      const int expect_metric_count) {
    base::HistogramTester histograms;

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    histograms.ExpectBucketCount(kLanguageHistorgramName, expect_metric_type,
                                 expect_metric_count);
  }

  void NavigateAndVerifyXmlHtmlHistogramsMetric(
      const GURL& url,
      const blink::AcceptLanguageAndXmlHtmlLangUsage expect_metric_type,
      const int expect_metric_count) {
    base::HistogramTester histograms;

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    RetryForHistogramUntilCountReached(&histograms,
                                       kXmlHtmlLanguageHistorgramName, 1);
    histograms.ExpectBucketCount(kXmlHtmlLanguageHistorgramName,
                                 expect_metric_type, expect_metric_count);
  }

  void VerifyAcceptLanguage(const std::string& expected_accept_language) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(
        content::EvalJs(web_contents, "navigator.language").ExtractString(),
        expected_accept_language);
  }

  void SetPerfsAcceptLanguage(
      const std::vector<std::string>& accept_languages) {
    std::unique_ptr<language::LanguagePrefs> language_prefs =
        std::make_unique<language::LanguagePrefs>(
            browser()->profile()->GetPrefs());
    language_prefs->SetUserSelectedLanguagesList(accept_languages);
  }

 private:
  // URLLoaderInterceptor callback
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    if (expected_request_urls_.find(params->url_request.url) ==
        expected_request_urls_.end())
      return false;

    std::string path = "chrome/test/data/content_language";
    path.append(static_cast<std::string>(params->url_request.url.path_piece()));

    // build response header and body for xml html lang tag value
    if (params->url_request.url.path() == "/xml_html_language.html") {
      URLLoaderInterceptor::WriteResponse(
          BuildXmlHtmlHeader(), BuildXmlHtmlBody(), params->client.get());
      return true;
    }

    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    if (params->url_request.url.path() == "/simple.html") {
      base::StrAppend(&headers, {BuildIframeResponseHeader()});
    } else {
      base::StrAppend(&headers, {BuildResponseHeader()});
    }

    URLLoaderInterceptor::WriteResponse(path, params->client.get(), &headers,
                                        std::nullopt,
                                        /*url=*/params->url_request.url);
    return true;
  }

  std::string BuildXmlHtmlBody() {
    std::string body = "<html ";
    if (test_options_.has_xml_lang) {
      base::StrAppend(&body,
                      {" xmlns='http://www.w3.org/1999/xhtml' xml:lang='",
                       test_options_.xml_lang_value, "' "});
    }
    if (test_options_.has_html_lang) {
      base::StrAppend(&body, {" lang='", test_options_.html_lang_value, "' "});
    }
    base::StrAppend(&body, {" ><head></head></html>"});
    return body;
  }

  std::string BuildXmlHtmlHeader() {
    std::string headers = "HTTP/1.1 200 OK\n";
    if (test_options_.has_xml_lang) {
      base::StrAppend(&headers, {"Content-Type: application/xhtml+xml\n"});
    } else {
      base::StrAppend(&headers, {"Content-Type: text/html\n"});
    }

    if (test_options_.has_content_language_in_parent) {
      base::StrAppend(&headers,
                      {"Content-Language: ",
                       test_options_.parent_content_language_value, "\n"});
    }

    return headers;
  }

  std::string BuildResponseHeader() {
    std::string headers;

    if (!test_options_.has_content_language_in_parent) {
      return headers;
    }
    base::StrAppend(&headers,
                    {"Content-Language: ",
                     test_options_.parent_content_language_value, "\n"});
    return headers;
  }

  std::string BuildIframeResponseHeader() {
    std::string headers;

    if (!test_options_.has_content_language_in_child) {
      return headers;
    }

    base::StrAppend(&headers,
                    {"Content-Language: ",
                     test_options_.child_content_language_value, "\n"});
    return headers;
  }

  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  std::set<GURL> expected_request_urls_;
  RecordLanguageMetricTestOptions test_options_;
  std::unique_ptr<language::LanguagePrefs> language_prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

constexpr const char RecordLanguagesMetricsBrowserTest::kOriginUrl[];
constexpr const char
    RecordLanguagesMetricsBrowserTest::kLanguageHistorgramName[];
constexpr const char
    RecordLanguagesMetricsBrowserTest::kXmlHtmlLanguageHistorgramName[];

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       ContentLanguageEmpty) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/""},
                 {content_language_url()});

  NavigateAndVerifyHistogramsMetric(
      content_language_url(),
      blink::AcceptLanguageAndContentLanguageUsage::kContentLanguageEmpty, 1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       ContentLanguageIsWildcard) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"*",
                  /*child_content_language_value=*/""},
                 {content_language_url()});

  NavigateAndVerifyHistogramsMetric(
      content_language_url(),
      blink::AcceptLanguageAndContentLanguageUsage::kContentLanguageWildcard,
      1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       ContentLanguageMatchTopMostAcceptLanguage) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"en-US",
                  /*child_content_language_value=*/""},
                 {content_language_url()});

  // The default Accept-Language is (en-US).
  VerifyAcceptLanguage("en-US");
  NavigateAndVerifyHistogramsMetric(
      content_language_url(),
      blink::AcceptLanguageAndContentLanguageUsage::
          kContentLanguageMatchesPrimaryAcceptLanguage,
      1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       ContentLanguageMatchesAnyAcceptLanguage) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"en-US",
                  /*child_content_language_value=*/""},
                 {content_language_url()});

  // The default Accept-Language is (en-US).
  VerifyAcceptLanguage("en-US");
  NavigateAndVerifyHistogramsMetric(
      content_language_url(),
      blink::AcceptLanguageAndContentLanguageUsage::
          kContentLanguageMatchesAnyAcceptLanguage,
      1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       IframeContentLanguageChildDifferFromParent) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/true,
                  /*parent_content_language_value=*/"fr",
                  /*child_content_language_value=*/"zh-CN"},
                 {content_language_with_iframe_url(), simple_request_url()});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           content_language_with_iframe_url()));
  EXPECT_EQ(last_request_url().path(), "/simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Child Content-Language vs Parent Content-Language
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageSubframeDiffers,
                               1);
}

IN_PROC_BROWSER_TEST_F(
    RecordLanguagesMetricsBrowserTest,
    IframeParentHasConentLanguageAndChildContentLanguageIsEmpty) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"en-US",
                  /*child_content_language_value=*/""},
                 {content_language_with_iframe_url(), simple_request_url()});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           content_language_with_iframe_url()));
  EXPECT_EQ(last_request_url().path(), "/simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Accept-Language vs Parent Content-Language: en-US == en-US
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageMatchesPrimaryAcceptLanguage,
                               1);

  // Accept-Language vs Child Content-Language: empty
  histograms.ExpectBucketCount(
      kLanguageHistorgramName,
      blink::AcceptLanguageAndContentLanguageUsage::kContentLanguageEmpty, 1);

  // Child Content-Language vs Parent Content-Language
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageSubframeDiffers,
                               1);
}

IN_PROC_BROWSER_TEST_F(
    RecordLanguagesMetricsBrowserTest,
    IframeParentContentLanguageIsEmptyAndChildContentLanguageHasValue) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/true,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"en-US"},
                 {content_language_with_iframe_url(), simple_request_url()});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           content_language_with_iframe_url()));
  EXPECT_EQ(last_request_url().path(), "/simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Accept-Language vs Parent Content-Language: empty
  histograms.ExpectBucketCount(
      kLanguageHistorgramName,
      blink::AcceptLanguageAndContentLanguageUsage::kContentLanguageEmpty, 1);

  // Accept-Language vs Child Content-Language: en-US == en-US
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageMatchesPrimaryAcceptLanguage,
                               1);

  // Child Content-Language vs Parent Content-Language
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageSubframeDiffers,
                               1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       IframeBothParentAndChildContentLanguageIsEmpty) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/""},
                 {content_language_with_iframe_url(), simple_request_url()});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           content_language_with_iframe_url()));
  EXPECT_EQ(last_request_url().path(), "/simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Accept-Language vs Parent Content-Language: empty
  // Accept-Language vs Child Content-Language: empty
  histograms.ExpectBucketCount(
      kLanguageHistorgramName,
      blink::AcceptLanguageAndContentLanguageUsage::kContentLanguageEmpty, 2);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       IframeParentAndChildContentLanguageHasSameValue) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/true,
                  /*parent_content_language_value=*/"en-US",
                  /*child_content_language_value=*/"en-US"},
                 {content_language_with_iframe_url(), simple_request_url()});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           content_language_with_iframe_url()));
  EXPECT_EQ(last_request_url().path(), "/simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The default Accept-Language is (en-US).
  VerifyAcceptLanguage("en-US");

  // Accept-Language vs Parent Content-Language: match top-most
  // Accept-Language vs Child Content-Language: match top-most
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageMatchesPrimaryAcceptLanguage,
                               2);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       IframeParentAndChildContentLanguageAreStar) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/true,
                  /*parent_content_language_value=*/"*",
                  /*child_content_language_value=*/"*"},
                 {content_language_with_iframe_url(), simple_request_url()});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           content_language_with_iframe_url()));
  EXPECT_EQ(last_request_url().path(), "/simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The default Accept-Language is (en-US).
  VerifyAcceptLanguage("en-US");

  // Accept-Language vs Parent Content-Language: *
  // Accept-Language vs Child Content-Language: *
  histograms.ExpectBucketCount(
      kLanguageHistorgramName,
      blink::AcceptLanguageAndContentLanguageUsage::kContentLanguageWildcard,
      2);
}

IN_PROC_BROWSER_TEST_F(
    RecordLanguagesMetricsBrowserTest,
    IframeParentHasConentLanguageAndChildContentLanguageIsStar) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/true,
                  /*parent_content_language_value=*/"en-US",
                  /*child_content_language_value=*/"*"},
                 {content_language_with_iframe_url(), simple_request_url()});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           content_language_with_iframe_url()));
  EXPECT_EQ(last_request_url().path(), "/simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Accept-Language vs Parent Content-Language: en-US == en-US
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageMatchesPrimaryAcceptLanguage,
                               1);

  // Accept-Language vs Child Content-Language: *
  histograms.ExpectBucketCount(
      kLanguageHistorgramName,
      blink::AcceptLanguageAndContentLanguageUsage::kContentLanguageWildcard,
      1);

  // Child Content-Language vs Parent Content-Language
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageSubframeDiffers,
                               1);
}

IN_PROC_BROWSER_TEST_F(
    RecordLanguagesMetricsBrowserTest,
    IframeParentContentLanuageIsStarAndChildContentLanguageHasValue) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/true,
                  /*parent_content_language_value=*/"*",
                  /*child_content_language_value=*/"en-US"},
                 {content_language_with_iframe_url(), simple_request_url()});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           content_language_with_iframe_url()));
  EXPECT_EQ(last_request_url().path(), "/simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Accept-Language vs Parent Content-Language: *
  histograms.ExpectBucketCount(
      kLanguageHistorgramName,
      blink::AcceptLanguageAndContentLanguageUsage::kContentLanguageWildcard,
      1);

  // Accept-Language vs Child Content-Language: en-US == en-US
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageMatchesPrimaryAcceptLanguage,
                               1);

  // Child Content-Language vs Parent Content-Language
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageSubframeDiffers,
                               1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       LargeAcceptLanguageList) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/true,
                  /*parent_content_language_value=*/kLargeLanguages,
                  /*child_content_language_value=*/kLargeLanguages},
                 {content_language_with_iframe_url(), simple_request_url()});

  SetPerfsAcceptLanguage(base::SplitString(
      kLargeLanguages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
  VerifyAcceptLanguage("zh");

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           content_language_with_iframe_url()));
  EXPECT_EQ(last_request_url().path(), "/simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Accept-Language vs Parent Content-Language: match top-most
  // Accept-Language vs Child Content-Language: match top-most
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageMatchesAnyAcceptLanguage,
                               2);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest, XmlLangEmpty) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/true,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"",
                  /*html_lang_value=*/"en-US"},
                 {xml_html_language_url()});

  NavigateAndVerifyXmlHtmlHistogramsMetric(
      xml_html_language_url(),
      blink::AcceptLanguageAndXmlHtmlLangUsage::kXmlLangEmpty, 1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest, XmlLangIsDash) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/true,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"-",
                  /*html_lang_value=*/"en-US"},
                 {xml_html_language_url()});
  // Make sure no crash and no metric reports.
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xml_html_language_url()));
  RetryForHistogramUntilCountReached(&histograms,
                                     kXmlHtmlLanguageHistorgramName, 0);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest, HtmlLangIsDash) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/true,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"",
                  /*html_lang_value=*/"-"},
                 {xml_html_language_url()});
  // Make sure no crash and no metric reports.
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xml_html_language_url()));
  RetryForHistogramUntilCountReached(&histograms,
                                     kXmlHtmlLanguageHistorgramName, 0);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest, XmlLangWildcard) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/true,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"*",
                  /*html_lang_value=*/"en-US"},
                 {xml_html_language_url()});

  NavigateAndVerifyXmlHtmlHistogramsMetric(
      xml_html_language_url(),
      blink::AcceptLanguageAndXmlHtmlLangUsage::kXmlLangWildcard, 1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       XmlLangMatchesAnyNonPrimayAcceptLanguage) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/true,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"zh-CN",
                  /*html_lang_value=*/"zh-CN"},
                 {xml_html_language_url()});

  SetPerfsAcceptLanguage({"en-US", "en-GB", "zh", "zh-CN"});
  NavigateAndVerifyXmlHtmlHistogramsMetric(
      xml_html_language_url(),
      blink::AcceptLanguageAndXmlHtmlLangUsage::
          kXmlLangMatchesAnyNonPrimayAcceptLanguage,
      1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       XmlLangMatchesPrimaryAcceptLanguage) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/true,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"en-US",
                  /*html_lang_value=*/"en-US"},
                 {xml_html_language_url()});

  SetPerfsAcceptLanguage({"en-US", "en-GB", "zh", "zh-CN"});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xml_html_language_url()));

  RetryForHistogramUntilCountReached(&histograms,
                                     kXmlHtmlLanguageHistorgramName, 1);
  // xml lang matches primary accept language
  histograms.ExpectBucketCount(kXmlHtmlLanguageHistorgramName,
                               blink::AcceptLanguageAndXmlHtmlLangUsage::
                                   kXmlLangMatchesPrimaryAcceptLanguage,
                               1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       XmlLangMatchesPrimaryAcceptLanguageWithoutLocale) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/true,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"en-GB",
                  /*html_lang_value=*/"en-GB"},
                 {xml_html_language_url()});

  SetPerfsAcceptLanguage({"en-US", "en-GB", "zh", "zh-CN"});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xml_html_language_url()));

  RetryForHistogramUntilCountReached(&histograms,
                                     kXmlHtmlLanguageHistorgramName, 1);

  // xml lang matches primary accept language
  histograms.ExpectBucketCount(kXmlHtmlLanguageHistorgramName,
                               blink::AcceptLanguageAndXmlHtmlLangUsage::
                                   kXmlLangMatchesPrimaryAcceptLanguage,
                               1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest, HtmlLangEmpty) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/false,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"",
                  /*html_lang_value=*/""},
                 {xml_html_language_url()});

  NavigateAndVerifyXmlHtmlHistogramsMetric(
      xml_html_language_url(),
      blink::AcceptLanguageAndXmlHtmlLangUsage::kHtmlLangEmpty, 1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest, HtmlLangWildCard) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/false,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"",
                  /*html_lang_value=*/"*"},
                 {xml_html_language_url()});

  NavigateAndVerifyXmlHtmlHistogramsMetric(
      xml_html_language_url(),
      blink::AcceptLanguageAndXmlHtmlLangUsage::kHtmlLangWildcard, 1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       HtmlLangMatchesAnyNonPrimayAcceptLanguage) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/false,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"",
                  /*html_lang_value=*/"zh-CN"},
                 {xml_html_language_url()});

  SetPerfsAcceptLanguage({"en-US", "en-GB", "zh", "zh-CN"});
  NavigateAndVerifyXmlHtmlHistogramsMetric(
      xml_html_language_url(),
      blink::AcceptLanguageAndXmlHtmlLangUsage::
          kHtmlLangMatchesAnyNonPrimayAcceptLanguage,
      1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       HtmlLangMatchesPrimaryAcceptLanguage) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/false,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"",
                  /*html_lang_value=*/"en-US"},
                 {xml_html_language_url()});

  SetPerfsAcceptLanguage({"en-US", "en-GB", "zh", "zh-CN"});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xml_html_language_url()));

  RetryForHistogramUntilCountReached(&histograms,
                                     kXmlHtmlLanguageHistorgramName, 1);

  // html lang matches primary accept language
  histograms.ExpectBucketCount(kXmlHtmlLanguageHistorgramName,
                               blink::AcceptLanguageAndXmlHtmlLangUsage::
                                   kHtmlLangMatchesPrimaryAcceptLanguage,
                               1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       HtmlLangMatchesPrimaryAcceptLanguageWithoutLocale) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/false,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"",
                  /*html_lang_value=*/"en-GB"},
                 {xml_html_language_url()});

  SetPerfsAcceptLanguage({"en-US", "en-GB", "zh", "zh-CN"});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xml_html_language_url()));

  RetryForHistogramUntilCountReached(&histograms,
                                     kXmlHtmlLanguageHistorgramName, 1);

  // html lang matches primary accept language
  histograms.ExpectBucketCount(kXmlHtmlLanguageHistorgramName,
                               blink::AcceptLanguageAndXmlHtmlLangUsage::
                                   kHtmlLangMatchesPrimaryAcceptLanguage,
                               1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       HtmlLangMatchesMatchNonPrimaryAcceptLanguageZh) {
  SetTestOptions({/*has_content_language_in_parent=*/false,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/false,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"",
                  /*html_lang_value=*/"zh-HK"},
                 {xml_html_language_url()});

  SetPerfsAcceptLanguage({"zh-CN", "zh-TW", "zh-HK"});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xml_html_language_url()));

  RetryForHistogramUntilCountReached(&histograms,
                                     kXmlHtmlLanguageHistorgramName, 1);

  // zh family, html lang matches non-primary accept language instead of primary
  histograms.ExpectBucketCount(kXmlHtmlLanguageHistorgramName,
                               blink::AcceptLanguageAndXmlHtmlLangUsage::
                                   kHtmlLangMatchesAnyNonPrimayAcceptLanguage,
                               1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       XmlLanguageContentLanguageIntegrationTest) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"zh",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/true,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"zh-CN",
                  /*html_lang_value=*/"zh-CN"},
                 {xml_html_language_url()});

  SetPerfsAcceptLanguage({"en-US", "en-GB", "zh", "zh-CN", "en"});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xml_html_language_url()));

  RetryForHistogramUntilCountReached(&histograms,
                                     kXmlHtmlLanguageHistorgramName, 1);

  // Accept-Language vs xml:lang: match any
  histograms.ExpectBucketCount(kXmlHtmlLanguageHistorgramName,
                               blink::AcceptLanguageAndXmlHtmlLangUsage::
                                   kXmlLangMatchesAnyNonPrimayAcceptLanguage,
                               1);

  // Accept-Language vs Content-Language: match any
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageMatchesAnyAcceptLanguage,
                               1);
}

IN_PROC_BROWSER_TEST_F(RecordLanguagesMetricsBrowserTest,
                       HtmlLanguageContentLanguageIntegrationTest) {
  SetTestOptions({/*has_content_language_in_parent=*/true,
                  /*has_content_language_in_child=*/false,
                  /*parent_content_language_value=*/"zh",
                  /*child_content_language_value=*/"",
                  /*has_xml_lang=*/false,
                  /*has_html_lang=*/true,
                  /*xml_lang_value=*/"",
                  /*html_lang_value=*/"en-US"},
                 {xml_html_language_url()});

  SetPerfsAcceptLanguage({"en-US", "en-GB", "zh", "zh-CN", "en"});

  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xml_html_language_url()));

  RetryForHistogramUntilCountReached(&histograms,
                                     kXmlHtmlLanguageHistorgramName, 1);

  // Accept-Language vs html lang: match primary
  histograms.ExpectBucketCount(kXmlHtmlLanguageHistorgramName,
                               blink::AcceptLanguageAndXmlHtmlLangUsage::
                                   kHtmlLangMatchesPrimaryAcceptLanguage,
                               1);

  // Accept-Language vs Content-Language: match any
  histograms.ExpectBucketCount(kLanguageHistorgramName,
                               blink::AcceptLanguageAndContentLanguageUsage::
                                   kContentLanguageMatchesAnyAcceptLanguage,
                               1);
}
