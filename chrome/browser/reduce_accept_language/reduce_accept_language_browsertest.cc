// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
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
#include "components/embedder_support/switches.h"
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

enum class FeatureEnableType { FeatureFlagEnable, OriginTrialEnable };

struct ReduceAcceptLanguageTestOptions {
  absl::optional<std::string> content_language_in_parent = absl::nullopt;
  absl::optional<std::string> variants_in_parent = absl::nullopt;
  absl::optional<std::string> vary_in_parent = absl::nullopt;
  absl::optional<std::string> content_language_in_child = absl::nullopt;
  absl::optional<std::string> variants_in_child = absl::nullopt;
  absl::optional<std::string> vary_in_child = absl::nullopt;
  bool is_fenced_frame = false;
};

struct ServerPortAndValidOriginToken {
  int port;
  std::string token;
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

static constexpr const char kFirstPartyOriginUrl[] = "https://127.0.0.1:44444";
static constexpr char kThirdPartyOriginUrl[] = "https://my-site.com:44444";

// Notes: Only use to test origin trial feature with URLLoaderInterceptor.
// generate_token.py https://127.0.0.1:44444 ReduceAcceptLanguage
// --expire-timestamp=2000000000
static constexpr const char kValidFirstPartyToken[] =
    "A/"
    "G09eTht7RFkWhm4ZJpY52cJ5OwzQ+"
    "UZG479jtGNTDhOcn4aZxwfptBJdCra1sn88R81ZqryWDQa2VAzXbLegIAAABeeyJvcmlnaW4iO"
    "iAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5"
    "ndWFnZSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==";

// Notes: Only use to test origin trial feature with URLLoaderInterceptor.
// generate_token.py https://my-site.com:44444 ReduceAcceptLanguage
// --is-third-party --expire-timestamp=2000000000
static constexpr const char kValidThirdPartyToken[] =
    "AyluNgtXRhECzUbr3uisA06MmzzhHjbUG6HBQnk6BBjT+Z9iUH2KG/"
    "EmrDW+"
    "zj5pycYyavqEbnorgiaKeP0szwUAAAB2eyJvcmlnaW4iOiAiaHR0cHM6Ly9teS1zaXRlLmNvbT"
    "o0NDQ0NCIsICJmZWF0dXJlIjogIlJlZHVjZUFjY2VwdExhbmd1YWdlIiwgImV4cGlyeSI6IDIw"
    "MDAwMDAwMDAsICJpc1RoaXJkUGFydHkiOiB0cnVlfQ==";

static constexpr const char kInvalidOriginToken[] =
    "AjfC47H1q8/Ho5ALFkjkwf9CBK6oUUeRTlFc50Dj+eZEyGGKFIY2WTxMBfy8cLc3"
    "E0nmFroDA3OmABmO5jMCFgkAAABXeyJvcmlnaW4iOiAiaHR0cDovL3ZhbGlkLmV4"
    "YW1wbGUuY29tOjgwIiwgImZlYXR1cmUiOiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6"
    "IDIwMDAwMDAwMDB9";

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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The public key for the default private key used by the
    // tools/origin_trials/generate_token.py tool.
    static constexpr char kOriginTrialTestPublicKey[] =
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
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

  GURL SameOriginIframeUrl() const {
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
    for (const auto& result : languages_list.GetList())
      actual_languages.push_back(result.GetString());

    EXPECT_EQ(expect_languages, actual_languages);
  }

  void SetPrefsAcceptLanguage(
      const std::vector<std::string>& accept_languages) {
    auto language_prefs = std::make_unique<language::LanguagePrefs>(
        browser()->profile()->GetPrefs());
    language_prefs->SetUserSelectedLanguagesList(accept_languages);
  }

  // Mock the site set content-language behavior. If site supports the language
  // in the accept-language request header, set the content-language the same as
  // accept-language, otherwise set as the first available language.
  std::string GetResponseContentLanguage(
      const std::string& accept_language,
      const std::vector<std::string>& variants_languages) {
    auto iter = base::ranges::find(variants_languages, accept_language);
    return iter != variants_languages.end() ? *iter : variants_languages[0];
  }

 protected:
  // Return the feature list for the tests.
  virtual void EnabledFeatures() = 0;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Returns whether a given |header| has been received for the last request.
  bool HasReceivedHeader(const std::string& header) const {
    return url_loader_interceptor_->GetLastRequestHeaders().HasHeader(header);
  }

  void ResetURLAndAcceptLanguageSequence() {
    actual_url_accept_language_.clear();
  }

  void VerifyURLAndAcceptLanguageSequence(
      const std::vector<std::vector<std::string>>& expect_url_accept_language,
      const std::string& message = "") {
    EXPECT_EQ(actual_url_accept_language_, expect_url_accept_language)
        << message;
  }

  // As origin trial needs to start a service in a specific port instead of
  // random port, sometime the specific port is not ready, this can cause tests
  // are flaky. Allow test server to retry on provided ports and set the origin
  // trial token if server starts succeed.
  void StartTestServerAndSetToken(
      net::EmbeddedTestServer* http_server,
      std::vector<ServerPortAndValidOriginToken> port_tokens,
      bool third_party_origin = false) {
    // Try start server in random ports.
    if (port_tokens.empty()) {
      EXPECT_TRUE(http_server->Start());
      return;
    }

    // Try different ports and assign the origin token.
    bool started = false;
    for (size_t i = 0; i < port_tokens.size(); i++) {
      LOG(INFO) << "Start server on port " << port_tokens[i].port
                << " in attempt " << i << ".";
      started = http_server->Start(port_tokens[i].port);

      if (started) {
        third_party_origin ? SetValidThirdPartyToken(port_tokens[i].token)
                           : SetValidFirstPartyToken(port_tokens[i].token);
        break;
      }
    }
    EXPECT_TRUE(started);
  }

  void SetValidFirstPartyToken(const std::string& token) {
    valid_first_party_token_ = token;
  }

  void SetValidThirdPartyToken(const std::string& token) {
    valid_third_party_token_ = token;
  }

