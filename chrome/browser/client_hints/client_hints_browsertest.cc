// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cctype>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/nqe/effective_connection_type.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace {

const unsigned expected_client_hints_number = 12u;
const int32_t uma_histogram_max_value = 1471228928;

// An interceptor that records count of fetches and client hint headers for
// requests to https://foo.com/non-existing-image.jpg.
class ThirdPartyURLLoaderInterceptor {
 public:
  explicit ThirdPartyURLLoaderInterceptor(const GURL intercepted_url)
      : intercepted_url_(intercepted_url),
        interceptor_(base::BindRepeating(
            &ThirdPartyURLLoaderInterceptor::InterceptURLRequest,
            base::Unretained(this))) {}

  ~ThirdPartyURLLoaderInterceptor() = default;

  size_t request_count_seen() const { return request_count_seen_; }

  size_t client_hints_count_seen() const { return client_hints_count_seen_; }

 private:
  bool InterceptURLRequest(
      content::URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url != intercepted_url_)
      return false;

    request_count_seen_++;
    for (size_t i = 0; i < blink::kClientHintsMappingsCount; ++i) {
      if (params->url_request.headers.HasHeader(
              blink::kClientHintsHeaderMapping[i])) {
        client_hints_count_seen_++;
      }
    }
    return false;
  }

  GURL intercepted_url_;

  size_t request_count_seen_ = 0u;

  size_t client_hints_count_seen_ = 0u;

  content::URLLoaderInterceptor interceptor_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyURLLoaderInterceptor);
};

// Returns true only if |header_value| satisfies ABNF: 1*DIGIT [ "." 1*DIGIT ]
bool IsSimilarToDoubleABNF(const std::string& header_value) {
  if (header_value.empty())
    return false;
  char first_char = header_value.at(0);
  if (!isdigit(first_char))
    return false;

  bool period_found = false;
  bool digit_found_after_period = false;
  for (char ch : header_value) {
    if (isdigit(ch)) {
      if (period_found) {
        digit_found_after_period = true;
      }
      continue;
    }
    if (ch == '.') {
      if (period_found)
        return false;
      period_found = true;
      continue;
    }
    return false;
  }
  if (period_found)
    return digit_found_after_period;
  return true;
}

// Returns true only if |header_value| satisfies ABNF: 1*DIGIT
bool IsSimilarToIntABNF(const std::string& header_value) {
  if (header_value.empty())
    return false;

  for (char ch : header_value) {
    if (!isdigit(ch))
      return false;
  }
  return true;
}

}  // namespace

