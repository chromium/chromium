// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>

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
#include "content/public/browser/origin_trials_controller_delegate.h"
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
  std::optional<std::string> content_language_in_parent = std::nullopt;
  std::optional<std::string> avail_language_in_parent = std::nullopt;
  std::optional<std::string> vary_in_parent = std::nullopt;
  std::optional<std::string> content_language_in_child = std::nullopt;
  std::optional<std::string> avail_language_in_child = std::nullopt;
  std::optional<std::string> vary_in_child = std::nullopt;
  bool is_fenced_frame = false;
  bool is_critical_origin_trial = false;
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
// generate_token.py https://127.0.0.1:44444 DisableReduceAcceptLanguage
// --expire-timestamp=2000000000
static constexpr const char kValidFirstPartyToken[] =
    "A8iumEkw+XVtNR0dFIBuu6jDlDRPOxG4z9lVnq8bunWBNoV//lHIIrHkpQlzZ5Xr9sEW/"
    "0KZibE/"
    "Nrt+"
    "pC3qUQwAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAiZmVhdHVyZS"
    "I6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0"
    "=";

// Notes: Only use to test origin trial feature with URLLoaderInterceptor.
// generate_token.py https://my-site.com:44444 DisableReduceAcceptLanguage
// --is-third-party --expire-timestamp=2000000000
static constexpr const char kValidThirdPartyToken[] =
    "A8zLsSI/JcHxa+c6CvX2asTG2Uh62FUsb9jTZVszTHGyert8A22L/"
    "XgCdGVFjujmRtDHeAd7ctVgUr7IWWgVgAwAAAB9eyJvcmlnaW4iOiAiaHR0cHM6Ly9teS1zaXR"
    "lLmNvbTo0NDQ0NCIsICJmZWF0dXJlIjogIkRpc2FibGVSZWR1Y2VBY2NlcHRMYW5ndWFnZSIsI"
    "CJleHBpcnkiOiAyMDAwMDAwMDAwLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZX0=";

// Notes: Only use to test origin trial feature with URLLoaderInterceptor.
// generate_token.py https://my-site.com:44444 DisableReduceAcceptLanguage
// --expire-timestamp=2000000000
static constexpr const char kValidMySiteFirstPartyToken[] =
    "A5z0r3ggtGZbmJt+zZWHzeLJeeXdkzmi38nNssSJet5TbRS+"
    "gdKQy9f8b5YCJvK478XVHd6fCKXOSHgxNQTV2ggAAABneyJvcmlnaW4iOiAiaHR0cHM6Ly9teS"
    "1zaXRlLmNvbTo0NDQ0NCIsICJmZWF0dXJlIjogIkRpc2FibGVSZWR1Y2VBY2NlcHRMYW5ndWFn"
    "ZSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==";

static constexpr const char kInvalidOriginToken[] =
    "AjfC47H1q8/Ho5ALFkjkwf9CBK6oUUeRTlFc50Dj+eZEyGGKFIY2WTxMBfy8cLc3"
    "E0nmFroDA3OmABmO5jMCFgkAAABXeyJvcmlnaW4iOiAiaHR0cDovL3ZhbGlkLmV4"
    "YW1wbGUuY29tOjgwIiwgImZlYXR1cmUiOiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6"
    "IDIwMDAwMDAwMDB9";