  void SetOriginTrialFirstPartyToken(const std::string& token) {
    origin_trial_first_party_token_ = token;
  }

  void SetOriginTrialThirdPartyToken(const std::string& token) {
    origin_trial_third_party_token_ = token;
  }

  std::vector<std::vector<std::string>> actual_url_accept_language_;
  std::string origin_trial_first_party_token_;
  std::string origin_trial_third_party_token_;
  std::string valid_first_party_token_;
  std::string valid_third_party_token_;

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

    // Build mock header for the first party origin if the token is not empty.
    if (!origin_trial_first_party_token_.empty()) {
      base::StrAppend(
          &headers,
          {"Origin-Trial: ", origin_trial_first_party_token_, "\r\n"});
    }

    // Only build mock header with third party origin trial tokens for the third
    // party requests.
    const GURL origin = params->url_request.url.DeprecatedGetOriginAsURL();
    if (!origin_trial_third_party_token_.empty() &&
        origin == GURL(kThirdPartyOriginUrl)) {
      base::StrAppend(
          &headers,
          {"Origin-Trial: ", origin_trial_third_party_token_, "\r\n"});
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
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Expect no Accept-Language header added because browser_tests can only check
  // headers in navigation layer, browser_tests can't see headers added by
  // network stack.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(),
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
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
  histograms_after.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
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
  histograms2.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
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
  histograms3.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  // Expect no perf storage updates.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       SubresourceRequestNoRestart) {
  base::HistogramTester histograms;
  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginImgUrl(), SimpleImgUrl()});
  SetPrefsAcceptLanguage({"es", "en-us"});

  // Initial request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginImgUrl(), "es");
  EXPECT_EQ(LastRequestUrl().path(), "/subresource_simple.jpg");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure no restart happens.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  // Total two different url requests:
  // * same_origin_img.html: one fetch for initially adding header.
  // * subresource_simple.jpg: no prefs read, it directly reads from the
  // navigation commit language.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
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
  histograms_after.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  // Expect no perf storage updates.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       PersistedAcceptLanguageNotAvailable) {
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

  base::HistogramTester histograms;
  SetPrefsAcceptLanguage({"zh"});
  // The third request should send out with the first accept-language since the
  // persisted language not available in latest user's accept-language list.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "zh");
  // The previous persisted language `en-US` is not in the user's preference
  // list. Verify that a language clear operation occurred.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.ClearLatency", 1);
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
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage(base::SplitString(
      kLargeLanguages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "en-US");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);

  // Total two different url requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 3);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");

  // Disable script for first party origin.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingCustomScope(
          ContentSettingsPattern::FromURL(GURL(kFirstPartyOriginUrl)),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::JAVASCRIPT,
          CONTENT_SETTING_BLOCK);

  // Even Script disabled, it still expects reduced accept-language. The second
  // navigation should use the language after negotiation which is en-us.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "en-US");
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
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
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 3);
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
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 3);
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
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 3);
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
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(), "en-us");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure restart happen once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // Total two different URL requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 3);
  // One store for same_origin_iframe_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}

class ThirdPartyReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 public:
  static constexpr char kOtherSiteOriginUrl[] = "https://other-site.com:44445";
  static constexpr char kOtherSiteBOriginUrl[] =
      "https://other-site-b.com:44445";

  GURL CrossOriginIframeUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/cross_origin_iframe.html"}));
  }

  GURL TopLevelWithIframeRedirectUrl() const {
    return GURL(base::StrCat(
        {kFirstPartyOriginUrl, "/top_level_with_iframe_redirect.html"}));
  }

  GURL CrossOriginIframeWithSubresourceUrl() const {
    return GURL(base::StrCat(
        {kFirstPartyOriginUrl, "/cross_origin_iframe_with_subrequests.html"}));
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 3);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 4);
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
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 3);
  // One store for top_level_with_iframe_redirect_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subresource_redirect_style.css");
}

class FencedFrameReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 public:
  static constexpr char kFirstPartyOriginUrl[] = "https://127.0.0.1:44444";
  static constexpr char kThirdPartyOriginUrl[] = "https://my-site.com:44444";

  GURL SameOriginFencedFrameUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/same_origin_fenced_frame.html"}));
  }

  GURL CrossOriginFencedFrameUrl() const {
    return GURL(base::StrCat(
        {kFirstPartyOriginUrl, "/cross_origin_fenced_frame.html"}));
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
        {{blink::features::kFencedFrames, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {network::features::kReduceAcceptLanguage, {}}},
        {/* disabled_features */});
  }
};

IN_PROC_BROWSER_TEST_F(FencedFrameReduceAcceptLanguageBrowserTest,
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
  // * simple_3p_request_url: no fetch for initially adding header since a
  // fenced frame but not a main frame will result in a nullopt origin value
  // when getting top-level main frame origin. In this case, we set the
  // Accept-Language header with the first user’s accept-language.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  // One store for cross_region_fenced_frame_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple_3p.html");
}

IN_PROC_BROWSER_TEST_F(FencedFrameReduceAcceptLanguageBrowserTest,
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
  // * simple_request_url: no fetch for initially adding header since a fenced
  // frame but not a main frame will result in a nullopt origin value when
  // getting top-level main frame origin. In this case, we set the
  // Accept-Language header with the first user’s accept-language.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  // One store for cross_region_fenced_frame_url main frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ("/subframe_simple.html", LastRequestUrl().path());
}

// Browser tests verify redirect same origin with different cases.
class SameOriginRedirectReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 public:
  explicit SameOriginRedirectReduceAcceptLanguageBrowserTest(
      const std::vector<ServerPortAndValidOriginToken>& port_tokens = {})
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

    // Using a specified port for origin trial to generate token instead of
    // always using an auto selected one.
    StartTestServerAndSetToken(&https_server_, port_tokens);

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

    if (origin_trial_first_party_token_ != kInvalidOriginToken) {
      response->AddCustomHeader(
          "Variants", base::StrCat({"accept-language=(", content_language_a_,
                                    " ", content_language_b_, ")"}));
    }

    if (!origin_trial_first_party_token_.empty()) {
      response->AddCustomHeader("Origin-Trial",
                                origin_trial_first_party_token_);
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

  GURL same_origin_redirect_;
  GURL same_origin_redirect_a_;
  GURL same_origin_redirect_b_;
  net::EmbeddedTestServer https_server_;
  std::string content_language_a_;
  std::string content_language_b_;
};

