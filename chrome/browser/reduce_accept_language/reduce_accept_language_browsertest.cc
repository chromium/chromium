// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/origin.h"

namespace {

using ::content::URLLoaderInterceptor;

struct ReduceAcceptLanguageTestOptions {
  std::optional<std::string> content_language_in_parent;
  std::optional<std::string> avail_language_in_parent;
  std::optional<std::string> vary_in_parent;
  std::optional<std::string> content_language_in_child;
  std::optional<std::string> avail_language_in_child;
  std::optional<std::string> vary_in_child;
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

const size_t kLargeLanguagesCount = base::SplitString(kLargeLanguages,
                                                      ",",
                                                      base::TRIM_WHITESPACE,
                                                      base::SPLIT_WANT_ALL)
                                        .size();

static constexpr const char kFirstPartyOriginUrl[] = "https://127.0.0.1:44444";

}  // namespace

class ReduceAcceptLanguageBrowserTest : public InProcessBrowserTest {
 public:
  ReduceAcceptLanguageBrowserTest() = default;

  void SetUp() override {
    EnabledFeatures();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // We use a URLLoaderInterceptor, we also can use the EmbeddedTestServer.
    url_loader_interceptor_ = std::make_unique<URLLoaderInterceptor>(
        base::BindRepeating(&ReduceAcceptLanguageBrowserTest::InterceptRequest,
                            base::Unretained(this)));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    intercepted_load_urls_.clear();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetTestOptions(const ReduceAcceptLanguageTestOptions& test_options,
                      const std::set<GURL>& expected_request_urls) {
    test_options_ = test_options;
    expected_request_urls_ = expected_request_urls;
  }

  GURL SameOriginRequestUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/same_origin_request.html"}));
  }

  GURL SameOriginIframeUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/same_origin_iframe.html"}));
  }

  GURL SimpleRequestUrl() const {
    return GURL(base::StrCat({kFirstPartyOriginUrl, "/subframe_simple.html"}));
  }

  GURL LastRequestUrl() const {
    return url_loader_interceptor_->GetLastRequestURL();
  }

  // Navigate `url` and wait for NavigateToURL to complete, including all
  // subframes and verify whether the Accept-Language header value of last
  // request in `expected_request_urls_` is `expect_accept_language`.
  void NavigateAndVerifyAcceptLanguageOfLastRequest(
      const GURL& url,
      const std::optional<std::string>& expect_accept_language) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    const std::optional<std::string>& accept_language_header_value =
        GetLastAcceptLanguageHeaderValue();
    if (!expect_accept_language) {
      EXPECT_FALSE(accept_language_header_value.has_value());
    } else {
      EXPECT_TRUE(accept_language_header_value.has_value());
      EXPECT_EQ(expect_accept_language.value(),
                accept_language_header_value.value());
    }
  }

  void VerifyNavigatorLanguages(
      const std::vector<std::string>& expect_languages) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    base::ListValue languages_list =
        content::EvalJs(web_contents, "navigator.languages")
            .TakeValue()
            .TakeList();
    std::vector<std::string> actual_languages;
    for (const auto& result : languages_list) {
      actual_languages.push_back(result.GetString());
    }

    EXPECT_EQ(expect_languages, actual_languages);
  }

  void SetPrefsAcceptLanguage(
      const std::vector<std::string>& accept_languages) {
    auto language_prefs = std::make_unique<language::LanguagePrefs>(
        browser()->GetProfile()->GetPrefs());
    language_prefs->SetUserSelectedLanguagesList(accept_languages);
  }

 protected:
  // Return the feature list for the tests.
  virtual void EnabledFeatures() = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::set<GURL> intercepted_load_urls_;

 private:
  // Returns the value of the Accept-Language request header from the last sent
  // request, or nullopt if the header could not be read.
  const std::optional<std::string>& GetLastAcceptLanguageHeaderValue() {
    last_accept_language_value_ =
        url_loader_interceptor_->GetLastRequestHeaders().GetHeader(
            "accept-language");
    return last_accept_language_value_;
  }

  // URLLoaderInterceptor callback
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    if (expected_request_urls_.find(params->url_request.url) ==
        expected_request_urls_.end())
      return false;

    intercepted_load_urls_.insert(params->url_request.url);

    std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";

    static constexpr auto kSubresourcePaths =
        base::MakeFixedFlatSet<std::string_view>({
            "/subframe_simple.html",
        });
    const std::string path = params->url_request.url.GetPath();
    if (kSubresourcePaths.contains(path)) {
      base::StrAppend(&headers, {BuildSubresourceResponseHeader()});
    } else {
      base::StrAppend(&headers, {BuildResponseHeader()});
    }

    std::string resource_path = "chrome/test/data/reduce_accept_language";
    resource_path.append(
        static_cast<std::string>(params->url_request.url.path()));

    URLLoaderInterceptor::WriteResponse(resource_path, params->client.get(),
                                        &headers, std::nullopt,
                                        /*url=*/params->url_request.url);
    return true;
  }

  std::string BuildResponseHeader() {
    std::string headers;
    if (test_options_.content_language_in_parent) {
      base::StrAppend(
          &headers, {"Content-Language: ",
                     test_options_.content_language_in_parent.value(), "\r\n"});
    }
    if (test_options_.avail_language_in_parent) {
      base::StrAppend(&headers,
                      {"Avail-Language: ",
                       test_options_.avail_language_in_parent.value(), "\r\n"});
    }
    if (test_options_.vary_in_parent) {
      base::StrAppend(&headers,
                      {"Vary: ", test_options_.vary_in_parent.value(), "\n"});
    }
    return headers;
  }

  std::string BuildSubresourceResponseHeader() {
    std::string headers;
    if (test_options_.content_language_in_child) {
      base::StrAppend(
          &headers, {"Content-Language: ",
                     test_options_.content_language_in_child.value(), "\r\n"});
    }
    if (test_options_.avail_language_in_child) {
      base::StrAppend(&headers,
                      {"Avail-Language: ",
                       test_options_.avail_language_in_child.value(), "\r\n"});
    }
    if (test_options_.vary_in_child) {
      base::StrAppend(&headers,
                      {"Vary: ", test_options_.vary_in_child.value(), "\r\n"});
    }
    return headers;
  }

  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  std::set<GURL> expected_request_urls_;
  ReduceAcceptLanguageTestOptions test_options_;
  std::optional<std::string> last_accept_language_value_;
};