class ClientHintsBrowserTest : public policy::PolicyTest,
                               public testing::WithParamInterface<bool> {
 public:
  ClientHintsBrowserTest()
      : http_server_(net::EmbeddedTestServer::TYPE_HTTP),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_cross_origin_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        expect_client_hints_on_main_frame_(false),
        expect_client_hints_on_subresources_(false),
        count_user_agent_hint_headers_seen_(0),
        count_ua_mobile_client_hints_headers_seen_(0),
        count_client_hints_headers_seen_(0),
        request_interceptor_(nullptr) {
    http_server_.ServeFilesFromSourceDirectory("chrome/test/data/client_hints");
    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");
    https_cross_origin_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");

    http_server_.RegisterRequestMonitor(
        base::BindRepeating(&ClientHintsBrowserTest::MonitorResourceRequest,
                            base::Unretained(this)));
    https_server_.RegisterRequestMonitor(
        base::BindRepeating(&ClientHintsBrowserTest::MonitorResourceRequest,
                            base::Unretained(this)));
    https_cross_origin_server_.RegisterRequestMonitor(
        base::BindRepeating(&ClientHintsBrowserTest::MonitorResourceRequest,
                            base::Unretained(this)));
    https_cross_origin_server_.RegisterRequestHandler(
        base::BindRepeating(&ClientHintsBrowserTest::RequestHandlerToRedirect,
                            base::Unretained(this)));
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &ClientHintsBrowserTest::RequestHandlerToFetchCrossOriginIframe,
        base::Unretained(this)));

    EXPECT_TRUE(http_server_.Start());
    EXPECT_TRUE(https_server_.Start());
    EXPECT_TRUE(https_cross_origin_server_.Start());

    EXPECT_NE(https_server_.base_url(), https_cross_origin_server_.base_url());

    accept_ch_with_lifetime_http_local_url_ =
        http_server_.GetURL("/accept_ch_with_lifetime.html");
    http_equiv_accept_ch_with_lifetime_http_local_url_ =
        http_server_.GetURL("/http_equiv_accept_ch_with_lifetime.html");
    EXPECT_TRUE(accept_ch_with_lifetime_http_local_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_FALSE(
        accept_ch_with_lifetime_http_local_url_.SchemeIsCryptographic());

    accept_ch_with_lifetime_url_ =
        https_server_.GetURL("/accept_ch_with_lifetime.html");
    accept_ch_with_short_lifetime_url_ =
        https_server_.GetURL("/accept_ch_with_short_lifetime.html");

    accept_ch_without_lifetime_url_ =
        https_server_.GetURL("/accept_ch_without_lifetime.html");
    EXPECT_TRUE(accept_ch_with_lifetime_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_TRUE(accept_ch_with_lifetime_url_.SchemeIsCryptographic());
    http_equiv_accept_ch_without_lifetime_url_ =
        https_server_.GetURL("/http_equiv_accept_ch_without_lifetime.html");

    without_accept_ch_without_lifetime_url_ =
        https_server_.GetURL("/without_accept_ch_without_lifetime.html");
    EXPECT_TRUE(without_accept_ch_without_lifetime_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_TRUE(
        without_accept_ch_without_lifetime_url_.SchemeIsCryptographic());

    without_accept_ch_without_lifetime_local_url_ =
        http_server_.GetURL("/without_accept_ch_without_lifetime.html");
    EXPECT_TRUE(
        without_accept_ch_without_lifetime_local_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_FALSE(
        without_accept_ch_without_lifetime_local_url_.SchemeIsCryptographic());

    without_accept_ch_without_lifetime_img_localhost_ = https_server_.GetURL(
        "/without_accept_ch_without_lifetime_img_localhost.html");
    without_accept_ch_without_lifetime_img_foo_com_ = https_server_.GetURL(
        "/without_accept_ch_without_lifetime_img_foo_com.html");
    accept_ch_without_lifetime_with_iframe_url_ =
        https_server_.GetURL("/accept_ch_without_lifetime_with_iframe.html");
    http_equiv_accept_ch_without_lifetime_with_iframe_url_ =
        https_server_.GetURL(
            "/http_equiv_accept_ch_without_lifetime_with_iframe.html");
    accept_ch_without_lifetime_with_subresource_url_ = https_server_.GetURL(
        "/accept_ch_without_lifetime_with_subresource.html");
    http_equiv_accept_ch_without_lifetime_with_subresource_url_ =
        https_server_.GetURL(
            "/http_equiv_accept_ch_without_lifetime_with_subresource.html");
    accept_ch_without_lifetime_with_subresource_iframe_url_ =
        https_server_.GetURL(
            "/accept_ch_without_lifetime_with_subresource_iframe.html");
    http_equiv_accept_ch_without_lifetime_with_subresource_iframe_url_ =
        https_server_.GetURL(
            "/http_equiv_accept_ch_without_lifetime_with_subresource_iframe."
            "html");
    accept_ch_without_lifetime_img_localhost_ =
        https_server_.GetURL("/accept_ch_without_lifetime_img_localhost.html");
    http_equiv_accept_ch_without_lifetime_img_localhost_ = https_server_.GetURL(
        "/http_equiv_accept_ch_without_lifetime_img_localhost.html");
    http_equiv_accept_ch_with_lifetime_ =
        https_server_.GetURL("/http_equiv_accept_ch_with_lifetime.html");

    redirect_url_ = https_cross_origin_server_.GetURL("/redirect.html");

    accept_ch_empty_ = https_server_.GetURL("/accept_ch_empty.html");
    http_equiv_accept_ch_merge_ =
        https_server_.GetURL("/http_equiv_accept_ch_merge.html");
  }

  ~ClientHintsBrowserTest() override {}

  virtual std::unique_ptr<base::FeatureList> EnabledFeatures() {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(
        "UserAgentClientHint,LangClientHintHeader", "");
    return feature_list;
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureList(EnabledFeatures());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    request_interceptor_ = std::make_unique<ThirdPartyURLLoaderInterceptor>(
        GURL("https://foo.com/non-existing-image.jpg"));
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override { request_interceptor_.reset(); }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII(network::switches::kForceEffectiveConnectionType,
                           net::kEffectiveConnectionType2G);
  }

  void SetClientHintExpectationsOnMainFrame(bool expect_client_hints) {
    expect_client_hints_on_main_frame_ = expect_client_hints;
  }

  void SetClientHintExpectationsOnSubresources(bool expect_client_hints) {
    base::AutoLock lock(expect_client_hints_on_subresources_lock_);
    expect_client_hints_on_subresources_ = expect_client_hints;
  }

  bool expect_client_hints_on_subresources() {
    base::AutoLock lock(expect_client_hints_on_subresources_lock_);
    return expect_client_hints_on_subresources_;
  }

  // Verify that the user is not notified that cookies or JavaScript were
  // blocked on the webpage due to the checks done by client hints.
  void VerifyContentSettingsNotNotified() const {
    auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
    EXPECT_FALSE(pscs->IsContentBlocked(ContentSettingsType::COOKIES));
    EXPECT_FALSE(pscs->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  }

  void SetExpectedEffectiveConnectionType(
      net::EffectiveConnectionType effective_connection_type) {
    expected_ect = effective_connection_type;
  }

  void SetJsEnabledForActiveView(bool enabled) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    blink::web_pref::WebPreferences prefs =
        web_contents->GetOrCreateWebPreferences();
    prefs.javascript_enabled = enabled;
    web_contents->SetWebPreferences(prefs);
  }

  void TestProfilesIndependent(Browser* browser_a, Browser* browser_b);

  const GURL& accept_ch_with_lifetime_http_local_url() const {
    return accept_ch_with_lifetime_http_local_url_;
  }
  const GURL& http_equiv_accept_ch_with_lifetime_http_local_url() const {
    return http_equiv_accept_ch_with_lifetime_http_local_url_;
  }

  // A URL whose response headers include Accept-CH and Accept-CH-Lifetime
  // headers.
  const GURL& accept_ch_with_lifetime_url() const {
    return accept_ch_with_lifetime_url_;
  }
  const GURL& http_equiv_accept_ch_with_lifetime() {
    return http_equiv_accept_ch_with_lifetime_;
  }

  // A URL whose response headers include Accept-CH and Accept-CH-Lifetime
  // headers. The Accept-CH-Lifetime duration is set very short to 1 second.
  const GURL& accept_ch_with_short_lifetime() const {
    return accept_ch_with_short_lifetime_url_;
  }

  // A URL whose response headers include only Accept-CH header.
  const GURL& accept_ch_without_lifetime_url() const {
    return accept_ch_without_lifetime_url_;
  }
  const GURL& http_equiv_accept_ch_without_lifetime_url() const {
    return http_equiv_accept_ch_without_lifetime_url_;
  }

  // A URL whose response headers do not include either Accept-CH or
  // Accept-CH-Lifetime headers. Navigating to this URL also fetches an image.
  const GURL& without_accept_ch_without_lifetime_url() const {
    return without_accept_ch_without_lifetime_url_;
  }

  // A URL whose response headers do not include either Accept-CH or
  // Accept-CH-Lifetime headers. Navigating to this URL also fetches an image.
  const GURL& without_accept_ch_without_lifetime_local_url() const {
    return without_accept_ch_without_lifetime_local_url_;
  }

  // A URL whose response headers do not include either Accept-CH or
  // Accept-CH-Lifetime headers. Navigating to this URL also fetches an image
  // from localhost.
  const GURL& without_accept_ch_without_lifetime_img_localhost() const {
    return without_accept_ch_without_lifetime_img_localhost_;
  }

  // A URL whose response headers do not include either Accept-CH or
  // Accept-CH-Lifetime headers. Navigating to this URL also fetches an image
  // from foo.com.
  const GURL& without_accept_ch_without_lifetime_img_foo_com() const {
    return without_accept_ch_without_lifetime_img_foo_com_;
  }

  // A URL whose response does not include Accept-CH or Accept-CH-Lifetime
  // headers. The response loads accept_ch_with_lifetime_url() in an iframe.
  const GURL& accept_ch_without_lifetime_with_iframe_url() const {
    return accept_ch_without_lifetime_with_iframe_url_;
  }
  const GURL& http_equiv_accept_ch_without_lifetime_with_iframe_url() const {
    return http_equiv_accept_ch_without_lifetime_with_iframe_url_;
  }

  // A URL whose response does not include Accept-CH or Accept-CH-Lifetime
  // headers. The response loads accept_ch_with_lifetime_url() as a subresource
  // in the main frame.
  const GURL& accept_ch_without_lifetime_with_subresource_url() const {
    return accept_ch_without_lifetime_with_subresource_url_;
  }
  const GURL& http_equiv_accept_ch_without_lifetime_with_subresource_url()
      const {
    return http_equiv_accept_ch_without_lifetime_with_subresource_url_;
  }

  // A URL whose response does not include Accept-CH or Accept-CH-Lifetime
  // headers. The response loads accept_ch_with_lifetime_url() or
  // http_equiv_accept_ch_with_lifetime_url() as a subresource in the iframe.
  const GURL& accept_ch_without_lifetime_with_subresource_iframe_url() const {
    return accept_ch_without_lifetime_with_subresource_iframe_url_;
  }
  const GURL&
  http_equiv_accept_ch_without_lifetime_with_subresource_iframe_url() const {
    return http_equiv_accept_ch_without_lifetime_with_subresource_iframe_url_;
  }

  // A URL whose response includes only Accept-CH header. Navigating to
  // this URL also fetches two images: One from the localhost, and one from
  // foo.com.
  const GURL& accept_ch_without_lifetime_img_localhost() const {
    return accept_ch_without_lifetime_img_localhost_;
  }
  const GURL& http_equiv_accept_ch_without_lifetime_img_localhost() const {
    return http_equiv_accept_ch_without_lifetime_img_localhost_;
  }

  const GURL& redirect_url() const { return redirect_url_; }

  // A URL to a page with a response containing an empty accept_ch header.
  const GURL& accept_ch_empty() const { return accept_ch_empty_; }

  // A page where some hints are in accept-ch header, some in http-equiv.
  const GURL& http_equiv_accept_ch_merge() const {
    return http_equiv_accept_ch_merge_;
  }

  size_t count_user_agent_hint_headers_seen() const {
    base::AutoLock lock(count_headers_lock_);
    return count_user_agent_hint_headers_seen_;
  }

  size_t count_ua_mobile_client_hints_headers_seen() const {
    base::AutoLock lock(count_headers_lock_);
    return count_ua_mobile_client_hints_headers_seen_;
  }

  size_t count_client_hints_headers_seen() const {
    base::AutoLock lock(count_headers_lock_);
    return count_client_hints_headers_seen_;
  }

  size_t third_party_request_count_seen() const {
    return request_interceptor_->request_count_seen();
  }

  size_t third_party_client_hints_count_seen() const {
    return request_interceptor_->client_hints_count_seen();
  }

  const std::string& main_frame_ua_observed() const {
    return main_frame_ua_observed_;
  }

  const std::string& main_frame_ua_full_version_observed() const {
    return main_frame_ua_full_version_observed_;
  }

  const std::string& main_frame_ua_mobile_observed() const {
    return main_frame_ua_mobile_observed_;
  }

  const std::string& main_frame_ua_platform_observed() const {
    return main_frame_ua_platform_observed_;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::string intercept_iframe_resource_;
  bool intercept_to_http_equiv_iframe_ = false;
  mutable base::Lock count_headers_lock_;

 private:
  // Intercepts only the main frame requests that contain
  // "redirect" in the resource path. The intercepted requests
  // are served an HTML file that fetches an iframe from a cross-origin HTTPS
  // server.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerToRedirect(
      const net::test_server::HttpRequest& request) {
    // Check if it's a main frame request.
    if (request.relative_url.find(".html") == std::string::npos)
      return nullptr;

    if (request.GetURL().spec().find("redirect") == std::string::npos)
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    response.reset(new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader("Location",
                              without_accept_ch_without_lifetime_url().spec());
    return std::move(response);
  }

  // Intercepts only the main frame requests that contain
  // |intercept_iframe_resource_| in the resource path. The intercepted requests
  // are served an HTML file that fetches an iframe from a cross-origin HTTPS
  // server.
  std::unique_ptr<net::test_server::HttpResponse>
  RequestHandlerToFetchCrossOriginIframe(
      const net::test_server::HttpRequest& request) {
    if (intercept_iframe_resource_.empty())
      return nullptr;

    // Check if it's a main frame request.
    if (request.relative_url.find(".html") == std::string::npos)
      return nullptr;

    if (request.relative_url.find(intercept_iframe_resource_) ==
        std::string::npos) {
      return nullptr;
    }

    const std::string iframe_url =
        intercept_to_http_equiv_iframe_
            ? https_cross_origin_server_
                  .GetURL("/http_equiv_accept_ch_with_lifetime.html")
                  .spec()
            : https_cross_origin_server_.GetURL("/accept_ch_with_lifetime.html")
                  .spec();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<html>"
        "<link rel='icon' href='data:;base64,='><head></head>"
        "Empty file which uses link-rel to disable favicon fetches. "
        "<iframe src='" +
        iframe_url + "'></iframe></html>");

    return std::move(http_response);
  }

  static std::string UpdateHeaderObservation(
      const net::test_server::HttpRequest& request,
      const std::string& header) {
    if (request.headers.find(header) != request.headers.end())
      return request.headers.find(header)->second;
    else
      return "";
  }

  // Called by |https_server_|.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    bool is_main_frame_navigation =
        request.GetURL().spec().find(".html") != std::string::npos;

    if (is_main_frame_navigation &&
        request.GetURL().spec().find("redirect") != std::string::npos) {
      return;
    }

    if (is_main_frame_navigation) {
      main_frame_ua_observed_ = UpdateHeaderObservation(request, "sec-ch-ua");
      main_frame_ua_full_version_observed_ =
          UpdateHeaderObservation(request, "sec-ch-ua-full-version");
      main_frame_ua_mobile_observed_ =
          UpdateHeaderObservation(request, "sec-ch-ua-mobile");
      main_frame_ua_platform_observed_ =
          UpdateHeaderObservation(request, "sec-ch-ua-platform");

      VerifyClientHintsReceived(expect_client_hints_on_main_frame_, request);
      if (expect_client_hints_on_main_frame_) {
        double value = 0.0;
        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("device-memory")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(
            request.headers.find("device-memory")->second));
        main_frame_device_memory_observed_ = value;

        EXPECT_TRUE(
            base::StringToDouble(request.headers.find("dpr")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(request.headers.find("dpr")->second));
        main_frame_dpr_observed_ = value;

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("viewport-width")->second, &value));
        EXPECT_TRUE(
            IsSimilarToIntABNF(request.headers.find("viewport-width")->second));
#if !defined(OS_ANDROID)
        EXPECT_LT(0.0, value);
#else
        EXPECT_EQ(980, value);
#endif
        main_frame_viewport_width_observed_ = value;

        VerifyNetworkQualityClientHints(request);
      }
    }

    if (!is_main_frame_navigation) {
      VerifyClientHintsReceived(expect_client_hints_on_subresources(), request);

      if (expect_client_hints_on_subresources()) {
        double value = 0.0;
        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("device-memory")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(
            request.headers.find("device-memory")->second));
        if (main_frame_device_memory_observed_ > 0) {
          EXPECT_EQ(main_frame_device_memory_observed_, value);
        }

        EXPECT_TRUE(
            base::StringToDouble(request.headers.find("dpr")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(request.headers.find("dpr")->second));
        if (main_frame_dpr_observed_ > 0) {
          EXPECT_EQ(main_frame_dpr_observed_, value);
        }

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("viewport-width")->second, &value));
        EXPECT_TRUE(
            IsSimilarToIntABNF(request.headers.find("viewport-width")->second));
#if !defined(OS_ANDROID)
        EXPECT_LT(0.0, value);
#else
        EXPECT_EQ(980, value);
#endif
#if defined(OS_ANDROID)
        // TODO(tbansal): https://crbug.com/825892: Viewport width on main
        // frame requests may be incorrect when the Chrome window is not
        // maximized.
        if (main_frame_viewport_width_observed_ > 0) {
          EXPECT_EQ(main_frame_viewport_width_observed_, value);
        }
#endif
        VerifyNetworkQualityClientHints(request);
      }
    }

    for (size_t i = 0; i < blink::kClientHintsMappingsCount; ++i) {
      if (base::Contains(request.headers,
                         blink::kClientHintsHeaderMapping[i])) {
        base::AutoLock lock(count_headers_lock_);
        // The user agent hint is special:
        if (std::string(blink::kClientHintsHeaderMapping[i]) == "sec-ch-ua") {
          count_user_agent_hint_headers_seen_++;
        } else if (std::string(blink::kClientHintsHeaderMapping[i]) ==
                   "sec-ch-ua-mobile") {
          count_ua_mobile_client_hints_headers_seen_++;
        } else {
          count_client_hints_headers_seen_++;
        }
      }
    }
  }

  void VerifyClientHintsReceived(bool expect_client_hints,
                                 const net::test_server::HttpRequest& request) {
    for (size_t i = 0; i < blink::kClientHintsMappingsCount; ++i) {
      SCOPED_TRACE(testing::Message()
                   << std::string(blink::kClientHintsHeaderMapping[i]));
      // Resource width client hint is only attached on image subresources.
      if (std::string(blink::kClientHintsHeaderMapping[i]) == "width") {
        continue;
      }

      // `Sec-CH-UA` and `Sec-CH-UA-Mobile` is attached on all requests.
      if (std::string(blink::kClientHintsHeaderMapping[i]) == "sec-ch-ua" ||
          std::string(blink::kClientHintsHeaderMapping[i]) ==
              "sec-ch-ua-mobile") {
        continue;
      }

      EXPECT_EQ(
          expect_client_hints,
          base::Contains(request.headers, blink::kClientHintsHeaderMapping[i]));
    }
  }

  void VerifyNetworkQualityClientHints(
      const net::test_server::HttpRequest& request) const {
    // Effective connection type is forced to 2G using command line in these
    // tests.
    int rtt_value = 0.0;
    EXPECT_TRUE(
        base::StringToInt(request.headers.find("rtt")->second, &rtt_value));
    EXPECT_LE(0, rtt_value);
    EXPECT_TRUE(IsSimilarToIntABNF(request.headers.find("rtt")->second));
    // Verify that RTT value is a multiple of 50 milliseconds.
    EXPECT_EQ(0, rtt_value % 50);
    EXPECT_GE(expected_ect == net::EFFECTIVE_CONNECTION_TYPE_2G ? 3000 : 500,
              rtt_value);

    double mbps_value = 0.0;
    EXPECT_TRUE(base::StringToDouble(request.headers.find("downlink")->second,
                                     &mbps_value));
    EXPECT_LE(0, mbps_value);
    EXPECT_TRUE(
        IsSimilarToDoubleABNF(request.headers.find("downlink")->second));
    // Verify that the mbps value is a multiple of 0.050 mbps.
    // Allow for small amount of noise due to double to integer conversions.
    EXPECT_NEAR(0, (static_cast<int>(mbps_value * 1000)) % 50, 1);
    EXPECT_GE(10.0, mbps_value);

    EXPECT_FALSE(request.headers.find("ect")->second.empty());

    // TODO(tbansal): https://crbug.com/819244: When network servicification is
    // enabled, the renderer processes do not receive notifications on
    // change in the network quality. Hence, the network quality client hints
    // are not set to the correct value on subresources.
    bool is_main_frame_navigation =
        request.GetURL().spec().find(".html") != std::string::npos;
    if (is_main_frame_navigation) {
      // Effective connection type is forced to 2G using command line in these
      // tests. RTT is expected to be 1800 msec but leave some gap to account
      // for added noise and randomization.
      if (expected_ect == net::EFFECTIVE_CONNECTION_TYPE_2G) {
        EXPECT_NEAR(1800, rtt_value, 360);
      } else if (expected_ect == net::EFFECTIVE_CONNECTION_TYPE_3G) {
        EXPECT_NEAR(450, rtt_value, 90);
      } else {
        NOTREACHED();
      }

      // Effective connection type is forced to 2G using command line in these
      // tests. downlink is expected to be 0.075 Mbps but leave some gap to
      // account for added noise and randomization.
      if (expected_ect == net::EFFECTIVE_CONNECTION_TYPE_2G) {
        EXPECT_NEAR(0.075, mbps_value, 0.05);
      } else if (expected_ect == net::EFFECTIVE_CONNECTION_TYPE_3G) {
        EXPECT_NEAR(0.4, mbps_value, 0.1);
      } else {
        NOTREACHED();
      }

      EXPECT_EQ(expected_ect == net::EFFECTIVE_CONNECTION_TYPE_2G ? "2g" : "3g",
                request.headers.find("ect")->second);
    }
  }

  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer https_cross_origin_server_;
  GURL accept_ch_with_lifetime_http_local_url_;
  GURL http_equiv_accept_ch_with_lifetime_http_local_url_;
  GURL accept_ch_with_lifetime_url_;
  GURL accept_ch_with_short_lifetime_url_;
  GURL accept_ch_without_lifetime_url_;
  GURL http_equiv_accept_ch_without_lifetime_url_;
  GURL without_accept_ch_without_lifetime_url_;
  GURL without_accept_ch_without_lifetime_local_url_;
  GURL accept_ch_without_lifetime_with_iframe_url_;
  GURL http_equiv_accept_ch_without_lifetime_with_iframe_url_;
  GURL accept_ch_without_lifetime_with_subresource_url_;
  GURL http_equiv_accept_ch_without_lifetime_with_subresource_url_;
  GURL accept_ch_without_lifetime_with_subresource_iframe_url_;
  GURL http_equiv_accept_ch_without_lifetime_with_subresource_iframe_url_;
  GURL without_accept_ch_without_lifetime_img_foo_com_;
  GURL without_accept_ch_without_lifetime_img_localhost_;
  GURL accept_ch_without_lifetime_img_localhost_;
  GURL http_equiv_accept_ch_without_lifetime_img_localhost_;
  GURL http_equiv_accept_ch_with_lifetime_;
  GURL redirect_url_;
  GURL accept_ch_empty_;
  GURL http_equiv_accept_ch_merge_;

  std::string main_frame_ua_observed_;
  std::string main_frame_ua_full_version_observed_;
  std::string main_frame_ua_mobile_observed_;
  std::string main_frame_ua_platform_observed_;

  double main_frame_dpr_observed_ = -1;
  double main_frame_viewport_width_observed_ = -1;
  double main_frame_device_memory_observed_ = -1;

  // Expect client hints on all the main frame request.
  bool expect_client_hints_on_main_frame_;
  // Expect client hints on all the subresource requests.
  bool expect_client_hints_on_subresources_
      GUARDED_BY(expect_client_hints_on_subresources_lock_);

  base::Lock expect_client_hints_on_subresources_lock_;

  size_t count_user_agent_hint_headers_seen_;
  size_t count_ua_mobile_client_hints_headers_seen_;
  size_t count_client_hints_headers_seen_;

  std::unique_ptr<ThirdPartyURLLoaderInterceptor> request_interceptor_;

  // Set to 2G in SetUpCommandLine().
  net::EffectiveConnectionType expected_ect = net::EFFECTIVE_CONNECTION_TYPE_2G;

  DISALLOW_COPY_AND_ASSIGN(ClientHintsBrowserTest);
};