IN_PROC_BROWSER_TEST_F(SameOriginRedirectReduceAcceptLanguageBrowserTest,
                       MatchFirstLanguage) {
  SetPrefsAcceptLanguage({"en", "ja"});
  SetOptions(/*content_language_a=*/"en", /*content_language_b=*/"ja");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));

  // 1. initial request to main request(/) with first user accept-language en.
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
  explicit CrossOriginRedirectReduceAcceptLanguageBrowserTest(
      const std::vector<ServerPortAndValidOriginToken>& port_tokens_a = {},
      const std::vector<ServerPortAndValidOriginToken>& port_tokens_b = {})
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

    // Using a specified port for origin trial to generate token instead of
    // always using an auto selected one.
    StartTestServerAndSetToken(&https_server_a_, port_tokens_a);
    StartTestServerAndSetToken(&https_server_b_, port_tokens_b, true);

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

  void SetOriginTrialFirstPartyToken(const std::string& origin_trial_token_a,
                                     const std::string& origin_trial_token_b) {
    origin_trial_token_a_ = origin_trial_token_a;
    origin_trial_token_b_ = origin_trial_token_b;
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
      // Stop sending Variants header as well if tests set an invalid origin
      // token.
      if (origin_trial_token_a_ != kInvalidOriginToken) {
        response->AddCustomHeader(
            "Variants",
            base::StrCat({"accept-language=(",
                          base::JoinString(variants_accept_language_a_, " "),
                          ")"}));
      }
      response->AddCustomHeader("Location", cross_origin_redirect_b().spec());
      if (!origin_trial_token_a_.empty()) {
        response->AddCustomHeader("Origin-Trial", origin_trial_token_a_);
      }
    } else if (request.relative_url == "/cross_origin_redirect_b.html") {
      response->set_code(net::HTTP_OK);
      response->AddCustomHeader(
          "Content-Language",
          GetResponseContentLanguage(accept_language,
                                     variants_accept_language_b_));
      if (origin_trial_token_b_ != kInvalidOriginToken) {
        response->AddCustomHeader(
            "Variants",
            base::StrCat({"accept-language=(",
                          base::JoinString(variants_accept_language_b_, " "),
                          ")"}));
      }
      if (!origin_trial_token_b_.empty()) {
        response->AddCustomHeader("Origin-Trial", origin_trial_token_b_);
      }
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

  GURL cross_origin_redirect_a_;
  GURL cross_origin_redirect_b_;
  net::EmbeddedTestServer https_server_a_;
  net::EmbeddedTestServer https_server_b_;
  std::vector<std::string> variants_accept_language_a_;
  std::vector<std::string> variants_accept_language_b_;
  std::string origin_trial_token_a_;
  std::string origin_trial_token_b_;
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

// Browser tests verify same origin redirect when ReduceAcceptLanguage origin
// trial enable.
// NOTES: As URLLoaderInterceptor doesn't support redirect, testing redirects
// with origin trial requires EmbeddedTestServer to start on specific ports, we
// can only add a single test in this test class in case the different tests run
// parallel to cause server can't starts on specific ports. It will cause tests
// flakiness. Also, we need to make sure it doesn't share port with any other
// browser_tests.
class SameOriginRedirectReduceAcceptLanguageOTBrowserTest
    : public SameOriginRedirectReduceAcceptLanguageBrowserTest {
 public:
  SameOriginRedirectReduceAcceptLanguageOTBrowserTest()
      : SameOriginRedirectReduceAcceptLanguageBrowserTest(
            GetValidPortsAndTokens()) {
    // Initialize with valid origin trial token.
    SetOriginTrialFirstPartyToken(GetValidFirstPartyToken());
  }

  // Work around solution to test redirect using EmbeddedTestServer. Make a list
  // port and corresponding OT token for test server to retry if port in use.
  // generate_token.py https://127.0.0.1:44455 ReduceAcceptLanguage
  // --expire-timestamp=2000000000
  const std::vector<ServerPortAndValidOriginToken>& GetValidPortsAndTokens() {
    static const base::NoDestructor<std::vector<ServerPortAndValidOriginToken>>
        vec({
            {44455,
             "AzSllhJ98+RSJMfR6M+Y+"
             "x3jxeFpelgI5Vl1nWuclvx2pcGOnRUwaOKXKQSa9jAeclvkuxgdBfENmhA3ZLGzAw"
             "oAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NTUiLCAiZmVhdHV"
             "yZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnkiOiAyMDAwMDAwMDAw"
             "fQ=="},
            {44456,
             "A4uA7J+"
             "vnItIm0hSGWrKOTT2mk7hYwyCIbBjH00QTtrITFNaRkBPcjfkwi5IHkjHjBTtqq2F"
             "0RXgLbB9MM7xWAcAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0N"
             "TYiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnkiOi"
             "AyMDAwMDAwMDAwfQ=="},
            {44457,
             "Az33aL7s0NkKODCoHmeHia1Bw9s6cPBdL4NJZJcIhFpnR60Dd76Vcb8NJhge/"
             "j8FkZ/"
             "FptxJi01YJBoQyyor9QMAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6"
             "NDQ0NTciLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpc"
             "nkiOiAyMDAwMDAwMDAwfQ=="},
            {44458,
             "Az1nlieDv/dL0a41vnsh5RbommI/"
             "twzSJorFqSoBbUCehLo1HpeuyrRUNosqBFqHlveIgpx7Pf3h3v1bJnEo1QYAAABee"
             "yJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NTgiLCAiZmVhdHVyZSI6IC"
             "JSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ=="},
            {44459,
             "A+sVf8DEBgKznNyLtNDiMO7HnE+cfBddPCIjVglIXZCj9+HkXKv1+"
             "b8D3lubralKDSlwL/"
             "quRzYQENR41DinZwUAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ"
             "0NTkiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnki"
             "OiAyMDAwMDAwMDAwfQ=="},
        });
    return *vec;
  }

  std::string GetValidFirstPartyToken() { return valid_first_party_token_; }

 protected:
  void EnabledFeatures() override {
    // Explicit disable feature ReduceAcceptLanguage but enable
    // ReduceAcceptLanguageOriginTrial.
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("ReduceAcceptLanguageOriginTrial",
                                            "ReduceAcceptLanguage");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(SameOriginRedirectReduceAcceptLanguageOTBrowserTest,
                       MatchFirstLanguage) {
  // Match the first language
  SetPrefsAcceptLanguage({"en", "ja"});
  SetOptions(/*content_language_a=*/"en", /*content_language_b=*/"ja");
  SetOriginTrialFirstPartyToken(GetValidFirstPartyToken());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));

  // First Request.
  // 1. initial request to main request(/) with unreduced user accept-language
  // since we can't validate origin trial token before sending requests.
  // 2. initial request to A(/en) with the reduced language en which persisted
  // when process request to main request(/).
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "en,ja;q=0.9"},
       {same_origin_redirect_a().spec(), "en"}},
      "Verifying the first request sequence failed in matching first "
      "language.");

  // Second request.
  ResetURLAndAcceptLanguageSequence();
  // 1. Second request to main request(/) with the reduced accept-language en.
  // 2. Second request to A(/en) with the reduced accept-language en.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "en"},
       {same_origin_redirect_a().spec(), "en"}},
      "Verifying the second request sequence failed in matching first "
      "language.");

  // Third Request: reset origin trial token to be invalid.
  SetOriginTrialFirstPartyToken(kInvalidOriginToken);
  ResetURLAndAcceptLanguageSequence();
  // 1. Third request to main request(/) with the reduced accept-language en.
  // 2. Third request to A(/en) with the reduced accept-language en.
  // All persisted languages for the givin origin should be cleaned in this
  // request, all subsequent requests should start sending unreduced
  // accept-language.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "en"},
       {same_origin_redirect_a().spec(), "en,ja;q=0.9"}},
      "Verifying the third request sequence failed in matching first "
      "language.");

  // Fourth request.
  ResetURLAndAcceptLanguageSequence();
  // 1. Fourth request to main request(/) with the unreduced accept-language.
  // 2. Fourth request to A(/en) with the unreduced accept-language.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "en,ja;q=0.9"},
       {same_origin_redirect_a().spec(), "en,ja;q=0.9"}},
      "Verifying the fourth request sequence failed in matching first "
      "language.");
}

