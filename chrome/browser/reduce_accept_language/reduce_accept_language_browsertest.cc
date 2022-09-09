// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
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
using ::net::test_server::EmbeddedTestServer;

struct ReduceAcceptLanguageTestOptions {
  absl::optional<std::string> content_language_in_parent = absl::nullopt;
  absl::optional<std::string> variants_in_parent = absl::nullopt;
  absl::optional<std::string> vary_in_parent = absl::nullopt;
  absl::optional<std::string> content_language_in_child = absl::nullopt;
  absl::optional<std::string> variants_in_child = absl::nullopt;
  absl::optional<std::string> vary_in_child = absl::nullopt;
  bool is_fenced_frame = false;
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

class ReduceAcceptLanguageBrowserTest : public InProcessBrowserTest {
 public:
  ReduceAcceptLanguageBrowserTest() = default;

  static constexpr const char kFirstPartyOriginUrl[] =
      "https://127.0.0.1:44444";

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
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetTestOptions(const ReduceAcceptLanguageTestOptions& test_options,
                      const std::set<GURL>& expected_request_urls) {
    test_options_ = test_options;
    expected_request_urls_ = expected_request_urls;
  }

  GURL CreateServiceWorkerRequestUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/create_service_worker.html"}));
  }

  GURL NavigationPreloadWorkerRequestUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/navigation_preload_worker.js"}));
  }

  GURL SameOriginRequestUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/same_origin_request.html"}));
  }

  GURL sameOriginIframeUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/same_origin_iframe.html"}));
  }

  GURL SameOriginImgUrl() const {
    return GURL(base::StrCat({kFirstPartyOriginUrl, "/same_origin_img.html"}));
  }

  GURL SimpleImgUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/subresource_simple.jpg"}));
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
      const absl::optional<std::string>& expect_accept_language) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    const absl::optional<std::string>& accept_language_header_value =
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
    base::Value languages_list =
        content::EvalJs(web_contents, "navigator.languages").ExtractList();
    std::vector<std::string> actual_languages;
    for (auto& result : languages_list.GetListDeprecated())
      actual_languages.push_back(result.GetString());

    EXPECT_EQ(expect_languages, actual_languages);
  }

  void SetPrefsAcceptLanguage(
      const std::vector<std::string>& accept_languages) {
    auto language_prefs = std::make_unique<language::LanguagePrefs>(
        browser()->profile()->GetPrefs());
    language_prefs->SetUserSelectedLanguagesList(accept_languages);
  }

 protected:
  // Return the feature list for the tests.
  virtual void EnabledFeatures() = 0;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Returns whether a given |header| has been received for the last request.
  bool HasReceivedHeader(const std::string& header) const {
    return url_loader_interceptor_->GetLastRequestHeaders().HasHeader(header);
  }

 private:
  // Returns the value of the Accept-Language request header from the last sent
  // request, or nullopt if the header could not be read.
  const absl::optional<std::string>& GetLastAcceptLanguageHeaderValue() {
    std::string accept_language_header_value;
    if (url_loader_interceptor_->GetLastRequestHeaders().GetHeader(
            "accept-language", &accept_language_header_value)) {
      last_accept_language_value_ = accept_language_header_value;
    } else {
      last_accept_language_value_ = absl::nullopt;
    }
    return last_accept_language_value_;
  }

  // URLLoaderInterceptor callback
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    if (expected_request_urls_.find(params->url_request.url) ==
        expected_request_urls_.end())
      return false;

    std::string headers = "HTTP/1.1 200 OK\r\n";
    if (params->url_request.url == NavigationPreloadWorkerRequestUrl()) {
      base::StrAppend(&headers, {"Content-Type: text/javascript\r\n"});
    } else {
      base::StrAppend(&headers, {"Content-Type: text/html\r\n"});
    }

    if (test_options_.is_fenced_frame) {
      base::StrAppend(&headers, {"Supports-Loading-Mode: fenced-frame\r\n"});
    }
    static constexpr auto kSubresourcePaths =
        base::MakeFixedFlatSet<base::StringPiece>({
            "/subframe_iframe_basic.html",
            "/subframe_iframe_3p.html",
            "/subframe_redirect.html",
            "/subframe_simple.html",
            "/subframe_simple_3p.html",
            "/subresource_simple.jpg",
            "/subresource_redirect_style.css",
        });
    const std::string path = params->url_request.url.path();
    if (base::Contains(kSubresourcePaths, path)) {
      base::StrAppend(&headers, {BuildSubresourceResponseHeader()});
    } else {
      base::StrAppend(&headers, {BuildResponseHeader()});
    }

    static constexpr auto kServiceWorkerPaths =
        base::MakeFixedFlatSet<base::StringPiece>({
            "/create_service_worker.html",
            "/navigation_preload_worker.js",
        });

    std::string resource_path;
    if (base::Contains(kServiceWorkerPaths, path)) {
      resource_path = "chrome/test/data/service_worker";
    } else {
      resource_path = "chrome/test/data/reduce_accept_language";
    }
    resource_path.append(
        static_cast<std::string>(params->url_request.url.path_piece()));

    URLLoaderInterceptor::WriteResponse(resource_path, params->client.get(),
                                        &headers, absl::nullopt,
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
    if (test_options_.variants_in_parent) {
      base::StrAppend(
          &headers,
          {"Variants: ", test_options_.variants_in_parent.value(), "\r\n"});
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
    if (test_options_.variants_in_child) {
      base::StrAppend(
          &headers,
          {"Variants: ", test_options_.variants_in_child.value(), "\r\n"});
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
  absl::optional<std::string> last_accept_language_value_;
};

// Browser tests that consider ReduceAcceptLanguage feature disabled.
class DisableFeatureReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
  void EnabledFeatures() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("", "ReduceAcceptLanguage");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(DisableFeatureReduceAcceptLanguageBrowserTest,
                       NoAcceptLanguageHeader) {
  SetTestOptions({.content_language_in_parent = "en",
                  .variants_in_parent = "accept-language=(en en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});
  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Expect no Accept-Language header added because browser_tests can only check
  // headers in navigation layer, browser_tests can't see headers added by
  // network stack.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               absl::nullopt);
  VerifyNavigatorLanguages({"zh", "en-us"});
}

IN_PROC_BROWSER_TEST_F(DisableFeatureReduceAcceptLanguageBrowserTest,
                       IframeNoAcceptLanguageHeader) {
  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .variants_in_child = "accept-language=(es en-US)",
                  .vary_in_child = "accept-language"},
                 {sameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Expect no Accept-Language header added because browser_tests can only check
  // headers in navigation layer, browser_tests can't see headers added by
  // network stack.
  NavigateAndVerifyAcceptLanguageOfLastRequest(sameOriginIframeUrl(),
                                               absl::nullopt);
  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}

// Tests same origin requests with the ReduceAcceptLanguage feature enabled.
class SameOriginReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 protected:
  void EnabledFeatures() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("ReduceAcceptLanguage", "");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       LargeLanguageListAndScriptDisable) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage(base::SplitString(
      kLargeLanguages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
  // Expect accept-language set as the negotiation language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "en-US");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // same_origin_request_url request has two fetch Prefs requests: one fetch
  // for initially adding header and another one for restart fetch.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // One store for same_origin_request_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  // Disable script for first party origin.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingCustomScope(
          ContentSettingsPattern::FromURL(GURL(kFirstPartyOriginUrl)),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::JAVASCRIPT,
          CONTENT_SETTING_BLOCK);

  // Even Script disabled, it still expects reduced accept-language. The second
  // navigation should use the language after negotiation which is en-us.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "en-US");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       NoVariantsHeader) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = absl::nullopt,
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en"});
  // Expect accept-language set as the first user's accept-language
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
  // Persist won't happen.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  // Verify navigator.languages only returns an array length 1 if
  // ReduceAcceptLanguage enabled.
  VerifyNavigatorLanguages({"zh"});
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       NoContentLanguageHeader) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = absl::nullopt,
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en"});
  // Expect accept-language set as the first user's accept-language
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure metrics report correctly.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kVariantsAndContentLanguageHeaderPresent=*/2, 0);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
  // Persist won't happen.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       EmptyVariantsAcceptLanguages) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=()",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en"});
  // Expect accept-language set as the first user's accept-language
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // One request, one prefs fetch when initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
  // Persist won't happen.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       VariantsAcceptLanguagesWhiteSpace) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(   )",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en"});
  // Expect accept-language set as the first user's accept-language
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure no restart happens.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  // One request, one Prefs fetch request when initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
  // Persist won't happen.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       SiteLanguageMatchNonPrimaryLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Expect accept-language set as negotiated language: en-us.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure only restart once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // One request same_origin_request_url: one Prefs fetch request when initial
  // add header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // One store for same_origin_request_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  base::HistogramTester histograms_after;
  SetTestOptions({.content_language_in_parent = "en-us",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  // The second request should send out with the first matched negotiation
  // language en-us instead of ja.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "en-us");
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure no restart happen.
  histograms_after.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  // One request same_origin_request_url: one fetch for initially adding header
  // and no restart fetch.
  histograms_after.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
  // One store for same_origin_request_url main frame.
  histograms_after.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);
}

// Verify no endless resend requests for the service worker navigation preload
// requests.
IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       ServiceWorkerNavigationPreload) {
  SetTestOptions(
      {.content_language_in_parent = "es",
       .variants_in_parent = "accept-language=(es en-US)",
       .vary_in_parent = "accept-language"},
      {CreateServiceWorkerRequestUrl(), NavigationPreloadWorkerRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  base::HistogramTester histograms;
  // Expect accept-language set as negotiated language: en-us.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CreateServiceWorkerRequestUrl(),
                                               "en-us");
  // Register a service worker that uses navigation preload.
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('/navigation_preload_worker.js', '/');"));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two Prefs fetch requests: one for initially adding header and another
  // one for the restart request adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // One store for create_service_worker_request_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  // Verify "Service-Worker-Navigation-Preload" is present and no future resend
  // requests when site responses with expected content-language 'en-us'.
  base::HistogramTester histograms2;
  SetTestOptions(
      {.content_language_in_parent = "en-us",
       .variants_in_parent = "accept-language=(es en-US)",
       .vary_in_parent = "accept-language"},
      {CreateServiceWorkerRequestUrl(), NavigationPreloadWorkerRequestUrl()});

  NavigateAndVerifyAcceptLanguageOfLastRequest(CreateServiceWorkerRequestUrl(),
                                               "en-us");
  EXPECT_TRUE(HasReceivedHeader("Service-Worker-Navigation-Preload"));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // One Prefs fetch request when initially adding header. No restart.
  histograms2.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
  histograms2.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kServiceWorkerPreloadRequest=*/2, 1);
  // Ensure no restart happen.
  histograms2.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  histograms2.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  // Verify "Service-Worker-Navigation-Preload" is present and no future resend
  // requests even when site made mistake responding with unexpected
  // content-language 'es'.
  base::HistogramTester histograms3;
  SetTestOptions(
      {.content_language_in_parent = "es",
       .variants_in_parent = "accept-language=(es en-US)",
       .vary_in_parent = "accept-language"},
      {CreateServiceWorkerRequestUrl(), NavigationPreloadWorkerRequestUrl()});

  NavigateAndVerifyAcceptLanguageOfLastRequest(CreateServiceWorkerRequestUrl(),
                                               "en-us");
  EXPECT_TRUE(HasReceivedHeader("Service-Worker-Navigation-Preload"));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // One Prefs fetch request when initially adding header.
  histograms3.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
  histograms3.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kServiceWorkerPreloadRequest=*/2, 1);
  // Ensure no restart happen.
  histograms3.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  histograms3.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       SiteLanguageMatchPrimaryLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"es", "en-us"});

  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "es");
  // Ensure no restart happen.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);

  // The second request should send out with the same preferred language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "es");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // For above two same_origin_request_url requests, both only have one Prefs
  // fetch when initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // Expect no perf storage updates.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       SiteLanguageMatchMultipleLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US ja)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us", "ja"});

  // Expect accept-language set as negotiated language: en-us.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure only restart once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // One request same_origin_request_url: one fetch for initially adding header
  // and another one for restart fetch.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // One store for same_origin_request_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  base::HistogramTester histograms_after;
  SetTestOptions({.content_language_in_parent = "en-us",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  // The second request should send out with the first matched negotiation
  // language en-us instead of ja.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "en-us");
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure no restart happen.
  histograms_after.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  // One request same_origin_request_url: one fetch for initially adding header
  // and no restart fetch.
  histograms_after.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
  // One store for same_origin_request_url main frame.
  histograms_after.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       SiteLanguageDontMatchAnyPreferredLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "ja"});

  // Expect accept-language set as the first user's accept-language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");
  // Ensure no restart happen.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);

  // The second request should send out with the same first preferred language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // For above two same_origin_request_url requests: each has one Prefs fetch
  // request when initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // Expect no perf storage updates.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       PersistedAcceptLanguageNotAvailable) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es ja en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "ja", "en-US"});
  // The first request should send out with the negotiated language which is ja.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "ja");

  SetPrefsAcceptLanguage({"zh", "en-US"});
  // The second request should send out with the new negotiated language en-us.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "en-US");
  SetPrefsAcceptLanguage({"zh"});
  // The third request should send out with the first accept-language since the
  // persisted language not available in latest user's accept-language list.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeReduceAcceptLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .variants_in_child = "accept-language=(es en-US)",
                  .vary_in_child = "accept-language"},
                 {sameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage(base::SplitString(
      kLargeLanguages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(sameOriginIframeUrl(), "en-US");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);

  // Total two different url requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");

  // Disable script for first party origin.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingCustomScope(
          ContentSettingsPattern::FromURL(GURL(kFirstPartyOriginUrl)),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::JAVASCRIPT,
          CONTENT_SETTING_BLOCK);

  // Even Script disabled, it still expects reduced accept-language. The second
  // navigation should use the language after negotiation which is en-us.
  NavigateAndVerifyAcceptLanguageOfLastRequest(sameOriginIframeUrl(), "en-US");
  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       ImgSubresourceReduceAcceptLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .variants_in_child = "accept-language=(es en-US)",
                  .vary_in_child = "accept-language"},
                 {SameOriginImgUrl(), SimpleImgUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Subresource img request expect to be the language after language
  // negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginImgUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests, only same_origin_img_url request has two
  // fetch Prefs requests: one fetch for initially adding header and another one
  // for the restart request adding header. For image request, it will directly
  // read the persisted from the navigation commit reduced accept language.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // One store for same_origin_img_url main frame.

  EXPECT_EQ(LastRequestUrl().path(), "/subresource_simple.jpg");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeNoContentLanguageInChild) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = absl::nullopt,
                  .variants_in_child = "accept-language=(es en-US)",
                  .vary_in_child = "accept-language"},
                 {sameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(sameOriginIframeUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);
  // One store for same_origin_iframe_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeNoVariantsAcceptLanguageInChild) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .variants_in_child = absl::nullopt,
                  .vary_in_child = "accept-language"},
                 {sameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(sameOriginIframeUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);
  // One store for same_origin_iframe_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeSameContentLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .variants_in_child = "accept-language=(es en-US)",
                  .vary_in_child = "accept-language"},
                 {sameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(sameOriginIframeUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);
  // One store for same_origin_iframe_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       IframeDifferentContentLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .variants_in_child = "accept-language=(zh)",
                  .vary_in_child = "accept-language"},
                 {sameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(sameOriginIframeUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);
  // One store for same_origin_iframe_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}

class ThirdPartyReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 public:
  static constexpr char kThirdPartyOriginUrl[] = "https://my-site.com:44444";
  static constexpr char kOtherSiteOriginUrl[] = "https://other-site.com:44445";
  static constexpr char kOtherSiteBOriginUrl[] =
      "https://other-site-b.com:44445";

  GURL CrossOriginIframeUrl() const {
    return GURL(
        base::StrCat({ReduceAcceptLanguageBrowserTest::kFirstPartyOriginUrl,
                      "/cross_origin_iframe.html"}));
  }

  GURL TopLevelWithIframeRedirectUrl() const {
    return GURL(
        base::StrCat({ReduceAcceptLanguageBrowserTest::kFirstPartyOriginUrl,
                      "/top_level_with_iframe_redirect.html"}));
  }

  GURL CrossOriginIframeWithSubresourceUrl() const {
    return GURL(
        base::StrCat({ReduceAcceptLanguageBrowserTest::kFirstPartyOriginUrl,
                      "/cross_origin_iframe_with_subrequests.html"}));
  }

  GURL SubframeThirdPartyRequestUrl() const {
    return GURL(
        base::StrCat({kThirdPartyOriginUrl, "/subframe_redirect_3p.html"}));
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
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("ReduceAcceptLanguage", "");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(ThirdPartyReduceAcceptLanguageBrowserTest,
                       IframeDifferentContentLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .variants_in_child = "accept-language=(zh)",
                  .vary_in_child = "accept-language"},
                 {CrossOriginIframeUrl(), SimpleThirdPartyRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Third party iframe subrequest expect to be the language of the main frame
  // after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CrossOriginIframeUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * cross_origin_iframe_url: one fetch for initially adding header and
  // another one for the restart request adding header.
  // * simple_3p_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);
  // One store for same_origin_iframe_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple_3p.html");
}

IN_PROC_BROWSER_TEST_F(ThirdPartyReduceAcceptLanguageBrowserTest,
                       ThirdPartyIframeWithSubresourceRequests) {
  base::HistogramTester histograms;

  SetTestOptions(
      {.content_language_in_parent = "es",
       .variants_in_parent = "accept-language=(es en-US)",
       .vary_in_parent = "accept-language",
       .content_language_in_child = "zh",
       .variants_in_child = "accept-language=(zh)",
       .vary_in_child = "accept-language"},
      {CrossOriginIframeWithSubresourceUrl(), IframeThirdPartyRequestUrl(),
       OtherSiteCssRequestUrl(), OtherSiteBasicRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Third party iframe subrequest expect to be the language of the main frame
  // after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(
      CrossOriginIframeWithSubresourceUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Fetch reduce accept-language when visiting the following three URLs, for
  // css request, it won't pass to navigation layer:
  // * cross_origin_iframe_with_subrequests_url(2):one fetch for initially
  // adding header and another one for the restart request adding header.
  // * iframe_3p_request_url(1): one fetch for initially adding header.
  // * other_site_b_basic_request_url(1): one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 4);
  // One store for cross_region_iframe_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_iframe_basic.html");
}

IN_PROC_BROWSER_TEST_F(ThirdPartyReduceAcceptLanguageBrowserTest,
                       ThirdPartyIframeWithSubresourceRedirectRequests) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .variants_in_child = "accept-language=(zh)",
                  .vary_in_child = "accept-language"},
                 {TopLevelWithIframeRedirectUrl(),
                  SubframeThirdPartyRequestUrl(), OtherSiteCssRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // It still expected an accept-language header has the reduced value even the
  // final url is a css style document,
  NavigateAndVerifyAcceptLanguageOfLastRequest(TopLevelWithIframeRedirectUrl(),
                                               "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Fetch reduce accept-language when visiting the following three URLs, for
  // css request, it won't pass to navigation layer:
  // * top_level_with_iframe_redirect_url(2):one fetch for initially adding
  // header and another one for the restart request adding header.
  // * subframe_3p_request_url(1): one fetch for initially adding header.
  // * other_site_css_request_url(0): directly read from commit parameter.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);
  // One store for top_level_with_iframe_redirect_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subresource_redirect_style.css");
}

class FencedFrameReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest,
      public ::testing::WithParamInterface<
          blink::features::FencedFramesImplementationType> {
 public:
  static constexpr char kFirstPartyOriginUrl[] = "https://127.0.0.1:44444";
  static constexpr char kThirdPartyOriginUrl[] = "https://my-site.com:44444";

  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case blink::features::FencedFramesImplementationType::kShadowDOM:
        return "ShadowDOM";
      case blink::features::FencedFramesImplementationType::kMPArch:
        return "MPArch";
    }
  }

  GURL SameOriginFencedFrameUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/same_origin_fenced_frame.html"}));
  }

  GURL CrossOriginFencedFrameUrl() const {
    return GURL(
        base::StrCat({ReduceAcceptLanguageBrowserTest::kFirstPartyOriginUrl,
                      "/cross_origin_fenced_frame.html"}));
  }

  GURL SimpleRequestUrl() const {
    return GURL(base::StrCat({kFirstPartyOriginUrl, "/subframe_simple.html"}));
  }

  GURL SimpleThirdPartyRequestUrl() const {
    return GURL(
        base::StrCat({kThirdPartyOriginUrl, "/subframe_simple_3p.html"}));
  }

 protected:
  void EnabledFeatures() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames,
          {{"implementation_type",
            GetParam() ==
                    blink::features::FencedFramesImplementationType::kShadowDOM
                ? "shadow_dom"
                : "mparch"}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {network::features::kReduceAcceptLanguage, {}}},
        {/* disabled_features */});
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FencedFrameReduceAcceptLanguageBrowserTest,
    ::testing::Values(
        blink::features::FencedFramesImplementationType::kShadowDOM,
        blink::features::FencedFramesImplementationType::kMPArch),
    &FencedFrameReduceAcceptLanguageBrowserTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(FencedFrameReduceAcceptLanguageBrowserTest,
                       CrossOriginFencedFrame) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .variants_in_child = "accept-language=(zh)",
                  .vary_in_child = "accept-language",
                  .is_fenced_frame = true},
                 {CrossOriginFencedFrameUrl(), SimpleThirdPartyRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // The result of the main frame's language negotiation should not be shared
  // with requests made from fenced frames, since fenced frames restrict
  // communication with their outer page. After language negotiation, the
  // persisted language is en-us. The third party fenced frame requests should
  // use the first accept-language zh instead of en-us.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CrossOriginFencedFrameUrl(),
                                               "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * cross_region_fenced_frame_url(2):one fetch for initially adding
  // header and another one for the restart request adding header.
  // * simple_3p_request_url(1): one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);
  // One store for cross_region_fenced_frame_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple_3p.html");
}