// True if testing for http-equiv correctness. When set to true, the tests
// use webpages that may contain http-equiv Accept-CH and Accept-CH-Lifetime
// headers. When set to false, the tests use webpages that set the headers in
// the HTTP response headers.
INSTANTIATE_TEST_SUITE_P(All,
                         ClientHintsBrowserTest,
                         testing::Bool());

class ClientHintsAllowThirdPartyBrowserTest : public ClientHintsBrowserTest {
  std::unique_ptr<base::FeatureList> EnabledFeatures() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(
        "AllowClientHintsToThirdParty,UserAgentClientHint,LangClientHintHeader",
        "");
    return feature_list;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClientHintsAllowThirdPartyBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, CorsChecks) {
  for (size_t i = 0; i < blink::kClientHintsMappingsCount; ++i) {
    // Do not test for headers that have not been enabled on the blink "stable"
    // yet.
    if (std::string(blink::kClientHintsHeaderMapping[i]) == "rtt" ||
        std::string(blink::kClientHintsHeaderMapping[i]) == "downlink" ||
        std::string(blink::kClientHintsHeaderMapping[i]) == "ect") {
      continue;
    }
    EXPECT_TRUE(network::cors::IsCorsSafelistedHeader(
        blink::kClientHintsHeaderMapping[i], "42" /* value */));
  }
  EXPECT_FALSE(network::cors::IsCorsSafelistedHeader("not-a-client-hint-header",
                                                     "" /* value */));
  EXPECT_TRUE(
      network::cors::IsCorsSafelistedHeader("save-data", "on" /* value */));
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, HttpEquivWorks) {
  const GURL gurl = http_equiv_accept_ch_without_lifetime_img_localhost();
  base::HistogramTester histogram_tester;

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);

  ui_test_utils::NavigateToURL(browser(), gurl);
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);
}