IN_PROC_BROWSER_TEST_F(SameOriginRedirectReduceAcceptLanguageOTBrowserTest,
                       MatchNonPrimaryLanguage) {
  // Match non primary language
  SetPrefsAcceptLanguage({"zh-CN", "ja"});
  SetOptions(/*content_language_a=*/"en", /*content_language_b=*/"ja");
  SetOriginTrialFirstPartyToken(GetValidFirstPartyToken());

  ResetURLAndAcceptLanguageSequence();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));

  // First Request.
  // 1. initial request to main request(/) with unreduced user accept-language
  // since we can't validate origin trial token before sending requests.
  // 2. restart request to main request(/) with the persisted language ja after
  // language negotiation.
  // 3. initial request to B(/ja) with the language matches the expected
  // accept-language.
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "zh-CN,zh;q=0.9,ja;q=0.8"},
       {same_origin_redirect().spec(), "ja"},
       {same_origin_redirect_b().spec(), "ja"}},
      "Verifying the first request sequence failed in matching non-primary "
      "language.");

  // Second request.
  ResetURLAndAcceptLanguageSequence();
  // 1. Second request to main request(/) with the reduced accept-language ja.
  // 2. Second request to B(/ja) with the language matches the expected
  // accept-language.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "ja"},
       {same_origin_redirect_b().spec(), "ja"}},
      "Verifying the second request sequence failed in matching non-primary "
      "language.");

  // Third Request: reset origin trial token to be invalid.
  SetOriginTrialFirstPartyToken(kInvalidOriginToken);
  ResetURLAndAcceptLanguageSequence();
  // 1. Third request to main request(/) with the reduced accept-language ja.
  // 2. Third request to B(/ja) with the reduced accept-language ja.
  // All persisted languages for the givin origin should be cleaned in this
  // request, all subsequent requests should start sending unreduced
  // accept-language.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "ja"},
       {same_origin_redirect_b().spec(), "zh-CN,zh;q=0.9,ja;q=0.8"}},
      "Verifying the third request sequence failed in matching non-primary "
      "language.");

  // Fourth request.
  ResetURLAndAcceptLanguageSequence();
  // 1. Fourth request to main request(/) with the unreduced accept-language,
  // and redirect to the default page A(/en).
  // 2. Fourth request to A(/en) with the unreduced accept-language .
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "zh-CN,zh;q=0.9,ja;q=0.8"},
       {same_origin_redirect_a().spec(), "zh-CN,zh;q=0.9,ja;q=0.8"}},
      "Verifying the fourth request sequence failed in matching non-primary "
      "language.");
}

