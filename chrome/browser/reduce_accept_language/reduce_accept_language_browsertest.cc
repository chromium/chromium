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
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
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
static constexpr char kThirdPartyOriginUrl[] = "https://my-site.com:44444";

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
            "/subframe_iframe_basic.html",
            "/subframe_iframe_3p.html",
            "/subframe_simple.html",
            "/subframe_simple_3p.html",
            "/subresource_redirect_style.css",
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

// Browser tests that consider ReduceAcceptLanguage feature disabled.
class DisableFeatureReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
  void EnabledFeatures() override {
    scoped_feature_list_.InitWithFeatures(
        {}, {network::features::kReduceAcceptLanguage,
             network::features::kReduceAcceptLanguageHTTP});
  }
};

IN_PROC_BROWSER_TEST_F(DisableFeatureReduceAcceptLanguageBrowserTest,
                       NoAcceptLanguageHeader) {
  SetTestOptions({.content_language_in_parent = "en",
                  .avail_language_in_parent = "en, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});
  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Expect no Accept-Language header added because browser_tests can only check
  // headers in navigation layer, browser_tests can't see headers added by
  // network stack.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               std::nullopt);
  VerifyNavigatorLanguages({"zh", "en-US"});
}

IN_PROC_BROWSER_TEST_F(DisableFeatureReduceAcceptLanguageBrowserTest,
                       IframeNoAcceptLanguageHeader) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Expect no Accept-Language header added because browser_tests can only check
  // headers in navigation layer, browser_tests can't see headers added by
  // network stack.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(),
                                               std::nullopt);
  EXPECT_EQ(LastRequestUrl().GetPath(), "/subframe_simple.html");
}

// Tests same origin requests with the ReduceAcceptLanguage feature enabled.
class SameOriginReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  void EnabledFeatures() override {
    // True: Enable the general feature for Reduce Accept-Language.
    // False: Only enable reduction for HTTP header.
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {network::features::kReduceAcceptLanguage}, {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {network::features::kReduceAcceptLanguageHTTP},
          {network::features::kReduceAcceptLanguage});
    }
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SameOriginReduceAcceptLanguageBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       LargeLanguageListAndScriptDisable) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage(base::SplitString(
      kLargeLanguages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
  // Expect accept-language set as the first user's accept-language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  // Disable script for first party origin.
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingCustomScope(
          ContentSettingsPattern::FromURL(GURL(kFirstPartyOriginUrl)),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::JAVASCRIPT,
          CONTENT_SETTING_BLOCK);

  // Even Script disabled, it still expects reduced accept-language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       EmptyUserAcceptLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({});
  // Expect no reduced Accept-Language header set on navigation request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               std::nullopt);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguagePrefValueIsEmpty", true, 1);

  // No prefs read and write operations.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 0);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       NoAvailLanguageHeader) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = std::nullopt,
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en"});
  // Expect accept-language set as the first user's accept-language
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
  // Persist won't happen.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  // Verify that navigator.languages only returns an array of length 1 if
  // ReduceAcceptLanguage is enabled. For the HTTP-only feature, it should
  // be no change and return the full list of languages.
  if (GetParam()) {
    VerifyNavigatorLanguages({"zh"});
  } else {
    VerifyNavigatorLanguages({"zh", "en"});
  }
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       NoContentLanguageHeader) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = std::nullopt,
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en"});
  // Expect accept-language set as the first user's accept-language
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
  // Persist won't happen.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       EmptyAvailLanguageAcceptLanguages) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en"});
  // Expect accept-language set as the first user's accept-language
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // One request, one prefs fetch when initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
  // Persist won't happen.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       AvailLanguageAcceptLanguagesWhiteSpace) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "   ",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en"});
  // Expect accept-language set as the first user's accept-language
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // One request, one Prefs fetch request when initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
  // Persist won't happen.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       SiteLanguageMatchPrimaryLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"es", "en-US"});

  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "es");

  // The second request should send out with the same preferred language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "es");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // For above two same_origin_request_url requests, both only have one Prefs
  // fetch when initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  // Expect no perf storage updates.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       SiteLanguageDontMatchAnyPreferredLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "ja"});

  // Expect accept-language set as the first user's accept-language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  // The second request should send out with the same first preferred language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // For above two same_origin_request_url requests: each has one Prefs fetch
  // request when initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  // Expect no perf storage updates.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeReduceAcceptLanguage) {
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

  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two requests, each has one Prefs fetch request when initially adding
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);

  EXPECT_EQ(LastRequestUrl().GetPath(), "/subframe_simple.html");

  // Disable script for first party origin.
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingCustomScope(
          ContentSettingsPattern::FromURL(GURL(kFirstPartyOriginUrl)),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::JAVASCRIPT,
          CONTENT_SETTING_BLOCK);

  // Even Script disabled, it still expects reduced accept-language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "zh");
  EXPECT_EQ(LastRequestUrl().GetPath(), "/subframe_simple.html");
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeNoContentLanguageInChild) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = std::nullopt,
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two different URL requests, each has one Prefs fetch request when
  // initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  EXPECT_EQ(LastRequestUrl().GetPath(), "/subframe_simple.html");
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeNoAvailLanguageAcceptLanguageInChild) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = std::nullopt,
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two different URL requests, each has one Prefs fetch request when
  // initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  EXPECT_EQ(LastRequestUrl().GetPath(), "/subframe_simple.html");
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeSameContentLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two different URL requests, each has one Prefs fetch request when
  // initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  EXPECT_EQ(LastRequestUrl().GetPath(), "/subframe_simple.html");
}