// Loads a webpage that requests persisting of client hints. Verifies that
// the browser receives the mojo notification from the renderer and persists the
// client hints to the disk --- unless it's using http-equiv which shouldn't
// persist.
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest, ClientHintsHttps) {
  base::HistogramTester histogram_tester;
  const GURL gurl = GetParam() ? http_equiv_accept_ch_with_lifetime()
                               : accept_ch_with_lifetime_url();
  ui_test_utils::NavigateToURL(browser(), gurl);

  if (GetParam())
    histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);
  else
    histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  if (GetParam()) {
    histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
    histogram_tester.ExpectTotalCount("ClientHints.PersistDuration", 0);
  } else {
    // client_hints_url() sets the expected number of client hints.
    histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                        expected_client_hints_number, 1);
    // accept_ch_with_lifetime_url() sets client hints persist duration to 3600
    // seconds, but a maximum value is registered instead.
    histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                        uma_histogram_max_value, 1);
  }
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, PRE_ClientHintsClearSession) {
  const GURL gurl = accept_ch_with_lifetime_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching |gurl| should persist the request for client hints iff using
  // headers and not http-equiv.
  ui_test_utils::NavigateToURL(browser(), gurl);

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  // accept_ch_with_lifetime_url() sets client hints persist duration to 3600
  // seconds, but a maximum value is registered instead.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      uma_histogram_max_value, 1);

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, ClientHintsClearSession) {
  const GURL gurl = accept_ch_with_lifetime_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(0u, count_client_hints_headers_seen());
}