// Browser tests verify cross origin redirect when ReduceAcceptLanguage origin
// trial enable.
// NOTES: As URLLoaderInterceptor doesn't support redirect, testing redirects
// with origin trial requires EmbeddedTestServer to start on specific ports, we
// can only add a single test in this test class in case the different tests run
// parallel to cause server can't starts on specific ports. It will cause tests
// flakiness. Also, we need to make sure it doesn't share port with any other
// browser_tests.
class CrossOriginRedirectReduceAcceptLanguageOTBrowserTest
    : public CrossOriginRedirectReduceAcceptLanguageBrowserTest {
 public:
  CrossOriginRedirectReduceAcceptLanguageOTBrowserTest()
      : CrossOriginRedirectReduceAcceptLanguageBrowserTest(
            GetValidPortsAndTokensA(),
            GetValidPortsAndTokensB()) {}

  // generate_token.py https://127.0.0.1:44466 ReduceAcceptLanguage
  // --expire-timestamp=2000000000
  const std::vector<ServerPortAndValidOriginToken>& GetValidPortsAndTokensA() {
    static const base::NoDestructor<std::vector<ServerPortAndValidOriginToken>>
        vec({
            {44466,
             "A74Um5MF3xynlCdMKu2ZNGxTd6BHSw7cGe8BPyLKjIlXLGvj+"
             "HwaM7rqQuVcy4nm50oJOnLyGG0iRqV8Q18hYwMAAABeeyJvcmlnaW4iOiAiaHR0cH"
             "M6Ly8xMjcuMC4wLjE6NDQ0NjYiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5"
             "ndWFnZSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ=="},
            {44467,
             "AzHyNd8z73giti5cN3MIwrz3pOBUx/"
             "GGen8J7X2r7z8jdVJzppuQ6cz7kMwcd+"
             "d4zh4czc8L8MllbkOD5H5usAQAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4"
             "wLjE6NDQ0NjciLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJl"
             "eHBpcnkiOiAyMDAwMDAwMDAwfQ=="},
            {44468,
             "A/"
             "6KlK14FmDDKt3Q8sl6wpWyh+"
             "B7GJuR1Fgc38zaz7zniUCK4THnze81TwpJW0Ajfkb1tOjB6/"
             "bysQG0HChJNAsAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0Njg"
             "iLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnkiOiAy"
             "MDAwMDAwMDAwfQ=="},
            {44469,
             "A0yYuNVkqdOaWwAUCwORp+IK/m7i7bRQ/5lvSmPWKWT1+kmRKgrXnHQy+X/"
             "BeQ72Zph6YEW8t0UiwO66hf7usQwAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcu"
             "MC4wLjE6NDQ0NjkiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsI"
             "CJleHBpcnkiOiAyMDAwMDAwMDAwfQ=="},
            {44470,
             "A48A+Y2WRyD0epUMEYebGCJ6wHTKxFw36nCKwVgDyy/QFt1sxO0377R6EfHw/"
             "MQ14HTQdpUjXVtY79PsnSiKCwgAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC"
             "4wLjE6NDQ0NzAiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJ"
             "leHBpcnkiOiAyMDAwMDAwMDAwfQ=="},
        });
    return *vec;
  }

  // generate_token.py https://127.0.0.1:44477 ReduceAcceptLanguage
  // --expire-timestamp=2000000000
  const std::vector<ServerPortAndValidOriginToken>& GetValidPortsAndTokensB() {
    static const base::NoDestructor<std::vector<ServerPortAndValidOriginToken>>
        vec({
            {44477,
             "A50zxBqtR5a+Scjas+8QsZkgVnataBlED3mz8nT5e6UBkW4enP6iXR+53S+"
             "iN7qirF+Xy0+R8bEv+"
             "zdnzRbG0AkAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NzciLC"
             "AiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnkiOiAyMDA"
             "wMDAwMDAwfQ=="},
            {44478,
             "AxMYDxbKzeDQN9le2VPZhVPfgj8x0E8DEX4YVTqQsqs2w0VstbnapwfNq74AQRL5y"
             "bw4hav2w0fSV/"
             "Bo+"
             "BRO0QkAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NzgiLCAiZm"
             "VhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnkiOiAyMDAwMDA"
             "wMDAwfQ=="},
            {44479,
             "AwbImb/qUq/32dyTuOk4/nUOqcAewg3JDciTHv84oLAFA8MDByjEPihPrG5/"
             "foecZXSAU3+"
             "FcCM3jZvBvtuqiQgAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0"
             "NzkiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnkiO"
             "iAyMDAwMDAwMDAwfQ=="},
            {44480,
             "AzTbdbqLqo9sZVhyd/5SyLkOOZhz+7oJiN6bcl/"
             "4xrFIudWsm4XfNqsADWKFs7sjY/"
             "YQl4b4+f9+PGxA2+"
             "18bQsAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0ODAiLCAiZmV"
             "hdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsICJleHBpcnkiOiAyMDAwMDAw"
             "MDAwfQ=="},
            {44481,
             "A1+jJE8Wm18wBOx5zNB6M4WbgR//"
             "63HTtIiUwNBA1ZU7RATSZkX3H5fA+"
             "cEONlmigEUA01ORpEorVr3agh7GpAQAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMj"
             "cuMC4wLjE6NDQ0ODEiLCAiZmVhdHVyZSI6ICJSZWR1Y2VBY2NlcHRMYW5ndWFnZSI"
             "sICJleHBpcnkiOiAyMDAwMDAwMDAwfQ=="},
        });
    return *vec;
  }

  std::string GetValidTokenA() { return valid_first_party_token_; }

  std::string GetValidTokenB() { return valid_third_party_token_; }

  void VerifyRestartOnABBothABOptInOT() {
    SetPrefsAcceptLanguage({"en-us", "zh"});
    SetOptions(/*variants_accept_language_a=*/{"ja", "zh"},
               /*variants_accept_language_b=*/{"de", "zh"});

    // Set A opt-in and B opt-in the origin trial.
    SetOriginTrialFirstPartyToken(
        /*origin_trial_token_a=*/GetValidTokenA(),
        /*origin_trial_token_b=*/GetValidTokenB());

    ResetURLAndAcceptLanguageSequence();
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
    // 1. initial request to A with unreduced accept-language list.
    // 2. restart request to A with the persisted language zh.
    // 3. initial request to B with unreduced accept-language list.
    // 4. restart request to A since redirect the original URL with persisted
    // language zh.
    // 5. restart request to B with the persisted language zh.
    VerifyURLAndAcceptLanguageSequence(
        {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
         {cross_origin_redirect_a().spec(), "zh"},
         {cross_origin_redirect_b().spec(), "en-us,en;q=0.9,zh;q=0.8"},
         {cross_origin_redirect_a().spec(), "zh"},
         {cross_origin_redirect_b().spec(), "zh"}},
        "Verifying the first request sequence failed.");

    ResetURLAndAcceptLanguageSequence();
    // Secondary redirect request expects no restarts and continue with
    // persisted language.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
    VerifyURLAndAcceptLanguageSequence(
        {{cross_origin_redirect_a().spec(), "zh"},
         {cross_origin_redirect_b().spec(), "zh"}},
        "Verifying the second request sequence failed.");
  }

 protected:
  void EnabledFeatures() override {
    // Explicit disable feature ReduceAcceptLanguage but enable
    // ReduceAcceptLanguageOriginTrial.
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("ReduceAcceptLanguageOriginTrial",
                                            "ReduceAcceptLanguage");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageOTBrowserTest,
                       RestartOnA) {
  // Restart only happens on A, and only A opt-in the origin trial, then
  // invalidate only B's token.
  SetPrefsAcceptLanguage({"en-us", "zh"});
  SetOptions(/*variants_accept_language_a=*/{"ja", "zh"},
             /*variants_accept_language_b=*/{"en-us"});

  // Set A opt-in and B opt-out the origin trial.
  SetOriginTrialFirstPartyToken(
      /*origin_trial_token_a=*/GetValidTokenA(),
      /*origin_trial_token_b=*/kInvalidOriginToken);

  ResetURLAndAcceptLanguageSequence();
  // initial redirect request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  // 1. initial request to A with with unreduced user accept-language
  // since we can't validate origin trial token before sending requests.
  // 2. restart request to A with the persisted language zh.
  // 3. initial request to B with unreduced user accept-language since B
  // hasn't participated in the origin trial.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-us,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnA the first request sequence failed.");

  ResetURLAndAcceptLanguageSequence();
  // Secondary redirect request expects no restarts.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-us,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnA the second request sequence failed.");

  // Set A opt-out the origin trial.
  SetOriginTrialFirstPartyToken(/*origin_trial_token_a=*/kInvalidOriginToken,
                                /*origin_trial_token_b=*/kInvalidOriginToken);

  base::HistogramTester histograms;
  ResetURLAndAcceptLanguageSequence();
  // Accept-Language in the third request header is the same as the second
  // one, but it will clear the persisted language for the given origin.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-us,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnA the third request sequence failed.");
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Clear persist language for Origin A.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.ClearLatency", 1);

  ResetURLAndAcceptLanguageSequence();
  // Fourth request will start to send the unreduced Accept-Language header
  // once the given origin opt-out origin trial.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_b().spec(), "en-us,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnA the fourth request sequence failed.");
}

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageOTBrowserTest,
                       RestartOnB) {
  // Restart only happens on B, and only B opt-in the origin trial, then
  // invalidate only B's token.
  SetPrefsAcceptLanguage({"en-us", "zh"});
  SetOptions(/*variants_accept_language_a=*/{"en-us", "zh"},
             /*variants_accept_language_b=*/{"de", "zh"});

  // Set B opt-in and A opt-out the origin trial.
  SetOriginTrialFirstPartyToken(/*origin_trial_token_a=*/kInvalidOriginToken,
                                /*origin_trial_token_b=*/GetValidTokenB());

  ResetURLAndAcceptLanguageSequence();
  // Initial request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  // 1. initial request to A with the unreduced user accept-language since A
  // hasn't participated in the origin trial.
  // 2. initial request to B with unreduced user accept-language since we
  // can't validate B's origin trial token before sending requests.
  // 3. restart request to A still sends the unreduced user accept-language.
  // 4. restart request to B with the persisted language zh.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_b().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying RestartOnB the first request sequence failed.");

  ResetURLAndAcceptLanguageSequence();
  // Secondary redirect request expects no restarts, A sends unreduced
  // Accept-Language and B sends reduced Accept-Language header.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying RestartOnB the second request sequence failed.");

  // Set B opt-out the origin trial.
  SetOriginTrialFirstPartyToken(/*origin_trial_token_a=*/kInvalidOriginToken,
                                /*origin_trial_token_b=*/kInvalidOriginToken);

  base::HistogramTester histograms;
  ResetURLAndAcceptLanguageSequence();
  // Accept-Language in the third request header is the same as the second
  // one, but it will clear the persisted language for the given origin.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying RestartOnB the third request sequence failed.");
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Clear persisted language for origin B.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.ClearLatency", 1);

  ResetURLAndAcceptLanguageSequence();
  // Fourth request will start to send the unreduced Accept-Language header
  // once the given origin opt-out origin trial.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_b().spec(), "en-us,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnB the fourth request sequence failed.");
}

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageOTBrowserTest,
                       RestartOnAB) {
  // Restart on both A and B, and both origin opt-in the origin trial, then
  // invalidate A's and B's token.
  // Verify Accept-Language header in both A and B for the first two requests.
  VerifyRestartOnABBothABOptInOT();

  // Set A opt-out the origin trial.
  SetOriginTrialFirstPartyToken(kInvalidOriginToken, GetValidTokenB());

  base::HistogramTester histograms;
  ResetURLAndAcceptLanguageSequence();
  // Third request will clear A's persist language.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying RestartOnAB the third request sequence failed.");
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Clear persisted language for origin A.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.ClearLatency", 1);

  ResetURLAndAcceptLanguageSequence();
  // Request to verify A starts sending the unreduced Accept-Language header.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying RestartOnAB the fourth request sequence failed.");

  // Set A and B both opt-out the origin trial.
  SetOriginTrialFirstPartyToken(kInvalidOriginToken, kInvalidOriginToken);

  base::HistogramTester histograms2;
  ResetURLAndAcceptLanguageSequence();
  // Request will clear B's persist language.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying RestartOnAB the fifth request sequence failed.");
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Clear persisted language for origin B.
  histograms2.ExpectTotalCount("ReduceAcceptLanguage.ClearLatency", 1);

  ResetURLAndAcceptLanguageSequence();
  // Request verify both A and B start sending the unreduced Accept-Language
  // header.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-us,en;q=0.9,zh;q=0.8"},
       {cross_origin_redirect_b().spec(), "en-us,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnA the fourth request sequence failed.");
}