static constexpr const char kDeprecationTrialName[] =
    "DisableReduceAcceptLanguage";

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
    // Clean up any saved settings after test run.
    browser()
        ->profile()
        ->GetOriginTrialsControllerDelegate()
        ->ClearPersistedTokens();

    url_loader_interceptor_.reset();
    intercepted_load_urls_.clear();
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

  GURL CrossOriginSubresourceUrl() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl, "/cross_origin_subresource.html"}));
  }

  GURL CrossOriginMetaTagInjectingJavascriptUrl() const {
    return GURL(base::StrCat({kThirdPartyOriginUrl, "/meta.js"}));
  }

  GURL CrossOriginCssRequestUrl() const {
    return GURL(base::StrCat(
        {kThirdPartyOriginUrl, "/subresource_redirect_style.css"}));
  }

  GURL CrossOriginSimpleImgUrl() const {
    return GURL(
        base::StrCat({kThirdPartyOriginUrl, "/subresource_simple.jpg"}));
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
      const std::vector<std::string>& avail_languages) {
    auto iter = base::ranges::find(avail_languages, accept_language);
    return iter != avail_languages.end() ? *iter : avail_languages[0];
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

  std::string GetFirstLanguage(std::string_view language_list) {
    auto end = language_list.find(",");
    return std::string(language_list.substr(0, end));
  }

  std::vector<std::vector<std::string>> actual_url_accept_language_;
  std::string origin_trial_first_party_token_;
  std::string origin_trial_third_party_token_;
  std::string valid_first_party_token_;
  std::string valid_third_party_token_;
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

    if (params->url_request.url == CrossOriginSubresourceUrl()) {
      return RespondForCrossOriginSubResourceOriginTrialUrl(params);
    }
    if (params->url_request.url == CrossOriginMetaTagInjectingJavascriptUrl()) {
      return RespondCrossOriginMetaTagInjectingScriptUrl(params);
    }

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
        base::MakeFixedFlatSet<std::string_view>({
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
      if (test_options_.is_critical_origin_trial) {
        base::StrAppend(
            &headers,
            {"Critical-Origin-Trial: ", kDeprecationTrialName, "\r\n"});
      }
    }

    // Only build mock header with third party origin trial tokens for the third
    // party requests.
    const GURL origin = params->url_request.url.DeprecatedGetOriginAsURL();
    if (!origin_trial_third_party_token_.empty() &&
        origin == GURL(kThirdPartyOriginUrl)) {
      base::StrAppend(
          &headers,
          {"Origin-Trial: ", origin_trial_third_party_token_, "\r\n"});
      if (test_options_.is_critical_origin_trial) {
        base::StrAppend(
            &headers,
            {"Critical-Origin-Trial: ", kDeprecationTrialName, "\r\n"});
      }
    }

    static constexpr auto kServiceWorkerPaths =
        base::MakeFixedFlatSet<std::string_view>({
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

  bool RespondForCrossOriginSubResourceOriginTrialUrl(
      URLLoaderInterceptor::RequestParams* params) {
    if (origin_trial_third_party_token_.empty()) {
      return false;
    }
    // Construct the origin trial header response.
    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    std::string body = base::StrCat(
        {"<html><head><script src=\"",
         CrossOriginMetaTagInjectingJavascriptUrl().spec(), "\"></script>",
         "<link rel=\"stylesheet\" href=\"", CrossOriginCssRequestUrl().spec(),
         "\">", "</head> <body> <img src=\"", CrossOriginSimpleImgUrl().spec(),
         "\"></img> This page has no title.</body></html>"});
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());
    return true;
  }

  bool RespondCrossOriginMetaTagInjectingScriptUrl(
      URLLoaderInterceptor::RequestParams* params) {
    if (origin_trial_third_party_token_.empty()) {
      return false;
    }
    // Construct the origin trial header response.
    std::string headers =
        "HTTP/1.1 200 OK\nContent-Type: application/javascript\n";
    std::string body =
        base::StrCat({"const otMeta = document.createElement('meta'); "
                      "otMeta.httpEquiv = 'origin-trial'; "
                      "otMeta.content = '",
                      origin_trial_third_party_token_,
                      "'; "
                      "document.head.append(otMeta); "});
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());
    return true;
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
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitFromCommandLine("", "ReduceAcceptLanguage");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
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
  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}

// Tests same origin requests with the ReduceAcceptLanguage feature enabled.
class SameOriginReduceAcceptLanguageBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 protected:
  void EnabledFeatures() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitFromCommandLine("ReduceAcceptLanguage", "");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       LargeLanguageListAndScriptDisable) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage(base::SplitString(
      kLargeLanguages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
  // Expect accept-language set as the negotiation language.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               "en-US,en;q=0.9");

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
  // navigation should use the language after negotiation which is en-US.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               "en-US,en;q=0.9");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
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

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
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

  // Verify navigator.languages only returns an array length 1 if
  // ReduceAcceptLanguage enabled.
  VerifyNavigatorLanguages({"zh"});
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
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
  // Ensure metrics report correctly.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kAvailLanguageAndContentLanguageHeaderPresent=*/2, 0);
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
  // Persist won't happen.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
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

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
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
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Expect accept-language set as negotiated language: en-US.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               "en-US,en;q=0.9");

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
  SetTestOptions({.content_language_in_parent = "en-US",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  // The second request should send out with the first matched negotiation
  // language en-US instead of ja.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               "en-US,en;q=0.9");
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
       .avail_language_in_parent = "es, en-US",
       .vary_in_parent = "accept-language"},
      {CreateServiceWorkerRequestUrl(), NavigationPreloadWorkerRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  base::HistogramTester histograms;
  // Expect accept-language set as negotiated language: en-US.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CreateServiceWorkerRequestUrl(),
                                               "en-US,en;q=0.9");
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
  // requests when site responses with expected content-language 'en-US'.
  base::HistogramTester histograms2;
  SetTestOptions(
      {.content_language_in_parent = "en-US",
       .avail_language_in_parent = "es, en-US",
       .vary_in_parent = "accept-language"},
      {CreateServiceWorkerRequestUrl(), NavigationPreloadWorkerRequestUrl()});

  NavigateAndVerifyAcceptLanguageOfLastRequest(CreateServiceWorkerRequestUrl(),
                                               "en-US,en;q=0.9");
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
       .avail_language_in_parent = "es, en-US",
       .vary_in_parent = "accept-language"},
      {CreateServiceWorkerRequestUrl(), NavigationPreloadWorkerRequestUrl()});

  NavigateAndVerifyAcceptLanguageOfLastRequest(CreateServiceWorkerRequestUrl(),
                                               "en-US,en;q=0.9");
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
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"es", "en-US"});

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
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginImgUrl(), SimpleImgUrl()});
  SetPrefsAcceptLanguage({"es", "en-US"});

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
                  .avail_language_in_parent = "es, en-US, ja",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US", "ja"});

  // Expect accept-language set as negotiated language: en-US.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               "en-US,en;q=0.9");

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
  SetTestOptions({.content_language_in_parent = "en-US",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  // The second request should send out with the first matched negotiation
  // language en-US instead of ja.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               "en-US,en;q=0.9");
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
                  .avail_language_in_parent = "es, en-US",
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
                  .avail_language_in_parent = "es, ja, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "ja", "en-US"});
  // The first request should send out with the negotiated language which is ja.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(), "ja");

  SetPrefsAcceptLanguage({"zh", "en-US"});
  // The second request should send out with the new negotiated language en-US.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               "en-US,en;q=0.9");

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
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage(base::SplitString(
      kLargeLanguages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(),
                                               "en-US,en;q=0.9");

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
  // navigation should use the language after negotiation which is en-US.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(),
                                               "en-US,en;q=0.9");
  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple.html");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageBrowserTest,
                       ImgSubresourceReduceAcceptLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language"},
                 {SameOriginImgUrl(), SimpleImgUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Subresource img request expect to be the language after language
  // negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginImgUrl(),
                                               "en-US,en;q=0.9");

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
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = std::nullopt,
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(),
                                               "en-US,en;q=0.9");

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

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(),
                                               "en-US,en;q=0.9");

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
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(),
                                               "en-US,en;q=0.9");

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
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .avail_language_in_child = "zh",
                  .vary_in_child = "accept-language"},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Iframe request expect to be the language after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginIframeUrl(),
                                               "en-US,en;q=0.9");

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
    feature_list->InitFromCommandLine("ReduceAcceptLanguage", "");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }
};

IN_PROC_BROWSER_TEST_F(ThirdPartyReduceAcceptLanguageBrowserTest,
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

  // Third party iframe subrequest expect to be the language of the main frame
  // after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CrossOriginIframeUrl(),
                                               "en-US,en;q=0.9");

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
       .avail_language_in_parent = "es, en-US",
       .vary_in_parent = "accept-language",
       .content_language_in_child = "zh",
       .avail_language_in_child = "zh",
       .vary_in_child = "accept-language"},
      {CrossOriginIframeWithSubresourceUrl(), IframeThirdPartyRequestUrl(),
       OtherSiteCssRequestUrl(), OtherSiteBasicRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Third party iframe subrequest expect to be the language of the main frame
  // after language negotiation.
  NavigateAndVerifyAcceptLanguageOfLastRequest(
      CrossOriginIframeWithSubresourceUrl(), "en-US,en;q=0.9");

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

  // All subresources should have been loaded,
  EXPECT_THAT(intercepted_load_urls_,
              testing::IsSupersetOf({IframeThirdPartyRequestUrl(),
                                     OtherSiteCssRequestUrl(),
                                     OtherSiteBasicRequestUrl()}));
}

IN_PROC_BROWSER_TEST_F(ThirdPartyReduceAcceptLanguageBrowserTest,
                       ThirdPartyIframeWithSubresourceRedirectRequests) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .avail_language_in_child = "zh",
                  .vary_in_child = "accept-language"},
                 {TopLevelWithIframeRedirectUrl(),
                  SubframeThirdPartyRequestUrl(), OtherSiteCssRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // It still expected an accept-language header has the reduced value even the
  // final url is a css style document,
  NavigateAndVerifyAcceptLanguageOfLastRequest(TopLevelWithIframeRedirectUrl(),
                                               "en-US,en;q=0.9");

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

  // All subresources should have been loaded,
  EXPECT_THAT(intercepted_load_urls_,
              testing::IsSupersetOf(
                  {SubframeThirdPartyRequestUrl(), OtherSiteCssRequestUrl()}));
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
         {blink::features::kFencedFramesAPIChanges, {}},
         {blink::features::kFencedFramesDefaultMode, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {network::features::kReduceAcceptLanguage, {}}},
        {/* disabled_features */});
  }
};