// Test that client hints are attached to subresources only if they belong
// to the same host as document host.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsHttpsSubresourceDifferentOrigin) {
  const GURL gurl = accept_ch_with_lifetime_url();

  base::HistogramTester histogram_tester;

  // Add client hints for the embedded test server.
  ui_test_utils::NavigateToURL(browser(), gurl);
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  // Verify that the client hints settings for localhost have been saved.
  ContentSettingsForOneType client_hints_settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &client_hints_settings);
  ASSERT_EQ(1U, client_hints_settings.size());

  // Copy the client hints setting for localhost to foo.com.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      GURL("https://foo.com/"), GURL(), ContentSettingsType::CLIENT_HINTS,
      std::string(),
      std::make_unique<base::Value>(
          client_hints_settings.at(0).setting_value.Clone()));

  // Verify that client hints for the two hosts has been saved.
  host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &client_hints_settings);
  ASSERT_EQ(2U, client_hints_settings.size());

  // Navigating to without_accept_ch_without_lifetime_img_localhost() should
  // attach client hints to the image subresouce contained in that page since
  // the image is located on the same server as the document origin.
  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(
      browser(), without_accept_ch_without_lifetime_img_localhost());
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The user agent hint is attached to all three requests, as is UA-mobile:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());

  // Navigating to without_accept_ch_without_lifetime_img_foo_com() should not
  // attach client hints to the image subresouce contained in that page since
  // the image is located on a different server as the document origin.
  ui_test_utils::NavigateToURL(
      browser(), without_accept_ch_without_lifetime_img_foo_com());
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The device-memory and dprheader is attached to the main frame request.
#if defined(OS_ANDROID)
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
#else
  EXPECT_EQ(expected_client_hints_number * 3,
            count_client_hints_headers_seen());
#endif

  // Requests to third party servers should have only one client hint attached
  // (`Sec-CH-UA`).
  EXPECT_EQ(1u, third_party_request_count_seen());
  EXPECT_EQ(2u, third_party_client_hints_count_seen());
}

// Test that client hints are attached to subresources checks the right setting
// for OTR profile.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsHttpsSubresourceOffTheRecord) {
  const GURL gurl = accept_ch_with_lifetime_url();

  base::HistogramTester histogram_tester;

  // Add client hints for the embedded test server.
  ui_test_utils::NavigateToURL(browser(), gurl);
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  // Main profile should get hints for both page and subresources.
  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(
      browser(), without_accept_ch_without_lifetime_img_localhost());
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());

  // OTR profile should get neither.
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ui_test_utils::NavigateToURL(
      otr_browser, without_accept_ch_without_lifetime_img_localhost());
}

// Verify that we send only major version information in the `Sec-CH-UA` header
// by default, regardless of opt-in.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, UserAgentVersion) {
  const GURL gurl = accept_ch_with_lifetime_url();

  blink::UserAgentMetadata ua = ::GetUserAgentMetadata();

  // Navigate to a page that opts-into the header: the value should end with
  // the major version, and not contain the full version.
  SetClientHintExpectationsOnMainFrame(false);
  ui_test_utils::NavigateToURL(browser(), gurl);
  std::string expected_ua = ua.SerializeBrandVersionList();
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_TRUE(main_frame_ua_full_version_observed().empty());

  // Navigate again, after the opt-in: the value should stay the major
  // version.
  SetClientHintExpectationsOnMainFrame(true);
  ui_test_utils::NavigateToURL(browser(), gurl);
  std::string expected_full_version = "\"" + ua.full_version + "\"";
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_EQ(main_frame_ua_full_version_observed(), expected_full_version);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, UAHintsTabletMode) {
  const GURL gurl = accept_ch_with_lifetime_url();

  blink::UserAgentMetadata ua = ::GetUserAgentMetadata();

  // First request: only minimal hints, no tablet override.
  SetClientHintExpectationsOnMainFrame(false);
  ui_test_utils::NavigateToURL(browser(), gurl);
  std::string expected_ua = ua.SerializeBrandVersionList();
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_EQ(main_frame_ua_full_version_observed(), "");
  EXPECT_EQ(main_frame_ua_mobile_observed(), "?0");
  EXPECT_EQ(main_frame_ua_platform_observed(), "");

  // Second request: table override, all hints.
  chrome::ToggleRequestTabletSite(browser());
  SetClientHintExpectationsOnMainFrame(true);
  ui_test_utils::NavigateToURL(browser(), gurl);
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  std::string expected_full_version = "\"" + ua.full_version + "\"";
  EXPECT_EQ(main_frame_ua_full_version_observed(), expected_full_version);
  EXPECT_EQ(main_frame_ua_mobile_observed(), "?1");
  EXPECT_EQ(main_frame_ua_platform_observed(), "\"Android\"");
}

// TODO(morlovich): Move this into WebContentsImplBrowserTest once things are
// refactored enough that UA client hints actually work in content/
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, UserAgentOverrideClientHints) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string kHeaderPath = std::string("/echoheader?") +
                                  net::HttpRequestHeaders::kUserAgent +
                                  "&sec-ch-ua&sec-ch-ua-mobile";
  const GURL kUrl(embedded_test_server()->GetURL(kHeaderPath));

  web_contents->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly("foo"), false);
  // Not enabled first.
  ui_test_utils::NavigateToURL(browser(), kUrl);
  std::string header_value;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(document.body.textContent);",
      &header_value));
  EXPECT_EQ(std::string::npos, header_value.find("foo")) << header_value;

  // Actually turn it on.
  web_contents->GetController()
      .GetLastCommittedEntry()
      ->SetIsOverridingUserAgent(true);

  ui_test_utils::NavigateToURL(browser(), kUrl);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(document.body.textContent);",
      &header_value));
  // Since no value was provided for client hints, they are not sent.
  EXPECT_EQ("foo\nNone\nNone", header_value);

  // Now actually provide values for the hints.
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = "foobar";
  ua_override.ua_metadata_override.emplace();
  ua_override.ua_metadata_override->mobile = true;
  ua_override.ua_metadata_override->brand_version_list.emplace_back(
      "Foobarnator", "3.14");
  web_contents->SetUserAgentOverride(ua_override, false);
  ui_test_utils::NavigateToURL(browser(), kUrl);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(document.body.textContent);",
      &header_value));
  EXPECT_EQ("foobar\n\"Foobarnator\";v=\"3.14\"\n?1", header_value);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, EmptyAcceptCH) {
  // First navigate to a page that enables hints. No CH for it yet, since
  // nothing opted in.
  GURL gurl = accept_ch_with_lifetime_url();
  SetClientHintExpectationsOnMainFrame(false);
  ui_test_utils::NavigateToURL(browser(), gurl);

  // Now go to a page with blank Accept-CH. Should get hints from previous
  // visit.
  gurl = accept_ch_empty();
  SetClientHintExpectationsOnMainFrame(true);
  ui_test_utils::NavigateToURL(browser(), gurl);

  // Visiting again should not expect them since we opted out again.
  SetClientHintExpectationsOnMainFrame(false);
  ui_test_utils::NavigateToURL(browser(), gurl);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, MergeAcceptCH) {
  // Go to page where some hints are enabled by headers, some by
  // http-equiv. It shouldn't get hints itself (due to first visit),
  // but subresources should get all the client hints.
  GURL gurl = http_equiv_accept_ch_merge();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(browser(), gurl);
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
}