// Browser tests verify same origin origin trial.
class SameOriginReduceAcceptLanguageOTBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 public:
  void VerifySubrequestOriginTrial(const std::set<GURL>& expected_request_urls,
                                   const GURL& url,
                                   const std::string& last_request_path,
                                   int expect_fetch_count) {
    base::HistogramTester histograms;
    SetTestOptions({.content_language_in_parent = "es",
                    .variants_in_parent = "accept-language=(es en-US)",
                    .vary_in_parent = "accept-language",
                    .content_language_in_child = "es",
                    .variants_in_child = "accept-language=(es en-US)",
                    .vary_in_child = "accept-language"},
                   expected_request_urls);
    SetPrefsAcceptLanguage({"zh", "en-US"});

    SetOriginTrialFirstPartyToken(kValidFirstPartyToken);

    // Initial request.
    NavigateAndVerifyAcceptLanguageOfLastRequest(url, "en-US");
    EXPECT_EQ(LastRequestUrl().path(), last_request_path);

    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    // Ensure restart happen once.
    histograms.ExpectBucketCount(
        "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
        /*=kNavigationRestarted=*/3, 1);
    histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs",
                                expect_fetch_count);

    // Verify navigator.languages only returns an array length 1 if
    // has valid origin trial token.
    VerifyNavigatorLanguages({"zh"});

    // Second request with invalid origin token.
    SetOriginTrialFirstPartyToken(kInvalidOriginToken);
    // No Accept-Language added in content navigation request, network layer
    // will add user's Accept-Language list.
    NavigateAndVerifyAcceptLanguageOfLastRequest(url, absl::nullopt);
    EXPECT_EQ(LastRequestUrl().path(), last_request_path);
    VerifyNavigatorLanguages({"zh", "en-US"});
  }

  void VerifySameOriginRequestNoRestart(
      const absl::optional<std::string>& expect_accept_language,
      int expect_fetch_count) {
    base::HistogramTester histograms;
    // The first request won't add the Accept-Language in navigation request
    // since it can't verify the origin trial.
    NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                                 expect_accept_language);
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    // Ensure no restart happen.
    histograms.ExpectBucketCount(
        "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
        /*=kNavigationRestarted=*/3, 0);
    histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs",
                                expect_fetch_count);
    // Expect one storage update when response has a valid origin token.
    histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);
  }

  void VerifySameOriginRequestAfterTokenInvalid(
      const absl::optional<std::string>& expect_accept_language) {
    SetOriginTrialFirstPartyToken(kInvalidOriginToken);
    base::HistogramTester histograms;
    // First request after token invalid will continue send reduced header since
    // we can't verify the response header before preparing the request
    // headers.
    NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                                 expect_accept_language);
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    histograms.ExpectTotalCount("ReduceAcceptLanguage.ClearLatency", 1);

    // Subsequent requests should not add reduced Accept-Language header.
    NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                                 absl::nullopt);
  }

 protected:
  void EnabledFeatures() override {
    // Explicit disable feature ReduceAcceptLanguage but enable
    // ReduceAcceptLanguageOriginTrial.
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("ReduceAcceptLanguageOriginTrial",
                                            "ReduceAcceptLanguage");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageOTBrowserTest,
                       SimpleRequestOriginTrial_MatchPrimaryLanguage) {
  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});
  SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
  SetPrefsAcceptLanguage({"es", "zh"});

  // The first request won't add the Accept-Language in navigation request
  // since it can't verify the origin trial.
  // One fetch for initially checking whether need to add reduce Accept-Language
  // header and one fetch for navigation request commits when visiting
  // same_origin_request.html.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/absl::nullopt,
                                   /*expect_fetch_count=*/2);
  // The second request should send out with the persist language.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/"es",
                                   /*expect_fetch_count=*/1);
  VerifySameOriginRequestAfterTokenInvalid("es");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageOTBrowserTest,
                       SimpleRequestOriginTrial_MatchNonPrimaryLanguage) {
  {
    base::HistogramTester histograms;

    SetTestOptions({.content_language_in_parent = "es",
                    .variants_in_parent = "accept-language=(es en-US)",
                    .vary_in_parent = "accept-language"},
                   {SameOriginRequestUrl()});
    SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
    SetPrefsAcceptLanguage({"zh", "en-us"});

    // First request restarts and send Accept-Language with negotiated language:
    // en-us.
    NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                                 "en-us");
    // Ensure only restart once.
    histograms.ExpectBucketCount(
        "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
        /*=kNavigationRestarted=*/3, 1);

    // Two fetches for initially adding header and restart fetch.
    histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
    // Expect no perf storage updates.
    histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);
  }

  {
    SetTestOptions({.content_language_in_parent = "en-us",
                    .variants_in_parent = "accept-language=(es en-US)",
                    .vary_in_parent = "accept-language"},
                   {SameOriginRequestUrl()});

    // The second request should send out with the first matched negotiation
    // language en-us.
    VerifySameOriginRequestNoRestart(/*expect_accept_language=*/"en-us",
                                     /*expect_fetch_count=*/1);
    VerifySameOriginRequestAfterTokenInvalid("en-us");
  }
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageOTBrowserTest,
                       SimpleRequestOriginTrial_NoMatchLanguage) {
  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});
  SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
  SetPrefsAcceptLanguage({"zh", "ja"});

  // The first request won't add the Accept-Language in navigation request
  // since it can't verify the origin trial.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/absl::nullopt,
                                   /*expect_fetch_count=*/2);
  // The second request should send out with the persist language zh.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/"zh",
                                   /*expect_fetch_count=*/1);
  VerifySameOriginRequestAfterTokenInvalid("zh");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageOTBrowserTest,
                       IframeRequestOriginTrial) {
  // See `expect_fetch_count` explanation on test: IframeReduceAcceptLanguage.
  VerifySubrequestOriginTrial(
      /*expected_request_urls=*/{SameOriginIframeUrl(), SimpleRequestUrl()},
      /*url=*/SameOriginIframeUrl(),
      /*last_request_path=*/"/subframe_simple.html", /*expect_fetch_count=*/3);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageOTBrowserTest,
                       ImgSubresourceRequestOriginTrial) {
  // See `expect_fetch_count` explanation on test:
  // ImgSubresourceReduceAcceptLanguage.
  VerifySubrequestOriginTrial(
      /*expected_request_urls=*/{SameOriginImgUrl(), SimpleImgUrl()},
      /*url=*/SameOriginImgUrl(),
      /*last_request_path=*/"/subresource_simple.jpg",
      /*expect_fetch_count=*/2);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageOTBrowserTest,
                       SubresourceRequestNoRestart) {
  base::HistogramTester histograms;
  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language"},
                 {{SameOriginImgUrl(), SimpleImgUrl()}});
  SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
  SetPrefsAcceptLanguage({"es", "ja"});

  // Initial request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginImgUrl(), "es");
  EXPECT_EQ(LastRequestUrl().path(), "/subresource_simple.jpg");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure no restart happens.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  // Total two different url requests:
  // * same_origin_img.html: one fetch for initially adding header and one for
  // navigation request commits.
  // * subresource_simple.jpg: no prefs read, it directly reads from the
  // navigation commit language.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);

  // Verify navigator.languages only returns an array length 1 if
  // has valid origin trial token.
  VerifyNavigatorLanguages({"es"});

  // Second request with invalid origin token.
  SetOriginTrialFirstPartyToken(kInvalidOriginToken);
  // No Accept-Language added in content navigation request, network layer
  // will add user's Accept-Language list.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginImgUrl(),
                                               absl::nullopt);
  EXPECT_EQ(LastRequestUrl().path(), "/subresource_simple.jpg");
  VerifyNavigatorLanguages({"es", "ja"});
}