IN_PROC_BROWSER_TEST_F(FencedFrameReduceAcceptLanguageBrowserTest,
                       CrossOriginFencedFrame) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .avail_language_in_child = "zh",
                  .vary_in_child = "accept-language",
                  .is_fenced_frame = true},
                 {CrossOriginFencedFrameUrl(), SimpleThirdPartyRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

  // The result of the main frame's language negotiation should not be shared
  // with requests made from fenced frames, since fenced frames restrict
  // communication with their outer page. After language negotiation, the
  // persisted language is en-US. The third party fenced frame requests should
  // use the first accept-language zh instead of en-US.
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
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .avail_language_in_child = "zh",
                  .vary_in_child = "accept-language",
                  .is_fenced_frame = true},
                 {SameOriginFencedFrameUrl(), SimpleRequestUrl()});

  SetPrefsAcceptLanguage({"zh", "en-US"});

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
  static constexpr auto kValidPaths = base::MakeFixedFlatSet<std::string_view>({
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
    // Explicit enable feature ReduceAcceptLanguage.
    scoped_feature_list_.InitAndEnableFeature(
        {network::features::kReduceAcceptLanguage});
  }

 private:
  // Intercepts only the requests that for same origin redirect tests.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerRedirect(
      const net::test_server::HttpRequest& request) {
    if (!base::Contains(kValidPaths, request.relative_url))
      return nullptr;

    std::string accept_language;
    if (request.headers.find(kAcceptLanguage) != request.headers.end()) {
      accept_language =
          GetFirstLanguage(request.headers.find(kAcceptLanguage)->second);
    }

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
        "Avail-Language",
        base::StrCat({content_language_a_, ", ", content_language_b_}));

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
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "zh-CN,zh;q=0.9"},
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
  static constexpr auto kValidPaths = base::MakeFixedFlatSet<std::string_view>({
      "/cross_origin_redirect_a.html",
      "/cross_origin_redirect_b.html",
  });

  GURL cross_origin_redirect_a() const { return cross_origin_redirect_a_; }

  GURL cross_origin_redirect_b() const { return cross_origin_redirect_b_; }

  void SetOptions(const std::vector<std::string> avail_language_a,
                  const std::vector<std::string> avail_language_b) {
    avail_language_a_ = avail_language_a;
    avail_language_b_ = avail_language_b;
  }

  void SetOriginTrialFirstPartyToken(const std::string& origin_trial_token_a,
                                     const std::string& origin_trial_token_b) {
    origin_trial_token_a_ = origin_trial_token_a;
    origin_trial_token_b_ = origin_trial_token_b;
  }

 protected:
  void EnabledFeatures() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitFromCommandLine("ReduceAcceptLanguage", "");
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
          GetResponseContentLanguage(accept_language, avail_language_a_));

      // Stop sending Avail-Language header as well if tests set an invalid
      // origin token.
      response->AddCustomHeader("Avail-Language",
                                base::JoinString(avail_language_a_, ", "));

      response->AddCustomHeader("Location", cross_origin_redirect_b().spec());
      if (!origin_trial_token_a_.empty()) {
        response->AddCustomHeader("Origin-Trial", origin_trial_token_a_);
      }
    } else if (request.relative_url == "/cross_origin_redirect_b.html") {
      response->set_code(net::HTTP_OK);
      response->AddCustomHeader(
          "Content-Language",
          GetResponseContentLanguage(accept_language, avail_language_b_));
      response->AddCustomHeader("Avail-Language",
                                base::JoinString(avail_language_b_, ", "));
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
  std::vector<std::string> avail_language_a_;
  std::vector<std::string> avail_language_b_;
  std::string origin_trial_token_a_;
  std::string origin_trial_token_b_;
};

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageBrowserTest,
                       RestartOnA) {
  SetPrefsAcceptLanguage({"en-US", "zh"});
  SetOptions(/*avail_language_a=*/{"ja", "zh"},
             /*avail_language_b=*/{"en-US"});

  // initial redirect request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));

  // 1. initial request to A with first user accept-language en-US.
  // 2. restart request to A with the persisted language zh.
  // 3. initial request to B with the first user accept-language en-US.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"}});

  ResetURLAndAcceptLanguageSequence();

  // Secondary redirect request expects no restarts.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"}});
}

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageBrowserTest,
                       RestartOnB) {
  SetPrefsAcceptLanguage({"en-US", "zh"});
  SetOptions(/*avail_language_a=*/{"en-US", "zh"},
             /*avail_language_b=*/{"de", "zh"});

  // initial redirect request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));

  // 1. initial request to A with first user accept-language en-US.
  // 2. initial request to B with the first user accept-language en-US.
  // 3. restart request to A with first user accept-language en-US.
  // 4. restart request to B with the persisted language zh.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_b().spec(), "zh"}});

  ResetURLAndAcceptLanguageSequence();

  // Secondary redirect request expects no restarts.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_b().spec(), "zh"}});
}

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageBrowserTest,
                       RestartBothAB) {
  SetPrefsAcceptLanguage({"en-US", "zh"});
  SetOptions(/*avail_language_a=*/{"ja", "zh"},
             /*avail_language_b=*/{"de", "zh"});

  // initial redirect request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));

  // 1. initial request to A with first user accept-language en-US.
  // 2. restart request to A with the persisted language zh.
  // 3. initial request to B with the first user accept-language en-US.
  // 4. restart request to A since redirect the original URL with persisted
  // language zh.
  // 5. restart request to B with the persisted language zh.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"},
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

// Browser tests verify same origin redirect when DisableReduceAcceptLanguage
// deprecation origin trial enable.
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
  // generate_token.py https://127.0.0.1:44455 DisableReduceAcceptLanguage
  // --expire-timestamp=2000000000
  const std::vector<ServerPortAndValidOriginToken>& GetValidPortsAndTokens() {
    static const base::NoDestructor<std::vector<ServerPortAndValidOriginToken>>
        vec({
            {44455,
             "A1IOD8fXBTZ01WHMaBk9MqqvmiLPtIioHYEpcPn7kLtRHqJNL4pwZguJErdl+"
             "hIXpDYbR+"
             "7VnhXtv7YtEyaJzgoAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ"
             "0NTUiLCAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLCAi"
             "ZXhwaXJ5IjogMjAwMDAwMDAwMH0="},
            {44456,
             "A+QbN+WXtJCFPhyFkS2uW0VU3DdceOtQvO/"
             "8ZYL9CgicyLVyZQngYWZeahtT2Hy3978TOwrCD7D+"
             "AJGo1eseqAcAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NTYiL"
             "CAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZXhwaX"
             "J5IjogMjAwMDAwMDAwMH0="},
            {44457,
             "A75oT3Ki0N9WCQNOlzmB8+1s3pJMdNIT1DqeXkjF1LF8Xg6rfK65Z/"
             "bDKuBzDNMhTgoD5fN+"
             "RBRcVG8jLLWhSQ0AAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0N"
             "TciLCAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZX"
             "hwaXJ5IjogMjAwMDAwMDAwMH0="},
            {44458,
             "A3tN+D5Qyma6ozNdZPQIyu32bgG1Nwb3rQzwP8Su+"
             "57FbFQTGSXu3Wpr0HHOhkdKk50FVs849XJEv1pMjwDb7QQAAABleyJvcmlnaW4iOi"
             "AiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NTgiLCAiZmVhdHVyZSI6ICJEaXNhYmxlUmV"
             "kdWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0="},
            {44459,
             "A2bAXhaCHO5WNgU4xkHLG7UeYzD4lVlXBJxAO0/7U/"
             "Rie3s82v4AfWaOCZOd0YvOgxoTw8WVQZObpgIGkyQrQgYAAABleyJvcmlnaW4iOiA"
             "iaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NTkiLCAiZmVhdHVyZSI6ICJEaXNhYmxlUmVk"
             "dWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0="},
        });
    return *vec;
  }

  std::string GetValidFirstPartyToken() { return valid_first_party_token_; }

 protected:
  void EnabledFeatures() override {
    // Explicit enable feature ReduceAcceptLanguage.
    scoped_feature_list_.InitAndEnableFeature(
        {network::features::kReduceAcceptLanguage});
  }
};

