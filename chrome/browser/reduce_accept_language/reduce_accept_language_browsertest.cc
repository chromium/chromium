// Copyright 2022 The Chromium Authors. All rights reserved.
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

    std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
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

    std::string resource_path = "chrome/test/data/reduce_accept_language";
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
  // Expect accept-language set as the first user's accept-language
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
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
  // One request, one prefs fetch when initial add header.
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
  // One request, one Prefs fetch request when initial add header.
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

  // TODO(victortan) For now, we haven't add the resend request logic, we expect
  // accept-language set as the first user's accept-language for the first
  // request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  // The second request should send out with the negotiation language en-us,
  // since sites have page language available in users' preferred language
  // en-us.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 2);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       SiteLanguageMatchPrimaryLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"es", "en-us"});

  // TODO(victortan) For now, we haven't add the resend request logic, we expect
  // accept-language set as the first user's accept-language for the first
  // request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "es");

  // The second request should send out with the same preferred language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "es");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
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

  // TODO(victortan) For now, we haven't add the resend request logic, we expect
  // accept-language set as the first user's accept-language for the first
  // request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  // The second request should send out with the first matched negotiation
  // language en-us instead of ja.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 2);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       SiteLanguageDontMatchAnyPreferredLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "ja"});

  // TODO(victortan) For now, we haven't add the resend request logic, we expect
  // accept-language set as the first user's accept-language for the first
  // request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  // The second request should send out with the same first preferred language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // Expect no perf storage updates.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
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
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);

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

  // TODO(victortan) For now, we haven't add the resend request logic, we
  // expect Subresource img request has the same language as the main frame
  // request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginImgUrl(), "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Total two requests, only same_origin_img_url request has one Prefs fetch
  // request when initial add header. For image request, it will directly read
  // the persisted from the navigation commit reduced accept language.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 1);
  // Only the first request has persisted language, embedded requests won't
  // persisted.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

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
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // Only the first request has persisted language, embedded requests won't
  // persisted.
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
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  // Only the first request has persisted language, embedded requests won't
  // persisted.
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
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
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
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
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
  // Total two requests, each has one Prefs fetch request when initial add
  // header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
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
  // Fetch reduce accept-language when visiting the following three URLs, for
  // css request, it won't pass to navigation layer:
  // * cross_origin_iframe_with_subrequests_url
  // * iframe_3p_request_url
  // * other_site_basic_request_url
  // each request has one Prefs fetch request when initial add header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 3);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatency", 2);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ("/subframe_simple.html", LastRequestUrl().path());
}