// Browser tests verify third party origin trial. Currently we are not
// supporting third-party origin trial.
class ThirdPartyReduceAcceptLanguageOTBrowserTest
    : public ThirdPartyReduceAcceptLanguageBrowserTest {
 protected:
  void EnabledFeatures() override {
    // Explicit disable feature ReduceAcceptLanguage but enable
    // ReduceAcceptLanguageOriginTrial.
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("ReduceAcceptLanguageOriginTrial",
                                            "ReduceAcceptLanguage");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(ThirdPartyReduceAcceptLanguageOTBrowserTest,
                       ThirdPartyOT_IframeRequests) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .variants_in_child = "accept-language=(zh)",
                  .vary_in_child = "accept-language"},
                 {CrossOriginIframeUrl(), SimpleThirdPartyRequestUrl()});

  SetOriginTrialThirdPartyToken(kValidThirdPartyToken);
  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Third party iframe subrequest expect no Accept-Language added in navigation
  // requests.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CrossOriginIframeUrl(),
                                               absl::nullopt);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure no restart happen.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  // One fetch for initially checking whether need to add reduce Accept-Language
  // header and one fetch for navigation request commits when visiting the
  // following two URLs:
  // * cross_origin_iframe_url.
  // * simple_3p_request_url.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 4);
  // No persist reduce accept language happens.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple_3p.html");

  // It won't send reduce Accept-Language when explicitly visiting the url with
  // origin token enabled third party.
  base::HistogramTester histograms2;
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  NavigateAndVerifyAcceptLanguageOfLastRequest(SimpleThirdPartyRequestUrl(),
                                               absl::nullopt);
  histograms2.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyReduceAcceptLanguageOTBrowserTest,
                       ThirdPartyOT_IframeWithSubresourceRequests) {
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

  SetOriginTrialThirdPartyToken(kValidThirdPartyToken);
  SetPrefsAcceptLanguage({"zh", "en-us"});

  // Third party iframe subrequest expect no Accept-Language added in navigation
  // requests.
  NavigateAndVerifyAcceptLanguageOfLastRequest(
      CrossOriginIframeWithSubresourceUrl(), absl::nullopt);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure no restart happen.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  // One fetch for initially checking whether need to add reduce Accept-Language
  // header and one fetch for navigation request commits when visiting the
  // following three URLs:
  // * cross_origin_iframe_with_subrequests_url.
  // * iframe_3p_request_url.
  // * other_site_b_basic_request_url.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 6);
  // No persist reduce accept language happens.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_iframe_basic.html");
}