IN_PROC_BROWSER_TEST_F(SameOriginRedirectReduceAcceptLanguageOTBrowserTest,
                       MatchFirstLanguage) {
  // Match the first language
  SetPrefsAcceptLanguage({"en", "ja"});
  SetOptions(/*content_language_a=*/"en", /*content_language_b=*/"ja");
  SetOriginTrialFirstPartyToken(GetValidFirstPartyToken());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));

  // First Request, opt-in the deprecation origin trial.
  // 1. initial request to main request(/) with reduced accept-language since
  // we can't validate deprecation origin trial token before sending requests.
  // 2. initial request to A(/en) with the reduced language en which persisted
  // when process request to main request(/).
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "en"},
       {same_origin_redirect_a().spec(), "en"}},
      "Verifying the first request sequence failed in matching first "
      "language.");

  // Second request.
  ResetURLAndAcceptLanguageSequence();
  // 1. Second request to main request(/) with the unreduced accept-language.
  // 2. Second request to A(/en) with the unreduced accept-language en.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "en,ja;q=0.9"},
       {same_origin_redirect_a().spec(), "en,ja;q=0.9"}},
      "Verifying the second request sequence failed in matching first "
      "language.");

  // Third Request: reset deprecation origin trial token to be invalid, opt-out
  // deprecation trial.
  SetOriginTrialFirstPartyToken(kInvalidOriginToken);
  ResetURLAndAcceptLanguageSequence();
  // 1. Third request to main request(/) with the unreduced accept-language.
  // 2. Third request to A(/en) with the unreduced accept-language.
  // All persisted languages for the givin origin should be cleaned in this
  // request, all subsequent requests should start sending unreduced
  // accept-language.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "en,ja;q=0.9"},
       {same_origin_redirect_a().spec(), "en,ja;q=0.9"}},
      "Verifying the third request sequence failed in matching first "
      "language.");

  // Fourth request.
  ResetURLAndAcceptLanguageSequence();
  // 1. Fourth request to main request(/) with the reduced accept-language en.
  // 2. Fourth request to A(/en) with the reduced accept-language en.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "en"},
       {same_origin_redirect_a().spec(), "en"}},
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

  // First Request, opt-in the deprecation origin trial.
  // 1. initial request to main request(/) with reduced accept-language zh-CN
  // since we can't validate deprecation origin trial token before sending
  // requests.
  // 2. restart request to main request(/) with the persisted language ja after
  // language negotiation.
  // 3. initial request to B(/ja) with the language matches the expected
  // accept-language.
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "zh-CN,zh;q=0.9"},
       {same_origin_redirect().spec(), "ja"},
       {same_origin_redirect_b().spec(), "ja"}},
      "Verifying the first request sequence failed in matching non-primary "
      "language.");

  // Second request.
  ResetURLAndAcceptLanguageSequence();
  // 1. Second request to main request(/) with unreduced accept-language.
  // 2. Second request to B(/ja) with unreduced accept-language.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "zh-CN,zh;q=0.9,ja;q=0.8"},
       {same_origin_redirect_a().spec(), "zh-CN,zh;q=0.9,ja;q=0.8"}},
      "Verifying the second request sequence failed in matching non-primary "
      "language.");

  // Third Request: reset deprecation origin trial token to be invalid, this is
  // also the first request opt-out deprecation trial.
  SetOriginTrialFirstPartyToken(kInvalidOriginToken);
  ResetURLAndAcceptLanguageSequence();
  // 1. Third request to main request(/) with unreduced accept-language.
  // 2. Third request to A(/en) with unreduced accept-language.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "zh-CN,zh;q=0.9,ja;q=0.8"},
       {same_origin_redirect_a().spec(), "zh-CN,zh;q=0.9,ja;q=0.8"}},
      "Verifying the third request sequence failed in matching non-primary "
      "language.");

  // Fourth request.
  ResetURLAndAcceptLanguageSequence();
  // 1. Fourth request to main request(/) with reduced accept-language zh-CN
  // since site opt-out the deprecation origin trial.
  // 2. restart request to main request(/) with the persisted language ja after
  // language negotiation.
  // 3. Fourth request to B(/ja) with the language matches the expected
  // accept-language.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_origin_redirect()));
  VerifyURLAndAcceptLanguageSequence(
      {{same_origin_redirect().spec(), "zh-CN,zh;q=0.9"},
       {same_origin_redirect().spec(), "ja"},
       {same_origin_redirect_b().spec(), "ja"}},
      "Verifying the fourth request sequence failed in matching non-primary "
      "language.");
}