// Browser tests verify reduce the total number of Accept-Language.
class ReduceAcceptLanguageCountBrowserTest
    : public ReduceAcceptLanguageBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  void EnabledFeatures() override {
    // If explicitly set custom count set 5 as max.
    if (GetParam()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{language::kReduceAcceptLanguageCount,
                                 {{language::kMaxAcceptLanguage.name, "5"}}}},
          /*disabled_features=*/{network::features::kReduceAcceptLanguage,
                                 network::features::kReduceAcceptLanguageHTTP});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{language::kReduceAcceptLanguageCount},
          /*disabled_features=*/{network::features::kReduceAcceptLanguage,
                                 network::features::kReduceAcceptLanguageHTTP});
    }
  }

  // Samples recorded for kLargeLanguages: 1 during initial profile setup,
  // twice more when SetPrefsAcceptLanguage() syncs the preference to the
  // renderer and network services, and 1 per WebUI omnibox popup or WebUI
  // toolbar WebContents.
  // The number of popups depends on platform defaults and AIM eligibility.
  size_t ExpectedAcceptLanguageCountSamples() const {
    return 3 + std::ranges::count_if(
                   content::GetAllWebContents(),
                   [](content::WebContents* contents) {
                     const std::string_view host =
                         contents->GetVisibleURL().host();
                     return host == chrome::kChromeUIOmniboxPopupHost ||
                            host == chrome::kChromeUIWebUIToolbarHost;
                   });
  }
};

INSTANTIATE_TEST_SUITE_P(FeatureFlag,
                         ReduceAcceptLanguageCountBrowserTest,
                         testing::Values(true, false));

IN_PROC_BROWSER_TEST_P(ReduceAcceptLanguageCountBrowserTest, RegularRequest) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "en",
                  .avail_language_in_parent = "en, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});
  SetPrefsAcceptLanguage(base::SplitString(
      kLargeLanguages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));

  // Expect no Accept-Language header added because browser_tests can only check
  // headers in navigation layer, browser_tests can't see headers added by
  // network stack.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               std::nullopt);
  if (GetParam()) {
    VerifyNavigatorLanguages({"zh", "zh-CN", "en-US", "en", "af"});
  } else {
    VerifyNavigatorLanguages(
        {"zh", "zh-CN", "en-US", "en", "af", "sq", "am", "ar", "an", "hy"});
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount("LanguageUsage.AcceptLanguage.Count2",
                               kLargeLanguagesCount,
                               ExpectedAcceptLanguageCountSamples());
}

IN_PROC_BROWSER_TEST_P(ReduceAcceptLanguageCountBrowserTest, Iframe) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage(base::SplitString(
      kLargeLanguages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));

  // Expect no Accept-Language header added because browser_tests can only check
  // headers in navigation layer, browser_tests can't see headers added by
  // network stack.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(),
                                               std::nullopt);
  if (GetParam()) {
    VerifyNavigatorLanguages({"zh", "zh-CN", "en-US", "en", "af"});
  } else {
    VerifyNavigatorLanguages(
        {"zh", "zh-CN", "en-US", "en", "af", "sq", "am", "ar", "an", "hy"});
  }
  EXPECT_EQ(LastRequestUrl().GetPath(), "/subframe_simple.html");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount("LanguageUsage.AcceptLanguage.Count2",
                               kLargeLanguagesCount,
                               ExpectedAcceptLanguageCountSamples());
}