IN_PROC_BROWSER_TEST_P(FencedFrameReduceAcceptLanguageBrowserTest,
                       SameOriginFencedFrame) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .variants_in_child = "accept-language=(zh)",
                  .vary_in_child = "accept-language",
                  .is_fenced_frame = true},
                 {SameOriginFencedFrameUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Main frame after language negotiation should not shared to fenced frame
  // subrequest since restricts communication.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginFencedFrameUrl(),
                                               "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * same_origin_fenced_frame_url(2):one fetch for initially adding
  // header and another one for the restart request adding header.
  // * simple_request_url(1): one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);
  // One store for cross_region_fenced_frame_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ("/subframe_simple.html", LastRequestUrl().path());
}

// Browser tests verify redirect same origin with different cases.
class SameOriginRedirectReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 public:
  SameOriginRedirectReduceAcceptLanguageBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/reduce_accept_language");

    https_server_.RegisterRequestMonitor(
        base::BindRepeating(&SameOriginRedirectReduceAcceptLanguageBrowserTest::
                                MonitorResourceRequest,
                            base::Unretained(this)));

    https_server_.RegisterRequestHandler(
        base::BindRepeating(&SameOriginRedirectReduceAcceptLanguageBrowserTest::
                                RequestHandlerRedirect,
                            base::Unretained(this)));

    EXPECT_TRUE(https_server_.Start());

    same_origin_redirect_ = https_server_.GetURL("/same_origin_redirect.html");
    same_origin_redirect_a_ =
        https_server_.GetURL("/same_origin_redirect_a.html");
    same_origin_redirect_b_ =
        https_server_.GetURL("/same_origin_redirect_b.html");
  }

  static constexpr const char kAcceptLanguage[] = "accept-language";
  static constexpr auto kValidPaths =
      base::MakeFixedFlatSet<base::StringPiece>({
          "/same_origin_redirect.html",
          "/same_origin_redirect_a.html",
          "/same_origin_redirect_b.html",
      });

  GURL same_origin_redirect() const { return same_origin_redirect_; }

  GURL same_origin_redirect_a() const { return same_origin_redirect_a_; }

  GURL same_origin_redirect_b() const { return same_origin_redirect_b_; }

  void SetOptions(const std::string& content_language_a,
                  const std::string& content_language_b) {
    content_language_a_ = content_language_a;
    content_language_b_ = content_language_b;
  }

  void VerifyURLAndAcceptLanguageSequence(
      const std::vector<std::vector<std::string>>& expect_url_accept_language) {
    EXPECT_EQ(actual_url_accept_language_, expect_url_accept_language);
  }

 protected:
  void EnabledFeatures() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("ReduceAcceptLanguage", "");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

 private:
  // Intercepts only the requests that for same origin redirect tests.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerRedirect(
      const net::test_server::HttpRequest& request) {
    if (!base::Contains(kValidPaths, request.relative_url))
      return nullptr;

    std::string accept_language;
    if (request.headers.find(kAcceptLanguage) != request.headers.end())
      accept_language = request.headers.find(kAcceptLanguage)->second;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (request.relative_url == "/same_origin_redirect.html") {
      response->set_code(net::HTTP_FOUND);
      // Assume site supports content_language_a_ and content_language_b_. If
      // accept-language matches content_language_b_ then returns
      // content_language_b_, otherwise returns content_language_a_.
      if (accept_language == content_language_b_) {
        response->AddCustomHeader("Content-Language", content_language_b_);
        response->AddCustomHeader("Location", same_origin_redirect_b().spec());
      } else {
        response->AddCustomHeader("Content-Language", content_language_a_);
        response->AddCustomHeader("Location", same_origin_redirect_a().spec());
      }
    } else if (request.relative_url == "/same_origin_redirect_a.html") {
      response->set_code(net::HTTP_OK);
      response->AddCustomHeader("Content-Language", content_language_a_);
    } else if (request.relative_url == "/same_origin_redirect_b.html") {
      response->set_code(net::HTTP_OK);
      response->AddCustomHeader("Content-Language", content_language_b_);
    }

    response->AddCustomHeader(
        "Variants", base::StrCat({"accept-language=(", content_language_a_, " ",
                                  content_language_b_, ")"}));
    return std::move(response);
  }

  // Called by `https_server_`.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    if (!base::Contains(kValidPaths, request.relative_url))
      return;

    if (request.headers.find(kAcceptLanguage) != request.headers.end()) {
      actual_url_accept_language_.push_back(
          {request.GetURL().spec(),
           request.headers.find(kAcceptLanguage)->second});
    }
  }

  GURL same_origin_redirect_;
  GURL same_origin_redirect_a_;
  GURL same_origin_redirect_b_;
  net::EmbeddedTestServer https_server_;
  std::string content_language_a_;
  std::string content_language_b_;
  std::vector<std::vector<std::string>> actual_url_accept_language_;
};