// Browser tests verify disable origin trial feature flags.
class DisableReduceAcceptLanguageOTBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 public:
  void VerifyOriginTrialFeatureDisableWithValidToken(const GURL& url) {
    SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
    // Expect no Accept-Language header added for incoming requests.
    NavigateAndVerifyAcceptLanguageOfLastRequest(url, absl::nullopt);
    NavigateAndVerifyAcceptLanguageOfLastRequest(url, absl::nullopt);
    // Even though we disable the feature, blink will verify whether sites send
    // valid origin trial token in js getter. It will continue send the reduce
    // accept-language in navigator.languages if sites opt-in the origin trial.
    VerifyNavigatorLanguages({"zh"});
  }

 protected:
  void EnabledFeatures() override {
    // Explicit disable feature ReduceAcceptLanguage and
    // ReduceAcceptLanguageOriginTrial.
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(
        "", "ReduceAcceptLanguageOriginTrial,ReduceAcceptLanguage");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(DisableReduceAcceptLanguageOTBrowserTest,
                       SimpleRequestOriginTrialDisable) {
  SetTestOptions({.content_language_in_parent = "en",
                  .variants_in_parent = "accept-language=(en en-US)",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});
  SetPrefsAcceptLanguage({"zh", "en-us"});
  VerifyOriginTrialFeatureDisableWithValidToken(SameOriginRequestUrl());
}

IN_PROC_BROWSER_TEST_F(DisableReduceAcceptLanguageOTBrowserTest,
                       IframeRequestOriginTrialDisable) {
  SetTestOptions({.content_language_in_parent = "es",
                  .variants_in_parent = "accept-language=(es en-US)",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .variants_in_child = "accept-language=(es en-US)",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});
  SetPrefsAcceptLanguage({"zh", "en-us"});
  VerifyOriginTrialFeatureDisableWithValidToken(SameOriginIframeUrl());

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}