void ClientHintsBrowserTest::TestProfilesIndependent(Browser* browser_a,
                                                     Browser* browser_b) {
  const GURL gurl = accept_ch_with_lifetime_url();

  blink::UserAgentMetadata ua = ::GetUserAgentMetadata();

  // Navigate |browser_a| to a page that opts-into the header: the value should
  // end with the major version, and not contain the full version.
  SetClientHintExpectationsOnMainFrame(false);
  ui_test_utils::NavigateToURL(browser_a, gurl);
  std::string expected_ua = ua.SerializeBrandVersionList();
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_TRUE(main_frame_ua_full_version_observed().empty());

  // Try again on |browser_a|, the header should have an effect there.
  SetClientHintExpectationsOnMainFrame(true);
  ui_test_utils::NavigateToURL(browser_a, gurl);
  std::string expected_full_version = "\"" + ua.full_version + "\"";
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_EQ(main_frame_ua_full_version_observed(), expected_full_version);

  // Navigate on |browser_b|. That should still only have the major
  // version.
  SetClientHintExpectationsOnMainFrame(false);
  ui_test_utils::NavigateToURL(browser_b, gurl);
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_TRUE(main_frame_ua_full_version_observed().empty());
}

// Check that client hints attached to navigation inside OTR profiles
// use the right settings, regular -> OTR direction.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, OffTheRecordIndependent) {
  TestProfilesIndependent(browser(),
                          CreateIncognitoBrowser(browser()->profile()));
}

// Check that client hints attached to navigation inside OTR profiles
// use the right settings, OTR -> regular direction.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, OffTheRecordIndependent2) {
  TestProfilesIndependent(CreateIncognitoBrowser(browser()->profile()),
                          browser());
}

// Test that client hints are attached to third party subresources if
// AllowClientHintsToThirdParty feature is enabled.
IN_PROC_BROWSER_TEST_P(ClientHintsAllowThirdPartyBrowserTest,
                       ClientHintsThirdPartyAllowed) {
  GURL gurl;
  unsigned update_event_count = 0;
  if (GetParam()) {
    gurl = http_equiv_accept_ch_without_lifetime_img_localhost();
  } else {
    gurl = accept_ch_without_lifetime_img_localhost();
    update_event_count = 1;
  }

  base::HistogramTester histogram_tester;

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);

  // Add client hints for the embedded test server.
  ui_test_utils::NavigateToURL(browser(), gurl);
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount",
                                    update_event_count);

  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());

  // Requests to third party servers should not have client hints attached.
  EXPECT_EQ(1u, third_party_request_count_seen());

  // Device memory, viewport width, DRP, and UA client hints should be sent to
  // the third-party when feature "AllowClientHintsToThirdParty" is enabled.
  EXPECT_EQ(5u, third_party_client_hints_count_seen());
}

// Test that client hints are not attached to third party subresources if
// AllowClientHintsToThirdParty feature is not enabled.
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest,
                       ClientHintsThirdPartyNotAllowed) {
  GURL gurl;
  unsigned update_event_count = 0;
  if (GetParam()) {
    gurl = http_equiv_accept_ch_without_lifetime_img_localhost();
  } else {
    gurl = accept_ch_without_lifetime_img_localhost();
    update_event_count = 1;
  }

  base::HistogramTester histogram_tester;

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);

  // Add client hints for the embedded test server.
  ui_test_utils::NavigateToURL(browser(), gurl);
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount",
                                    update_event_count);

  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());

  // Requests to third party servers should not have client hints attached.
  EXPECT_EQ(1u, third_party_request_count_seen());

  // Client hints should not be sent to the third-party when feature
  // "AllowClientHintsToThirdParty" is not enabled, with the exception of the
  // `Sec-CH-UA` hint, which is sent with every request.
  EXPECT_EQ(2u, third_party_client_hints_count_seen());
}

// Loads a HTTPS webpage that does not request persisting of client hints.
// A same-origin iframe loaded by the webpage requests persistence of client
// hints. Since that's not a main frame, persistence should not happen.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       PersistenceRequestIframe_SameOrigin) {
  const GURL gurl = accept_ch_without_lifetime_with_iframe_url();
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ui_test_utils::NavigateToURL(browser(), gurl);

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // accept_ch_without_lifetime_with_iframe_url() loads
  // accept_ch_with_lifetime() in an iframe. The request to persist client
  // hints from accept_ch_with_lifetime() should not be persisted.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  histogram_tester.ExpectTotalCount("ClientHints.PersistDuration", 0);
}

// Loads a HTTPS webpage that does not request persisting of client hints.
// An iframe loaded by the webpage from an cross origin server requests
// persistence of client hints.
// Verify that the request from the cross origin iframe is not honored, and
// client hints preference is not persisted.
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest,
                       DisregardPersistenceRequestIframe_CrossOrigin) {
  const GURL gurl =
      GetParam() ? http_equiv_accept_ch_without_lifetime_with_iframe_url()
                 : accept_ch_without_lifetime_with_iframe_url();

  intercept_iframe_resource_ = gurl.path();
  intercept_to_http_equiv_iframe_ = GetParam();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ui_test_utils::NavigateToURL(browser(), gurl);

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // accept_ch_without_lifetime_with_iframe_url() loads
  // accept_ch_with_lifetime() in a cross origin iframe. The request to persist
  // client hints from accept_ch_with_lifetime() should be disregarded.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  histogram_tester.ExpectTotalCount("ClientHints.PersistDuration", 0);
}

// Loads a HTTPS webpage that does not request persisting of client hints.
// A subresource loaded by the webpage requests persistence of client hints.
// Verify that the request from the subresource is not honored, and client hints
// preference is not persisted.
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest,
                       DisregardPersistenceRequestSubresource) {
  const GURL gurl =
      GetParam() ? http_equiv_accept_ch_without_lifetime_with_subresource_url()
                 : accept_ch_without_lifetime_with_subresource_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ui_test_utils::NavigateToURL(browser(), gurl);

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // accept_ch_without_lifetime_with_subresource_url() loads
  // accept_ch_with_lifetime() as a subresource. The request to persist client
  // hints from accept_ch_with_lifetime() should be disregarded.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  histogram_tester.ExpectTotalCount("ClientHints.PersistDuration", 0);
}