IN_PROC_BROWSER_TEST_F(SameOriginRedirectReduceAcceptLanguageBrowserTest,
                       MatchFirstLanguage) {
  SetPrefsAcceptLanguage({"en", "ja"});
  SetOptions(/*content_language_a=*/"en", /*content_language_b=*/"ja");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));

  // 1. initial request to main request(/) with first user accept-language
  // en.
  // 2. initial request to A(/en) with the language matches the expected
  // accept-language.
  VerifyURLAndAcceptLanguageSequence({{same_origin_redirect().spec(), "en"},
                                      {same_origin_redirect_a().spec(), "en"}});
}

IN_PROC_BROWSER_TEST_F(SameOriginRedirectReduceAcceptLanguageBrowserTest,
                       MatchSecondaryLanguage) {
  SetPrefsAcceptLanguage({"zh-CN", "ja"});
  SetOptions(/*content_language_a=*/"en", /*content_language_b=*/"ja");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));

  // 1. initial request to main request(/) with first user accept-language
  // zh-CN.
  // 2. restart request to main request(/) with the persisted language ja after
  // language negotiation.
  // 3. initial request to B(/ja) with the language matches the expected
  // accept-language.
  VerifyURLAndAcceptLanguageSequence({{same_origin_redirect().spec(), "zh-CN"},
                                      {same_origin_redirect().spec(), "ja"},
                                      {same_origin_redirect_b().spec(), "ja"}});
}