// Browser tests verify cross origin redirect when DisableReduceAcceptLanguage
// origin trial enable.
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

  // generate_token.py https://127.0.0.1:44466 DisableReduceAcceptLanguage
  // --expire-timestamp=2000000000
  const std::vector<ServerPortAndValidOriginToken>& GetValidPortsAndTokensA() {
    static const base::NoDestructor<std::vector<ServerPortAndValidOriginToken>>
        vec({
            {44466,
             "A3yx7PpTSg5saq9Ni4by9/"
             "8lCmxy9kTkz9l2qcVCWTxE7m1hF5JNBJQnrnva5dEI0iozq08H+TsS3bFRptQ+"
             "vg0AAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NjYiLCAiZmVhd"
             "HVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZXhwaXJ5IjogMj"
             "AwMDAwMDAwMH0="},
            {44467,
             "A8uLuFYjCwhU1tNYQBC6JsK7QtZTrYe1QOeSU/irQMdmOaU/"
             "dXv6n7JWxS7vsQgEApOWyc58HVlIxr3TT8rT3A0AAABleyJvcmlnaW4iOiAiaHR0c"
             "HM6Ly8xMjcuMC4wLjE6NDQ0NjciLCAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQW"
             "NjZXB0TGFuZ3VhZ2UiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0="},
            {44468,
             "AyRZGI32JZcK6DYat+TahnkxJ+nrT/"
             "G9vw9DkmVMmMF7IZLFQ9PC3NKZ9Votiik9sGY+"
             "PSV8odfsLrqZtpHQzAwAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6N"
             "DQ0NjgiLCAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLC"
             "AiZXhwaXJ5IjogMjAwMDAwMDAwMH0="},
            {44469,
             "AxViEPVpoX+1KbTBnBv0HZoI5dukMCi0Ib4FQYPzadSdg3XWaFv+"
             "CnvdtWl7IyjQjLuO0v3cnFs797PjUrcDoQwAAABleyJvcmlnaW4iOiAiaHR0cHM6L"
             "y8xMjcuMC4wLjE6NDQ0NjkiLCAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZX"
             "B0TGFuZ3VhZ2UiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0="},
            {44470,
             "A657dEV4mXzjMLET+T6Z/"
             "iHO9WbXi+i7aC7g42WzY8I96tDKGGM6On4vKhqB6VlntM/Ec0iIw2DJ3/"
             "VrNMOpKg4AAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NzAiLCA"
             "iZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZXhwaXJ5"
             "IjogMjAwMDAwMDAwMH0="},
        });
    return *vec;
  }

  // generate_token.py https://127.0.0.1:44477 DisableReduceAcceptLanguage
  // --expire-timestamp=2000000000
  const std::vector<ServerPortAndValidOriginToken>& GetValidPortsAndTokensB() {
    static const base::NoDestructor<std::vector<ServerPortAndValidOriginToken>>
        vec({
            {44477,
             "A6P0N8LB9c/"
             "ZmFPpIyoPk7cSL3wv5adWfrmFI0FNfvY672xO96N9e5tLK2MtHm89YH2QIp4ROfow"
             "krIlh2OzxgIAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NzciL"
             "CAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZXhwaX"
             "J5IjogMjAwMDAwMDAwMH0="},
            {44478,
             "A1hljUHw96l+l4zSiLZjGwCdEJ8jMdZp0GJnBJIHO9nCvP6KiXJi/"
             "Ow5yRLy+"
             "8RtrWMyyTJVDaX3fbJCFpcavg0AAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC"
             "4wLjE6NDQ0NzgiLCAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3V"
             "hZ2UiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0="},
            {44479,
             "A2mynuvz4OL0gAkgDO1SNfLrAL7Mpb1aKJBzZ7TMby/"
             "nZQNEXed1Cr9mDAWoG1Kj6sD3ygKPgm68dnTo+"
             "7ujDQUAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NzkiLCAiZm"
             "VhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZXhwaXJ5Ijo"
             "gMjAwMDAwMDAwMH0="},
            {44480,
             "A0qwpXBuq0NOJjSypFvI2O59dKzJSGZKLXSgPYDS2N7IjX7nBHBtISWbGRXxm24QL"
             "puxWAUxyQ/"
             "RTdB4uGdy3AMAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0ODAi"
             "LCAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2UiLCAiZXhwa"
             "XJ5IjogMjAwMDAwMDAwMH0="},
            {44481,
             "A/aKNOr1iw10YYvsrJPF4TMpMFiyGUk/qGw3uUk4ZD/"
             "t0TFjxVZa7NsdLr5jFAVDT+"
             "aWyWTDu41hyBqkjWIrlQgAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE"
             "6NDQ0ODEiLCAiZmVhdHVyZSI6ICJEaXNhYmxlUmVkdWNlQWNjZXB0TGFuZ3VhZ2Ui"
             "LCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0="},
        });
    return *vec;
  }

  std::string GetValidTokenA() { return valid_first_party_token_; }

  std::string GetValidTokenB() { return valid_third_party_token_; }

 protected:
  void EnabledFeatures() override {
    // Explicit enable feature ReduceAcceptLanguage.
    scoped_feature_list_.InitAndEnableFeature(
        {network::features::kReduceAcceptLanguage});
  }
};

// Persistent origin trial doesn't works when a.com redirects to b.com and only
// a.com opt-in the deprecation origin trial, because persistent origin trial
// only parse and persist token for commit origin, in this redirect case b.com
// is always the commit origin.
IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageOTBrowserTest,
                       RestartOnA) {
  // Restart only happens on A, and only A opt-in the deprecation origin trial,
  // then invalidate only A's token.
  SetPrefsAcceptLanguage({"en-US", "zh"});
  SetOptions(/*avail_language_a=*/{"ja", "zh"},
             /*avail_language_b=*/{"en-US"});

  // Set A opt-in and B opt-out the origin trial.
  SetOriginTrialFirstPartyToken(
      /*origin_trial_token_a=*/GetValidTokenA(),
      /*origin_trial_token_b=*/kInvalidOriginToken);

  ResetURLAndAcceptLanguageSequence();
  // initial redirect request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  // 1. initial request to A with with reduced accept-language en-US since
  // we can't validate deprecation origin trial token before sending requests.
  // 2. restart request to A with the persisted language zh.
  // 3. initial request to B with reduced user accept-language en-US.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"}},
      "Verifying RestartOnA the first request sequence failed.");

  ResetURLAndAcceptLanguageSequence();
  // Secondary redirect request expects no restarts, but it won't send unreduced
  // accept-language. Persistent origin trial won't persist origin A's token in
  // case a.com redirects to b.com since it only persist the token for the
  // commit origin, in the redirect case, b.com is the commit origin.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"}},
      "Verifying RestartOnA the second request sequence failed.");

  // Set A opt-out the deprecation origin trial.
  SetOriginTrialFirstPartyToken(/*origin_trial_token_a=*/kInvalidOriginToken,
                                /*origin_trial_token_b=*/kInvalidOriginToken);

  base::HistogramTester histograms;
  ResetURLAndAcceptLanguageSequence();
  // Accept-Language in the third request header is the same as the second one,
  // it won't clear the persisted language since we can't validate a.com
  // deprecation trial token.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"}},
      "Verifying RestartOnA the third request sequence failed.");
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("ReduceAcceptLanguage.ClearLatency", 0);

  ResetURLAndAcceptLanguageSequence();
  // Fourth request will be the same as the third one.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"}},
      "Verifying RestartOnA the fourth request sequence failed.");
}

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageOTBrowserTest,
                       RestartOnB) {
  // Restart only happens on B, and only B opt-in the deprecation origin trial,
  // then invalidate only B's token.
  SetPrefsAcceptLanguage({"en-US", "zh"});
  SetOptions(/*avail_language_a=*/{"en-US", "zh"},
             /*avail_language_b=*/{"de", "zh"});

  // Set B opt-in and A opt-out the origin trial.
  SetOriginTrialFirstPartyToken(/*origin_trial_token_a=*/kInvalidOriginToken,
                                /*origin_trial_token_b=*/GetValidTokenB());

  ResetURLAndAcceptLanguageSequence();
  // Initial request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  // 1. initial request to A with the reduced accept-language en-US since A
  // hasn't participated in the deprecation origin trial.
  // 2. initial request to B with reduced accept-language en-US since we
  // can't validate B's deprecation origin trial token before sending requests.
  // 3. restart request to A still sends the reduced accept-language.
  // 4. restart request to B with the persisted language zh.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying RestartOnB the first request sequence failed.");

  ResetURLAndAcceptLanguageSequence();
  // Secondary redirect request expects no restarts, A sends reduced
  // Accept-Language and B sends unreduced Accept-Language header.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnB the second request sequence failed.");

  // Set B opt-out the origin trial.
  SetOriginTrialFirstPartyToken(/*origin_trial_token_a=*/kInvalidOriginToken,
                                /*origin_trial_token_b=*/kInvalidOriginToken);

  ResetURLAndAcceptLanguageSequence();
  // Accept-Language in the third request header is the same as the second one.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnB the third request sequence failed.");

  ResetURLAndAcceptLanguageSequence();
  // Fourth request will start to send the reduced Accept-Language header for B
  // and it will do the language negotiation once the given origin opt-out
  // the deprecation origin trial.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying RestartOnB the fourth request sequence failed.");
}