// Loads a HTTPS webpage that does not request persisting of client hints.
// A subresource loaded by the webpage in an iframe requests persistence of
// client hints. Verify that the request from the subresource in the iframe
// is not honored, and client hints preference is not persisted.
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest,
                       DisregardPersistenceRequestSubresourceIframe) {
  const GURL gurl =
      GetParam()
          ? http_equiv_accept_ch_without_lifetime_with_subresource_iframe_url()
          : accept_ch_without_lifetime_with_subresource_iframe_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ui_test_utils::NavigateToURL(browser(), gurl);

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // |gurl| loads accept_ch_with_lifetime() or
  // http_equiv_accept_ch_with_lifetime() as a subresource in an iframe. The
  // request to persist client hints from accept_ch_with_lifetime() or
  // http_equiv_accept_ch_with_lifetime() should be disregarded.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  histogram_tester.ExpectTotalCount("ClientHints.PersistDuration", 0);
}

// Loads a HTTP local webpage (which qualifies as a secure context) that
// requests persisting of client hints. Verifies that the browser receives the
// mojo notification from the renderer and persists the client hints to the
// disk.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeFollowedByNoClientHintHttpLocal) {
  const GURL gurl = accept_ch_with_lifetime_http_local_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ui_test_utils::NavigateToURL(browser(), gurl);

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  // |gurl| sets client hints persist duration to 3600 seconds, but a maximum
  // value is registered instead.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      uma_histogram_max_value, 1);

  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_local_url());

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
}

// Loads a webpage that does not request persisting of client hints.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, NoClientHintsHttps) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // no_client_hints_url() does not sets the client hints.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  histogram_tester.ExpectTotalCount("ClientHints.PersistDuration", 0);
}

IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest,
                       ClientHintsLifetimeFollowedByNoClientHint) {
  const GURL gurl = GetParam() ? http_equiv_accept_ch_with_lifetime()
                               : accept_ch_with_lifetime_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching |gurl| should persist the request for client hints iff using
  // headers and not http-equiv.
  ui_test_utils::NavigateToURL(browser(), gurl);

  if (GetParam())
    histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);
  else
    histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::RunLoop().RunUntilIdle();

  if (GetParam()) {
    histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
    histogram_tester.ExpectTotalCount("ClientHints.PersistDuration", 0);
  } else {
    histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                        expected_client_hints_number, 1);
    // accept_ch_with_lifetime_url() sets client hints persist duration to 3600
    // seconds, but a maximum value is registered instead.
    histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                        uma_histogram_max_value, 1);

    // Clients hints preferences for one origin should be persisted.
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                                std::string(), &host_settings);
    EXPECT_EQ(1u, host_settings.size());
  }

  SetClientHintExpectationsOnMainFrame(!GetParam());
  SetClientHintExpectationsOnSubresources(!GetParam());
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(GetParam() ? 0 : expected_client_hints_number * 2,
            count_client_hints_headers_seen());
}

// The test first fetches a page that sets Accept-CH-Lifetime. Next, it fetches
// a URL from a different origin. However, that URL response redirects to the
// same origin from where the first page was fetched. The test verifies that
// on receiving redirect to an origin for which the browser has persisted client
// hints prefs, the browser attaches the client hints headers when fetching the
// redirected URL.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeFollowedByRedirectToNoClientHint) {
  const GURL gurl = accept_ch_with_lifetime_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching |gurl| should persist the request for client hints.
  ui_test_utils::NavigateToURL(browser(), gurl);

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  // accept_ch_with_lifetime_url() sets client hints persist duration to 3600
  // seconds, but a maximum value is registered instead.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      uma_histogram_max_value, 1);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(browser(), redirect_url());

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
}

// Ensure that even when cookies are blocked, client hint preferences are
// persisted.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimePersistedCookiesBlocked) {
  const GURL gurl_with = accept_ch_with_lifetime_url();

  scoped_refptr<content_settings::CookieSettings> cookie_settings_ =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  // Block cookies.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(gurl_with, GURL(),
                                      ContentSettingsType::COOKIES,
                                      std::string(), CONTENT_SETTING_BLOCK);

  // Fetching |gurl_with| should persist the request for client hints.
  ui_test_utils::NavigateToURL(browser(), gurl_with);
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 1);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  VerifyContentSettingsNotNotified();
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeAttachedCookiesBlocked) {
  const GURL gurl_with = accept_ch_with_lifetime_url();
  const GURL gurl_without = accept_ch_without_lifetime_url();
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching |gurl_with| should persist the request for client hints.
  ui_test_utils::NavigateToURL(browser(), gurl_with);
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  // |gurl_with| tries to set client hints persist duration to 3600 seconds, but
  // a maximum value is registered instead.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      uma_histogram_max_value, 1);

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  // Block the cookies: Client hints should be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(gurl_without, GURL(),
                                      ContentSettingsType::COOKIES,
                                      std::string(), CONTENT_SETTING_BLOCK);

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::COOKIES);
}

// Ensure that when the JavaScript is blocked, client hint preferences are not
// persisted.
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest,
                       ClientHintsLifetimeNotPersistedJavaScriptBlocked) {
  ContentSettingsForOneType host_settings;

  // Start a navigation. This navigation makes it possible to block JavaScript
  // later.
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  const GURL gurl =
      GetParam() ? http_equiv_accept_ch_without_lifetime_with_iframe_url()
                 : accept_ch_with_lifetime_url();

  // Block the JavaScript: Client hint preferences should not be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(gurl, GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      std::string(), CONTENT_SETTING_BLOCK);
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());
  VerifyContentSettingsNotNotified();

  // Allow the JavaScript: Client hint preferences should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(gurl, GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      std::string(), CONTENT_SETTING_ALLOW);
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);
}

// Ensure that when the JavaScript is blocked, persisted client hints are not
// attached to the request headers.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeNotAttachedJavaScriptBlocked) {
  const GURL gurl = accept_ch_with_lifetime_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching accept_ch_with_lifetime_url() should persist the request for
  // client hints.
  ui_test_utils::NavigateToURL(browser(), gurl);
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_EQ(1u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(1u, count_ua_mobile_client_hints_headers_seen());

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  // accept_ch_with_lifetime_url() tries to set client hints persist duration to
  // 3600 seconds, but a maximum value is registered instead.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      uma_histogram_max_value, 1);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  // Block JavaScript via WebPreferences: Client hints should not be attached.
  SetJsEnabledForActiveView(false);

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  EXPECT_EQ(0u, count_client_hints_headers_seen());
  VerifyContentSettingsNotNotified();
  EXPECT_EQ(1u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(1u, count_ua_mobile_client_hints_headers_seen());

  SetJsEnabledForActiveView(true);

  // Block JavaScript via ContentSetting: Client hints should not be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(without_accept_ch_without_lifetime_url(),
                                      GURL(), ContentSettingsType::JAVASCRIPT,
                                      std::string(), CONTENT_SETTING_BLOCK);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  VerifyContentSettingsNotNotified();
  EXPECT_EQ(1u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(1u, count_ua_mobile_client_hints_headers_seen());

  // Allow JavaScript: Client hints should now be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(without_accept_ch_without_lifetime_url(),
                                      GURL(), ContentSettingsType::JAVASCRIPT,
                                      std::string(), CONTENT_SETTING_ALLOW);

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);
}