// Browser tests verify redirect cross origin A to B with different cases.
class CrossOriginRedirectReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 public:
  CrossOriginRedirectReduceAcceptLanguageBrowserTest()
      : https_server_a_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_b_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_a_.ServeFilesFromSourceDirectory(
        "chrome/test/data/reduce_accept_language");
    https_server_b_.ServeFilesFromSourceDirectory(
        "chrome/test/data/reduce_accept_language");

    https_server_a_.RegisterRequestMonitor(base::BindRepeating(
        &CrossOriginRedirectReduceAcceptLanguageBrowserTest::
            MonitorResourceRequest,
        base::Unretained(this)));

    https_server_a_.RegisterRequestHandler(base::BindRepeating(
        &CrossOriginRedirectReduceAcceptLanguageBrowserTest::
            RequestHandlerRedirect,
        base::Unretained(this)));

    https_server_b_.RegisterRequestMonitor(base::BindRepeating(
        &CrossOriginRedirectReduceAcceptLanguageBrowserTest::
            MonitorResourceRequest,
        base::Unretained(this)));
    https_server_b_.RegisterRequestHandler(base::BindRepeating(
        &CrossOriginRedirectReduceAcceptLanguageBrowserTest::
            RequestHandlerRedirect,
        base::Unretained(this)));

    EXPECT_TRUE(https_server_a_.Start());
    EXPECT_TRUE(https_server_b_.Start());
    // Make sure two origins are different.
    EXPECT_NE(https_server_a_.base_url(), https_server_b_.base_url());
    cross_origin_redirect_a_ =
        https_server_a_.GetURL("/cross_origin_redirect_a.html");
    cross_origin_redirect_b_ =
        https_server_b_.GetURL("/cross_origin_redirect_b.html");
  }

  static constexpr const char kAcceptLanguage[] = "accept-language";
  static constexpr auto kValidPaths =
      base::MakeFixedFlatSet<base::StringPiece>({
          "/cross_origin_redirect_a.html",
          "/cross_origin_redirect_b.html",
      });

  GURL cross_origin_redirect_a() const { return cross_origin_redirect_a_; }

  GURL cross_origin_redirect_b() const { return cross_origin_redirect_b_; }

  void SetOptions(const std::vector<std::string> variants_accept_language_a,
                  const std::vector<std::string> variants_accept_language_b) {
    variants_accept_language_a_ = variants_accept_language_a;
    variants_accept_language_b_ = variants_accept_language_b;
  }

  void ResetURLAndAcceptLanguageSequence() {
    actual_url_accept_language_.clear();
  }

  void VerifyURLAndAcceptLanguageSequence(
      const std::vector<std::vector<std::string>>& expect_url_accept_language) {
    EXPECT_EQ(actual_url_accept_language_, expect_url_accept_language);
  }

 protected:
  void EnabledFeatures() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("ReduceAcceptLanguage", "");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

 private:
  // Intercepts only the requests that for cross origin redirect tests.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerRedirect(
      const net::test_server::HttpRequest& request) {
    if (!base::Contains(kValidPaths, request.relative_url))
      return nullptr;

    std::string accept_language;
    if (request.headers.find(kAcceptLanguage) != request.headers.end())
      accept_language = request.headers.find(kAcceptLanguage)->second;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (request.relative_url == "/cross_origin_redirect_a.html") {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader(
          "Content-Language",
          GetResponseContentLanguage(accept_language,
                                     variants_accept_language_a_));
      response->AddCustomHeader(
          "Variants",
          base::StrCat({"accept-language=(",
                        base::JoinString(variants_accept_language_a_, " "),
                        ")"}));
      response->AddCustomHeader("Location", cross_origin_redirect_b().spec());
    } else if (request.relative_url == "/cross_origin_redirect_b.html") {
      response->set_code(net::HTTP_OK);
      response->AddCustomHeader(
          "Content-Language",
          GetResponseContentLanguage(accept_language,
                                     variants_accept_language_b_));
      response->AddCustomHeader(
          "Variants",
          base::StrCat({"accept-language=(",
                        base::JoinString(variants_accept_language_b_, " "),
                        ")"}));
    }
    return std::move(response);
  }

  // Called by `https_server_`.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    if (!base::Contains(kValidPaths, request.relative_url))
      return;

    if (request.headers.find(kAcceptLanguage) != request.headers.end()) {
      actual_url_accept_language_.push_back(
          {request.GetURL().spec(),
           request.headers.find(kAcceptLanguage)->second});
    }
  }

  // Mock the site set content-language behavior. If site supports the language
  // in the accept-language request header, set the content-language the same as
  // accept-language, otherwise set as the first available language.
  std::string GetResponseContentLanguage(
      const std::string& accept_language,
      const std::vector<std::string>& variants_languages) {
    auto iter =
        std::find_if(variants_languages.begin(), variants_languages.end(),
                     [&](const std::string& available_language) {
                       return accept_language == available_language;
                     });
    return iter != variants_languages.end() ? *iter : variants_languages[0];
  }

  GURL cross_origin_redirect_a_;
  GURL cross_origin_redirect_b_;
  net::EmbeddedTestServer https_server_a_;
  net::EmbeddedTestServer https_server_b_;
  std::vector<std::string> variants_accept_language_a_;
  std::vector<std::string> variants_accept_language_b_;
  std::vector<std::vector<std::string>> actual_url_accept_language_;
};

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageBrowserTest,
                       RestartOnA) {
  SetPrefsAcceptLanguage({"en-us", "zh"});
  SetOptions(/*variants_accept_language_a=*/{"ja", "zh"},
             /*variants_accept_language_b=*/{"en-us"});

  // initial redirect request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));

  // 1. initial request to A with first user accept-language en-us.
  // 2. restart request to A with the persisted language zh.
  // 3. initial request to B with the first user accept-language en-us.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-us"}});

  ResetURLAndAcceptLanguageSequence();

  // Secondary redirect request expects no restarts.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-us"}});
}

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageBrowserTest,
                       RestartOnB) {
  SetPrefsAcceptLanguage({"en-us", "zh"});
  SetOptions(/*variants_accept_language_a=*/{"en-us", "zh"},
             /*variants_accept_language_b=*/{"de", "zh"});

  // initial redirect request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));

  // 1. initial request to A with first user accept-language en-us.
  // 2. initial request to B with the first user accept-language en-us.
  // 3. restart request to A with first user accept-language en-us.
  // 4. restart request to B with the persisted language zh.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us"},
       {cross_origin_redirect_b().spec(), "en-us"},
       {cross_origin_redirect_a().spec(), "en-us"},
       {cross_origin_redirect_b().spec(), "zh"}});

  ResetURLAndAcceptLanguageSequence();

  // Secondary redirect request expects no restarts.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us"},
       {cross_origin_redirect_b().spec(), "zh"}});
}

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageBrowserTest,
                       RestartBothAB) {
  SetPrefsAcceptLanguage({"en-us", "zh"});
  SetOptions(/*variants_accept_language_a=*/{"ja", "zh"},
             /*variants_accept_language_b=*/{"de", "zh"});

  // initial redirect request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));

  // 1. initial request to A with first user accept-language en-us.
  // 2. restart request to A with the persisted language zh.
  // 3. initial request to B with the first user accept-language en-us.
  // 4. restart request to A since redirect the original URL with persisted
  // language zh.
  // 5. restart request to B with the persisted language zh.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-us"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "zh"}});

  ResetURLAndAcceptLanguageSequence();

  // Secondary redirect request expects no restarts.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "zh"}});
}