IN_PROC_BROWSER_TEST_F(CrossOriginRedirectReduceAcceptLanguageOTBrowserTest,
                       RestartOnAB) {
  // Restart on both A and B, and both origin opt-in the deprecation origin
  // trial, then invalidate A's and B's token. Verify Accept-Language header in
  // both A and B for the first two requests.
  SetPrefsAcceptLanguage({"en-US", "zh"});
  SetOptions(/*avail_language_a=*/{"ja", "zh"},
             /*avail_language_b=*/{"de", "zh"});

  // Set A opt-in and B opt-in the deprecation origin trial.
  SetOriginTrialFirstPartyToken(
      /*origin_trial_token_a=*/GetValidTokenA(),
      /*origin_trial_token_b=*/GetValidTokenB());

  ResetURLAndAcceptLanguageSequence();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  // 1. initial request to A with reduced accept-language of first language.
  // 2. restart request to A with the persisted language zh.
  // 3. initial request to B with reduced accept-language of first language.
  // 4. restart request to A since redirect the original URL with persisted
  // language zh.
  // 5. restart request to B with the persisted language zh.
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying the first request sequence failed.");

  ResetURLAndAcceptLanguageSequence();
  // Secondary redirect request expects no restarts and B starts to send
  // unreduced accept-language.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9,zh;q=0.8"}},
      "Verifying the second request sequence failed.");

  // Set A opt-out the deprecation origin trial.
  SetOriginTrialFirstPartyToken(kInvalidOriginToken, GetValidTokenB());

  ResetURLAndAcceptLanguageSequence();
  // For the first request after A opting-out the deprecation trial: there is no
  // changes on both A and B.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnAB the third request sequence failed.");

  ResetURLAndAcceptLanguageSequence();
  // For the second request after A opting-out the deprecation trial, it's the
  // same as the above request.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnAB the fourth request sequence failed.");

  // Set A and B both opt-out the deprecation origin trial.
  SetOriginTrialFirstPartyToken(kInvalidOriginToken, kInvalidOriginToken);

  ResetURLAndAcceptLanguageSequence();
  // There is no change for the first request after B opting-out the
  // deprecation trial.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9,zh;q=0.8"}},
      "Verifying RestartOnAB the fifth request sequence failed.");

  ResetURLAndAcceptLanguageSequence();
  // For the second request after B opting-out the deprecation trial, there is
  // no change on A, but B will do the language negotiation and send the new
  // reduced Accept-Language.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), cross_origin_redirect_a()));
  VerifyURLAndAcceptLanguageSequence(
      {{cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "en-US,en;q=0.9"},
       {cross_origin_redirect_a().spec(), "zh"},
       {cross_origin_redirect_b().spec(), "zh"}},
      "Verifying RestartOnA the fourth request sequence failed.");
}

