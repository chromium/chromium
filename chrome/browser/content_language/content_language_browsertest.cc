// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
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
  RecordLanguagesMetricsBrowserTest() = default;

  static constexpr const char kOriginUrl[] = "https://127.0.0.1:44444";
  static constexpr const char kLanguageHistorgramName[] =
      "LanguageUsage.AcceptLanguageAndContentLanguageUsage";

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
      int expect_metric_count) {
    base::HistogramTester histograms;

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    histograms.ExpectBucketCount(kLanguageHistorgramName, expect_metric_type,
                                 expect_metric_count);
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

    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    if (params->url_request.url.path() == "/simple.html") {
      base::StrAppend(&headers, {BuildIframeResponseHeader()});
    } else {
      base::StrAppend(&headers, {BuildResponseHeader()});
    }

    URLLoaderInterceptor::WriteResponse(path, params->client.get(), &headers,
                                        absl::nullopt,
                                        /*url=*/params->url_request.url);
    return true;
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
};

constexpr const char RecordLanguagesMetricsBrowserTest::kOriginUrl[];
constexpr const char
    RecordLanguagesMetricsBrowserTest::kLanguageHistorgramName[];

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
                  /*parent_content_language_value=*/"en",
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