IN_PROC_BROWSER_TEST_P(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeDifferentContentLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .avail_language_in_child = "zh",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two different URL requests, each has one Prefs fetch request when
  // initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  EXPECT_EQ(LastRequestUrl().GetPath(), "/subframe_simple.html");
}

class ThirdPartyReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  static constexpr char kOtherSiteOriginUrl[] = "https://other-site.com:44445";
  static constexpr char kOtherSiteBOriginUrl[] =
      "https://other-site-b.com:44445";

  GURL CrossOriginIframeUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/cross_origin_iframe.html"}));
  }

  GURL CrossOriginIframeWithSubresourceUrl() const {
    return GURL(base::StrCat(
        {kFirstPartyOriginUrl, "/cross_origin_iframe_with_subrequests.html"}));
  }

  GURL SimpleThirdPartyRequestUrl() const {
    return GURL(
        base::StrCat({kThirdPartyOriginUrl, "/subframe_simple_3p.html"}));
  }

  GURL IframeThirdPartyRequestUrl() const {
    return GURL(
        base::StrCat({kThirdPartyOriginUrl, "/subframe_iframe_3p.html"}));
  }

  GURL OtherSiteCssRequestUrl() const {
    return GURL(
        base::StrCat({kOtherSiteOriginUrl, "/subresource_redirect_style.css"}));
  }

  GURL OtherSiteBasicRequestUrl() const {
    return GURL(
        base::StrCat({kOtherSiteBOriginUrl, "/subframe_iframe_basic.html"}));
  }

 protected:
  void EnabledFeatures() override {
    // True: Enable the general feature for Reduce Accept-Language.
    // False: Only enable reduction for HTTP header.
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {network::features::kReduceAcceptLanguage}, {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {network::features::kReduceAcceptLanguageHTTP},
          {network::features::kReduceAcceptLanguage});
    }
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ThirdPartyReduceAcceptLanguageBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(ThirdPartyReduceAcceptLanguageBrowserTest,
                       IframeDifferentContentLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .avail_language_in_child = "zh",
                  .vary_in_child = "accept-language"},
                 {CrossOriginIframeUrl(), SimpleThirdPartyRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Third party iframe subrequest expects the language of the main frame.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CrossOriginIframeUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two different URL requests, each has one Prefs fetch request when
  // initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  EXPECT_EQ(LastRequestUrl().GetPath(), "/subframe_simple_3p.html");
}

IN_PROC_BROWSER_TEST_P(ThirdPartyReduceAcceptLanguageBrowserTest,
                       ThirdPartyIframeWithSubresourceRequests) {
  base::HistogramTester histograms;

  SetTestOptions(
      {.content_language_in_parent = "es",
       .avail_language_in_parent = "es, en-US",
       .vary_in_parent = "accept-language",
       .content_language_in_child = "zh",
       .avail_language_in_child = "zh",
       .vary_in_child = "accept-language"},
      {CrossOriginIframeWithSubresourceUrl(), IframeThirdPartyRequestUrl(),
       OtherSiteCssRequestUrl(), OtherSiteBasicRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Third party iframe subrequest expects the language of the main frame.
  NavigateAndVerifyAcceptLanguageOfLastRequest(
      CrossOriginIframeWithSubresourceUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Fetch reduce accept-language when visiting the following three URLs, for
  // css request, it won't pass to navigation layer:
  // * cross_origin_iframe_with_subrequests_url(1): one fetch for initially
  // adding header.
  // * iframe_3p_request_url(1): one fetch for initially adding header.
  // * other_site_b_basic_request_url(1): one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 3);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  // All subresources should have been loaded,
  EXPECT_THAT(intercepted_load_urls_,
              testing::IsSupersetOf({IframeThirdPartyRequestUrl(),
                                     OtherSiteCssRequestUrl(),
                                     OtherSiteBasicRequestUrl()}));
}

// Browser tests verify reduce the total number of Accept-Language.
class ReduceAcceptLanguageCountBrowserTest
    : public ReduceAcceptLanguageBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  void EnabledFeatures() override {
    // If explicitly set custom count set 5 as max.
    if (GetParam()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{network::features::kReduceAcceptLanguageCount,
                                 {{network::features::kMaxAcceptLanguage.name,
                                   "5"}}}},
          /*disabled_features=*/{network::features::kReduceAcceptLanguage,
                                 network::features::kReduceAcceptLanguageHTTP});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{network::features::kReduceAcceptLanguageCount},
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