// Browser tests verify same origin deprecation origin trial.
class SameOriginReduceAcceptLanguageDeprecationOTBrowserTest
    : public ReduceAcceptLanguageBrowserTest {
 public:
  void VerifySubrequest(
      const GURL& url,
      const std::string& last_request_path,
      const std::vector<std::string>& user_accept_languages,
      int expect_restart_count,
      int expect_fetch_count,
      const std::optional<std::string>& expect_opt_in_fq_language,
      const std::optional<std::string>& expect_opt_out_fq_language,
      const std::string& expect_reduced_accept_language) {
    base::HistogramTester histograms;

    // 1. Test opt-in deprecation origin trial.
    SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
    // Verify the first request opt-in deprecation origin trial.
    NavigateAndVerifyAcceptLanguageOfLastRequest(url,
                                                 expect_opt_in_fq_language);
    EXPECT_EQ(LastRequestUrl().path(), last_request_path);

    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    // Ensure restart happen once.
    histograms.ExpectBucketCount(
        "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
        /*=kNavigationRestarted=*/3, expect_restart_count);
    histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs",
                                expect_fetch_count);

    // Verify navigator.languages returns full list of accept-languages if
    // has valid deprecation origin trial token.
    VerifyNavigatorLanguages(user_accept_languages);

    // Verify the send request after opt-in deprecation origin trial.
    NavigateAndVerifyAcceptLanguageOfLastRequest(url, std::nullopt);

    // 2. Test opt-out deprecation origin trial.
    // Verify the first request has invalid deprecation origin trial token.
    SetOriginTrialFirstPartyToken(kInvalidOriginToken);
    NavigateAndVerifyAcceptLanguageOfLastRequest(url,
                                                 expect_opt_out_fq_language);
    EXPECT_EQ(LastRequestUrl().path(), last_request_path);
    VerifyNavigatorLanguages({user_accept_languages[0]});

    // Verify the second request has invalid deprecation origin trial token, it
    // should continue to reduce the Accept-Language.
    NavigateAndVerifyAcceptLanguageOfLastRequest(
        url, expect_reduced_accept_language);
    EXPECT_EQ(LastRequestUrl().path(), last_request_path);
  }

  void VerifySameOriginRequestNoRestart(
      const std::optional<std::string>& expect_accept_language,
      int expect_fetch_count,
      int expect_store_count) {
    base::HistogramTester histograms;
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
    histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency",
                                expect_store_count);
  }

  void VerifySameOriginTwoRequestsAfterTokenInvalid(
      const std::optional<std::string>& expect_accept_language) {
    SetOriginTrialFirstPartyToken(kInvalidOriginToken);
    base::HistogramTester histograms;
    // First request after token invalid will continue not sending reduced
    // header since we can't verify the response header before preparing the
    // request headers.
    NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                                 std::nullopt);
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    // For opt-out deprecation origin trial, there is no clear cache operation.
    histograms.ExpectTotalCount("ReduceAcceptLanguage.ClearLatency", 0);

    // The second request with invalid token should continue to reduced
    // Accept-Language header.
    NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                                 expect_accept_language);
  }

 protected:
  void EnabledFeatures() override {
    // Explicit enable feature ReduceAcceptLanguage.
    scoped_feature_list_.InitAndEnableFeature(
        {network::features::kReduceAcceptLanguage});
  }
};

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       FirstRequestMatchPrimaryLanguage) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});
  SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
  SetPrefsAcceptLanguage({"es", "zh"});

  // The first request will add the reduced Accept-Language in navigation
  // request since we enabled the ReduceAcceptLanguage.
  // One fetch for initially checking whether need to add reduce Accept-Language
  // header. No call to store the language since we don't store the reduced
  // language which is the first primary language.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/"es",
                                   /*expect_fetch_count=*/1,
                                   /*expect_store_count=*/0);
  // The second request should not send out reduced Accept-Language, the network
  // layer will add the full Accept-Language list.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/std::nullopt,
                                   /*expect_fetch_count=*/0,
                                   /*expect_store_count=*/0);
  // Verify requests after invalid token will continue send the reduced
  // accept-language.
  VerifySameOriginTwoRequestsAfterTokenInvalid("es");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       FirstRequestMatchNonPrimaryLanguage) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "en-US, es;d",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});
  SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
  SetPrefsAcceptLanguage({"zh", "en-US"});

  // First request restarts and send Accept-Language with negotiated language:
  // en-us, the deprecation origin trial doesn't apply to the first request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               "en-US,en;q=0.9");
  // Ensure only restart once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);

  // Two fetches for initially adding header and restart fetch.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 2);
  // Expect no perf storage updates.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  // The second request shouldn't send out any reduced accept-language due to
  // opted-in the deprecation origin trial.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/std::nullopt,
                                   /*expect_fetch_count=*/0,
                                   /*expect_store_count=*/0);
  // Verify requests after invalid token will continue send the reduced
  // accept-language.
  VerifySameOriginTwoRequestsAfterTokenInvalid("en-US,en;q=0.9");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       FirstRequestMatchNonPrimaryLanguageWithCriticalTrial) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "en-US, es;d",
                  .vary_in_parent = "accept-language",
                  .is_critical_origin_trial = true},
                 {SameOriginRequestUrl()});
  SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
  SetPrefsAcceptLanguage({"zh", "en-US"});

  // First request restarts and won't send reduced Accept-Language header due to
  // critical deprecation origin trial apply to the first request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(SameOriginRequestUrl(),
                                               std::nullopt);
  // Ensure only restart once.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);

  // The second request shouldn't send out any reduced accept-language due to
  // opted-in the deprecation origin trial.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/std::nullopt,
                                   /*expect_fetch_count=*/0,
                                   /*expect_store_count=*/0);
  // Verify requests after invalid token will continue to send the reduced
  // accept-language.
  VerifySameOriginTwoRequestsAfterTokenInvalid("en-US,en;q=0.9");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       FirstRequestNoMatchLanguage) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {SameOriginRequestUrl()});
  SetOriginTrialFirstPartyToken(kValidFirstPartyToken);
  SetPrefsAcceptLanguage({"zh", "ja"});

  // The first request adds the reduced Accept-Language in navigation request
  // since we enabled ReduceAcceptLanguage feature.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/"zh",
                                   /*expect_fetch_count=*/1,
                                   /*expect_store_count=*/0);
  // The second request should not send out reduced Accept-Language, the network
  // layer will add the full Accept-Language list.
  VerifySameOriginRequestNoRestart(/*expect_accept_language=*/std::nullopt,
                                   /*expect_fetch_count=*/0,
                                   /*expect_store_count=*/0);
  // Verify requests after invalid token will continue send the reduced
  // accept-language.
  VerifySameOriginTwoRequestsAfterTokenInvalid("zh");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       IframeRequestRestart) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language",
                  .is_critical_origin_trial = false},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});
  std::vector<std::string> user_accept_languages = {"zh", "en-US"};
  SetPrefsAcceptLanguage(user_accept_languages);

  // Total two different url requests:
  // * same_origin_iframe_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * simple_request_url: no fetch due to opted-in deprecation origin trial.
  //
  // For the first iframe request after site opt-in the deprecation OT, we
  // won't add the Accept-Language since language negotiation restart the
  // request and find a valid deprecation origin trial token. Network layer
  // will add the full list of user's Accept-Language.
  // For the first iframe request after site opt-out (or invalid token) the
  // deprecation OT, we resume to reduce the Accept-Language, however, the
  // reduced accept-language is the first user accept-language because the
  // negotiated language won't take effect for the first request after opt-out
  // the deprecation trial without the critical trial.
  VerifySubrequest(
      /*url=*/SameOriginIframeUrl(),
      /*last_request_path=*/"/subframe_simple.html",
      /*user_accept_languages=*/user_accept_languages,
      /*expect_restart_count=*/1,
      /*expect_fetch_count=*/2,
      /*expect_opt_in_fq_language=*/std::nullopt,
      /*expect_opt_out_fq_language=*/"zh",
      /*expect_reduced_accept_language=*/"en-US,en;q=0.9");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       IframeRequestRestartWithCriticalTrial) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language",
                  .is_critical_origin_trial = true},
                 {SameOriginIframeUrl(), SimpleRequestUrl()});
  std::vector<std::string> user_accept_languages = {"zh", "en-US"};
  SetPrefsAcceptLanguage(user_accept_languages);

  // Only one fetch reduced Accept-Language prefs for initially add the
  // Accept-Language HTTP header, no fetch call for restart request since we
  // added the deprecation trial as critical trial. Also, we add the critical
  // trial, the negotiated language should take effect for the first request
  // after opt-out the deprecation trial.
  VerifySubrequest(
      /*url=*/SameOriginIframeUrl(),
      /*last_request_path=*/"/subframe_simple.html",
      /*user_accept_languages=*/user_accept_languages,
      /*expect_restart_count=*/1,
      /*expect_fetch_count=*/1,
      /*expect_opt_in_fq_language=*/std::nullopt,
      /*expect_opt_out_fq_language=*/"en-US,en;q=0.9",
      /*expect_reduced_accept_language=*/"en-US,en;q=0.9");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       IframeRequestNoRestart) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .is_critical_origin_trial = false},
                 {{SameOriginIframeUrl(), SimpleRequestUrl()}});
  std::vector<std::string> user_accept_languages = {"es", "ja"};
  SetPrefsAcceptLanguage(user_accept_languages);

  // No restart
  VerifySubrequest(
      /*url=*/SameOriginIframeUrl(),
      /*last_request_path=*/"/subframe_simple.html",
      /*user_accept_languages=*/user_accept_languages,
      /*expect_restart_count=*/0,
      /*expect_fetch_count=*/1,
      /*expect_opt_in_fq_language=*/std::nullopt,
      /*expect_opt_out_fq_language=*/"es",
      /*expect_reduced_accept_language=*/"es");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       IframeRequestNoRestartWithCriticalTrial) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .is_critical_origin_trial = true},
                 {{SameOriginIframeUrl(), SimpleRequestUrl()}});
  std::vector<std::string> user_accept_languages = {"es", "ja"};
  SetPrefsAcceptLanguage(user_accept_languages);

  // No restart
  VerifySubrequest(
      /*url=*/SameOriginIframeUrl(),
      /*last_request_path=*/"/subframe_simple.html",
      /*user_accept_languages=*/user_accept_languages,
      /*expect_restart_count=*/0,
      /*expect_fetch_count=*/1,
      /*expect_opt_in_fq_language=*/std::nullopt,
      /*expect_opt_out_fq_language=*/"es",
      /*expect_reduced_accept_language=*/"es");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       ImgSubresourceRestart) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language",
                  .is_critical_origin_trial = false},
                 {SameOriginImgUrl(), SimpleImgUrl()});
  std::vector<std::string> user_accept_languages = {"zh", "en-US"};
  SetPrefsAcceptLanguage(user_accept_languages);

  // Total two different url requests:
  // * same_origin_image_url: one fetch for initially adding header and another
  // one for the restart request adding header.
  // * subresource_simple.jpg: no language fetch on subresource.
  //
  // For the first subresource request after site opt-in the deprecation OT.
  // We won't add the Accept-Language since language negotiation restart the
  // request and find a valid deprecation origin trial token. Network layer
  // will add the full list of user's Accept-Language.
  // For the first subresource request after site opt-out (or invalid token)
  // the deprecation OT, we also won't add the Accept-Language since we cleared
  // the commit language for subresource once we see invalid OT token when
  // committing the navigation.
  VerifySubrequest(
      /*url=*/SameOriginImgUrl(),
      /*last_request_path=*/"/subresource_simple.jpg",
      /*user_accept_languages=*/user_accept_languages,
      /*expect_restart_count=*/1,
      /*expect_fetch_count=*/2,
      /*expect_opt_in_fq_language=*/std::nullopt,
      /*expect_opt_out_fq_language=*/std::nullopt,
      /*expect_reduced_accept_language=*/"en-US,en;q=0.9");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       ImgSubresourceRestartWithCriticalTrial) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "es",
                  .avail_language_in_child = "es, en-US",
                  .vary_in_child = "accept-language",
                  .is_critical_origin_trial = true},
                 {SameOriginImgUrl(), SimpleImgUrl()});
  std::vector<std::string> user_accept_languages = {"zh", "en-US"};
  SetPrefsAcceptLanguage(user_accept_languages);

  // Only one fetch call for initially add the reduced Accept-Language header,
  // no fetch call for restart request since we added the deprecation trial as
  // critical trial.
  VerifySubrequest(
      /*url=*/SameOriginImgUrl(),
      /*last_request_path=*/"/subresource_simple.jpg",
      /*user_accept_languages=*/user_accept_languages,
      /*expect_restart_count=*/1,
      /*expect_fetch_count=*/1,
      /*expect_opt_in_fq_language=*/std::nullopt,
      /*expect_opt_out_fq_language=*/std::nullopt,
      /*expect_reduced_accept_language=*/"en-US,en;q=0.9");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       ImgSubresourceNoRestart) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language"},
                 {{SameOriginImgUrl(), SimpleImgUrl()}});
  std::vector<std::string> user_accept_languages = {"es", "ja"};
  SetPrefsAcceptLanguage(user_accept_languages);

  // No restart
  VerifySubrequest(
      /*url=*/SameOriginImgUrl(),
      /*last_request_path=*/"/subresource_simple.jpg",
      /*user_accept_languages=*/user_accept_languages,
      /*expect_restart_count=*/0,
      /*expect_fetch_count=*/1,
      /*expect_opt_in_fq_language=*/std::nullopt,
      /*expect_opt_out_fq_language=*/std::nullopt,
      /*expect_reduced_accept_language=*/"es");
}