// Test that if the content settings are malformed, then the browser does not
// crash.
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest,
                       ClientHintsMalformedContentSettings) {
  ContentSettingsForOneType client_hints_settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  // Add setting for the host.
  std::unique_ptr<base::ListValue> expiration_times_list =
      std::make_unique<base::ListValue>();
  expiration_times_list->AppendInteger(42 /* client hint value */);
  auto expiration_times_dictionary = std::make_unique<base::DictionaryValue>();
  expiration_times_dictionary->SetList("client_hints",
                                       std::move(expiration_times_list));
  expiration_times_dictionary->SetDouble(
      "expiration_time",
      (base::Time::Now() + base::TimeDelta::FromDays(1)).ToDoubleT());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      without_accept_ch_without_lifetime_url(), GURL(),
      ContentSettingsType::CLIENT_HINTS, std::string(),
      std::make_unique<base::Value>(expiration_times_dictionary->Clone()));

  // Reading the settings should now return one setting.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &client_hints_settings);
  EXPECT_EQ(1U, client_hints_settings.size());

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());
}

// Ensure that when the JavaScript is blocked, client hints requested using
// Accept-CH are not attached to the request headers for subresources.
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest,
                       ClientHintsNoLifetimeScriptNotAllowed) {
  const GURL gurl = GetParam() ? http_equiv_accept_ch_with_lifetime()
                               : accept_ch_with_lifetime_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Block the Javascript: Client hints should not be attached.
  SetClientHintExpectationsOnSubresources(false);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(
          accept_ch_without_lifetime_img_localhost(), GURL(),
          ContentSettingsType::JAVASCRIPT, std::string(),
          CONTENT_SETTING_BLOCK);
  ui_test_utils::NavigateToURL(browser(),
                               accept_ch_without_lifetime_img_localhost());
  EXPECT_EQ(0u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(0u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  EXPECT_EQ(1u, third_party_request_count_seen());
  EXPECT_EQ(0u, third_party_client_hints_count_seen());

  // Allow the Javascript: Client hints should now be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(
          accept_ch_without_lifetime_img_localhost(), GURL(),
          ContentSettingsType::JAVASCRIPT, std::string(),
          CONTENT_SETTING_ALLOW);

  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(browser(),
                               accept_ch_without_lifetime_img_localhost());

  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
  EXPECT_EQ(2u, third_party_request_count_seen());
  EXPECT_EQ(2u, third_party_client_hints_count_seen());
  VerifyContentSettingsNotNotified();

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);

  // Block the Javascript again: Client hints should not be attached.
  SetClientHintExpectationsOnSubresources(false);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(
          accept_ch_without_lifetime_img_localhost(), GURL(),
          ContentSettingsType::JAVASCRIPT, std::string(),
          CONTENT_SETTING_BLOCK);
  ui_test_utils::NavigateToURL(browser(),
                               accept_ch_without_lifetime_img_localhost());
  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
  EXPECT_EQ(3u, third_party_request_count_seen());
  EXPECT_EQ(2u, third_party_client_hints_count_seen());

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);
}

// Ensure that when the cookies is blocked, client hints are attached to the
// request headers.
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTest,
                       ClientHintsLifetimeCookiesNotAllowed) {
  const GURL gurl = GetParam()
                        ? http_equiv_accept_ch_without_lifetime_img_localhost()
                        : accept_ch_without_lifetime_img_localhost();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_ =
      CookieSettingsFactory::GetForProfile(browser()->profile());

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Block cookies.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(gurl, GURL(),
                                      ContentSettingsType::COOKIES,
                                      std::string(), CONTENT_SETTING_BLOCK);
  base::RunLoop().RunUntilIdle();

  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(browser(), gurl);
  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
  EXPECT_EQ(1u, third_party_request_count_seen());
  EXPECT_EQ(2u, third_party_client_hints_count_seen());

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::COOKIES);
}

// Verify that client hints are sent in the incognito profiles, and server
// client hint opt-ins are honored within the incognito profile.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsLifetimeFollowedByNoClientHintIncognito) {
  const GURL gurl = accept_ch_with_lifetime_url();

  base::HistogramTester histogram_tester;
  Browser* incognito = CreateIncognitoBrowser();
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(incognito->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching |gurl| should persist the request for client hints.
  ui_test_utils::NavigateToURL(incognito, gurl);

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  // accept_ch_with_lifetime_url() sets client hints persist duration to 3600
  // seconds, but a maximum value is registered instead.
  histogram_tester.ExpectUniqueSample("ClientHints.PersistDuration",
                                      uma_histogram_max_value, 1);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(incognito->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS, std::string(),
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(incognito,
                               without_accept_ch_without_lifetime_url());

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());

  // Navigate using regular profile. Client hints should not be send.
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ui_test_utils::NavigateToURL(browser(),
                               without_accept_ch_without_lifetime_url());

  // The user agent hint is attached to the two new requests.
  EXPECT_EQ(5u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(5u, count_ua_mobile_client_hints_headers_seen());

  // No additional hints are sent.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
}

class ClientHintsEnterprisePolicyTest : public ClientHintsBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kUserAgentClientHintsEnabled,
              base::Value(false));
    provider_.UpdateChromePolicy(policies);
  }
};

// Makes sure that no client hints are sent by default when the
// "UserAgentClientHintsEnabled" enterprise polickly is set to
// false
IN_PROC_BROWSER_TEST_F(ClientHintsEnterprisePolicyTest,
                       ClientHintsEnterprisePolicy) {
  const GURL gurl = accept_ch_without_lifetime_url();
  ui_test_utils::NavigateToURL(browser(), gurl);
  // These would normally be one each
  EXPECT_EQ(0u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(0u, count_ua_mobile_client_hints_headers_seen());
}

class ClientHintsWebHoldbackBrowserTest : public ClientHintsBrowserTest {
 public:
  ClientHintsWebHoldbackBrowserTest() : ClientHintsBrowserTest() {
    ConfigureHoldbackExperiment();
  }

  net::EffectiveConnectionType web_effective_connection_type_override() const {
    return web_effective_connection_type_override_;
  }

  std::unique_ptr<base::FeatureList> EnabledFeatures() override {
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    const std::string kTrialName = "TrialFoo";
    const std::string kGroupName = "GroupFoo";  // Value not used

    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

    std::map<std::string, std::string> params;

    params["web_effective_connection_type_override"] =
        net::GetNameForEffectiveConnectionType(
            web_effective_connection_type_override_);
    EXPECT_TRUE(
        base::FieldTrialParamAssociator::GetInstance()
            ->AssociateFieldTrialParams(kTrialName, kGroupName, params));

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(
        "UserAgentClientHint,LangClientHintHeader", "");
    feature_list->RegisterFieldTrialOverride(
        features::kNetworkQualityEstimatorWebHoldback.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    return feature_list;
  }

 private:
  void ConfigureHoldbackExperiment() {}

  const net::EffectiveConnectionType web_effective_connection_type_override_ =
      net::EFFECTIVE_CONNECTION_TYPE_3G;
};

// Make sure that when NetInfo holdback experiment is enabled, the NetInfo APIs
// and client hints return the overridden values. Verify that the client hints
// are overridden on both main frame and subresource requests.
IN_PROC_BROWSER_TEST_F(ClientHintsWebHoldbackBrowserTest,
                       EffectiveConnectionTypeChangeNotified) {
  SetExpectedEffectiveConnectionType(web_effective_connection_type_override());

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(browser(), accept_ch_with_lifetime_url());
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  EXPECT_EQ(0u, third_party_request_count_seen());
  EXPECT_EQ(0u, third_party_client_hints_count_seen());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ui_test_utils::NavigateToURL(
      browser(), accept_ch_without_lifetime_with_subresource_url());
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  EXPECT_EQ(0u, third_party_request_count_seen());
  EXPECT_EQ(0u, third_party_client_hints_count_seen());
}