IN_PROC_BROWSER_TEST_F(SameOriginReduceAcceptLanguageDeprecationOTBrowserTest,
                       ImgSubresourceNoRestartWithCriticalTrial) {
  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .is_critical_origin_trial = true},
                 {{SameOriginImgUrl(), SimpleImgUrl()}});
  std::vector<std::string> user_accept_languages = {"es", "ja"};
  SetPrefsAcceptLanguage(user_accept_languages);

  // No restart
  VerifySubrequest(
      /*url=*/SameOriginImgUrl(),
      /*last_request_path=*/"/subresource_simple.jpg",
      /*user_accept_languages=*/user_accept_languages,
      /*expect_restart_count=*/0,
      /*expect_fetch_count=*/1,
      /*expect_opt_in_fq_language=*/std::nullopt,
      /*expect_opt_out_fq_language=*/std::nullopt,
      /*expect_reduced_accept_language=*/"es");
}

// Browser tests verify third party deprecation origin trial.
class ThirdPartyReduceAcceptLanguageDeprecationOTBrowserTest
    : public ThirdPartyReduceAcceptLanguageBrowserTest {
 protected:
  void EnabledFeatures() override {
    // Explicit enable feature ReduceAcceptLanguage ReduceAcceptLanguage.
    scoped_feature_list_.InitWithFeatures(
        {network::features::kReduceAcceptLanguage}, {});
  }
};

// For third-party embedded as an iframe, the third-party can opt-in the
// deprecation trial as a first party deprecation origin trial.
IN_PROC_BROWSER_TEST_F(ThirdPartyReduceAcceptLanguageDeprecationOTBrowserTest,
                       IframeRequests) {
  base::HistogramTester histograms;

  SetTestOptions({.content_language_in_parent = "es",
                  .avail_language_in_parent = "es, en-US",
                  .vary_in_parent = "accept-language",
                  .content_language_in_child = "zh",
                  .avail_language_in_child = "zh",
                  .vary_in_child = "accept-language"},
                 {CrossOriginIframeUrl(), SimpleThirdPartyRequestUrl()});

  SetOriginTrialThirdPartyToken(kValidMySiteFirstPartyToken);
  SetPrefsAcceptLanguage({"zh", "en-US"});

  // The first third-party iframe subrequest expect continue to send reduced
  // Accept-Language which is inherited from the top-level frame request.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CrossOriginIframeUrl(),
                                               "en-US,en;q=0.9");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure only one restart to do the language negotiation.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 1);
  // cross_origin_iframe_url: one fetch for initially adding header and
  // another one for the restart request adding header.
  // simple_3p_request_url: one fetch for initially adding header.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 3);
  // Persist reduce accept language happens.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);

  EXPECT_EQ(LastRequestUrl().path(), "/subframe_simple_3p.html");

  // For the second request, we expect no reduced Accept-Language send once
  // the deprecation origin trial takes effect.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CrossOriginIframeUrl(),
                                               std::nullopt);

  // For the first request we continue to send the reduced Accept-Language since
  // we persist the deprecation origin trial token on a third-party context
  // which means the partition origin is first part origin.
  // For the second request, we won't send reduce Accept-Language as deprecation
  // origin trial take effects, in this case, the partition origin is the
  // third-party origin itself.
  base::HistogramTester histograms2;
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  NavigateAndVerifyAcceptLanguageOfLastRequest(SimpleThirdPartyRequestUrl(),
                                               "zh");
  NavigateAndVerifyAcceptLanguageOfLastRequest(SimpleThirdPartyRequestUrl(),
                                               std::nullopt);
  histograms2.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyReduceAcceptLanguageDeprecationOTBrowserTest,
                       JavaScriptRequest) {
  base::HistogramTester histograms;

  SetTestOptions(
      {.content_language_in_parent = "es",
       .avail_language_in_parent = "es, en-US",
       .vary_in_parent = "accept-language",
       .content_language_in_child = "zh",
       .avail_language_in_child = "zh",
       .vary_in_child = "accept-language"},
      {CrossOriginSubresourceUrl(), CrossOriginMetaTagInjectingJavascriptUrl(),
       CrossOriginCssRequestUrl()});

  SetOriginTrialThirdPartyToken(kValidThirdPartyToken);
  SetPrefsAcceptLanguage({"zh", "en-US"});

  // Third party iframe subrequest expect inherit the reduced Accept-Language
  // from the top-level navigation requests.
  NavigateAndVerifyAcceptLanguageOfLastRequest(CrossOriginSubresourceUrl(),
                                               "zh");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Ensure no restart happen.
  histograms.ExpectBucketCount(
      "ReduceAcceptLanguage.AcceptLanguageNegotiationRestart",
      /*=kNavigationRestarted=*/3, 0);
  // One fetch for initially checking whether need to add reduce Accept-Language
  // header to the top-level frame.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.FetchLatencyUs", 1);
  // No persist reduce accept language happens.
  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 0);

  // All subresources should have been loaded,
  EXPECT_THAT(intercepted_load_urls_,
              testing::IsSupersetOf({CrossOriginMetaTagInjectingJavascriptUrl(),
                                     CrossOriginCssRequestUrl()}));

  // Ensure third-party JavaScript access JS getters get the full list of
  // accept-language.
  VerifyNavigatorLanguages({"zh", "en-US"});
}
