// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/mock_infobar_service.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "chrome/browser/previews/previews_lite_page_redirect_url_loader_interceptor.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/history/core/browser/history_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/optimization_guide/bloom_filter.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "components/previews/core/previews_switches.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/nqe/effective_connection_type.h"
#include "net/reporting/reporting_policy.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
const int kTimeoutMs = 250;
const int kRedirectLoopCount = 3;

const char kOriginHost[] = "origin.com";

// This should match the value in //components/google/core/common/google_util.cc
// so that the X-Client-Data header is sent for subresources.
const char kPreviewsHost[] = "litepages.googlezip.net";

// A host that is blacklisted for Lite Page Redirect previews and won't trigger
// on it.
const char kBlacklistedHost[] = "blacklisted.com";

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      total_count += bucket.count;
    }
    if (total_count >= count) {
      break;
    }
  }
}

}  // namespace

class BasePreviewsLitePageRedirectServerBrowserTest
    : public InProcessBrowserTest,
      public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  BasePreviewsLitePageRedirectServerBrowserTest() {}

  ~BasePreviewsLitePageRedirectServerBrowserTest() override {}

  virtual bool ShouldEnableDRPHoldback() const = 0;

  enum PreviewsServerAction {
    // Previews server will respond with HTTP 200 OK, OFCL=60,
    // Content-Length=20.
    kSuccess = 0,

    // Previews server will respond with a bypass signal (HTTP 307).
    kBypass = 1,

    // Previews server will respond with HTTP 307 to a non-preview page.
    kRedirectNonPreview = 2,

    // Previews server will respond with HTTP 503.
    kLoadshed = 3,

    // Previews server will respond with HTTP 403.
    kAuthFailure = 4,

    // Previews server will respond with HTTP 307 bypass to a non-preview page
    // and set the host-blacklist header value.
    kBypassAndBlacklistOriginHost = 5,

    // Previews server will respond with HTTP 200 and a content body that loads
    // a subresource. When the subresource is loaded, |subresources_requested|_
    // will be incremented if the X-Client-Data header if in the request.
    kSubresources = 6,

    // Previews server will respond with HTTP 307 to a preview page.
    kRedirectPreview = 7,

    // Previews server will put Chrome into a redirect loop.
    kRedirectLoop = 8,

    // The URL that intervention reports should be sent to.
    kInterventionReport = 9,
  };

  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitchASCII("data-reduction-proxy-client-config",
                           data_reduction_proxy::DummyBase64Config());
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
    cmd->AppendSwitchASCII("force-variation-ids", "42");
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitch("ignore-litepage-redirect-optimization-blacklist");
  }

  void SetUp() override {
    SetUpLitePageTest(false /* use_timeout */, false /* is_control */);

    InProcessBrowserTest::SetUp();
  }

  void SetUpLitePageTest(bool use_timeout, bool is_control) {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &BasePreviewsLitePageRedirectServerBrowserTest::HandleOriginRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    https_url_ =
        https_server_->GetURL(kOriginHost, "/previews/noscript_test.html");
    ASSERT_TRUE(https_url_.SchemeIs(url::kHttpsScheme));

    blacklisted_https_url_ =
        https_server_->GetURL(kBlacklistedHost, "/previews/noscript_test.html");
    ASSERT_TRUE(blacklisted_https_url_.SchemeIs(url::kHttpsScheme));

    https_to_https_redirect_url_ =
        https_server_->GetURL(kOriginHost, "/previews/to_https_redirect.html");
    ASSERT_TRUE(https_to_https_redirect_url_.SchemeIs(url::kHttpsScheme));

    https_redirect_loop_url_ =
        https_server_->GetURL(kOriginHost, "/previews/redirect_loop.html");
    ASSERT_TRUE(https_redirect_loop_url_.SchemeIs(url::kHttpsScheme));

    base_https_lite_page_url_ =
        https_server_->GetURL(kOriginHost, "/previews/lite_page_test.html");
    ASSERT_TRUE(base_https_lite_page_url_.SchemeIs(url::kHttpsScheme));

    https_media_url_ =
        https_server_->GetURL(kOriginHost, "/image_decoding/droids.jpg");
    ASSERT_TRUE(https_media_url_.SchemeIs(url::kHttpsScheme));

    https_subframe_url_ =
        https_server_->GetURL(kOriginHost, "/previews/iframe_blank.html");
    ASSERT_TRUE(https_subframe_url_.SchemeIs(url::kHttpsScheme));

    // Set up http server with resource monitor and redirect handler.
    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    http_server_->RegisterRequestHandler(base::BindRepeating(
        &BasePreviewsLitePageRedirectServerBrowserTest::HandleOriginRequest,
        base::Unretained(this)));
    ASSERT_TRUE(http_server_->Start());

    http_url_ =
        http_server_->GetURL(kOriginHost, "/previews/noscript_test.html");
    ASSERT_TRUE(http_url_.SchemeIs(url::kHttpScheme));

    base_http_lite_page_url_ =
        http_server_->GetURL(kOriginHost, "/previews/lite_page_test.html");
    ASSERT_TRUE(base_http_lite_page_url_.SchemeIs(url::kHttpScheme));

    http_subframe_url_ =
        http_server_->GetURL(kOriginHost, "/previews/iframe_blank.html");
    ASSERT_TRUE(http_subframe_url_.SchemeIs(url::kHttpScheme));

    http_to_https_redirect_url_ =
        http_server_->GetURL(kOriginHost, "/previews/to_https_redirect.html");
    ASSERT_TRUE(http_to_https_redirect_url_.SchemeIs(url::kHttpScheme));

    http_redirect_loop_url_ =
        http_server_->GetURL(kOriginHost, "/previews/redirect_loop.html");
    ASSERT_TRUE(http_redirect_loop_url_.SchemeIs(url::kHttpScheme));

    client_redirect_url_ =
        http_server_->GetURL(kOriginHost, "/previews/client_redirect.html");
    ASSERT_TRUE(client_redirect_url_.SchemeIs(url::kHttpScheme));

    // Set up previews server with resource handler.
    previews_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    previews_server_->RegisterRequestHandler(base::BindRepeating(
        &BasePreviewsLitePageRedirectServerBrowserTest::HandleResourceRequest,
        base::Unretained(this)));
    previews_server_->SetConnectionListener(this);
    ASSERT_TRUE(previews_server_->Start());

    previews_server_url_ = previews_server_->GetURL(kPreviewsHost, "/");
    ASSERT_TRUE(previews_server_url_.SchemeIs(url::kHttpsScheme));

    // Set up the slow HTTP server with delayed resource handler.
    slow_http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    slow_http_server_->RegisterRequestHandler(
        base::BindRepeating(&BasePreviewsLitePageRedirectServerBrowserTest::
                                HandleSlowResourceRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(slow_http_server_->Start());

    slow_http_url_ = slow_http_server_->GetURL(kOriginHost, "/");
    ASSERT_TRUE(slow_http_url_.SchemeIs(url::kHttpScheme));

    std::map<std::string, std::string> feature_parameters = {
        {"previews_host", previews_server_url().spec()},
        {"full_probe_url", previews_server_url().spec()},
        {"blacklisted_path_suffixes", ".mp4,.jpg"},
        {"trigger_on_localhost", "true"},
        {"max_navigation_restart", base::NumberToString(kRedirectLoopCount)},
        {"navigation_timeout_milliseconds",
         use_timeout ? base::NumberToString(kTimeoutMs) : "60000"},
        {"control_group", is_control ? "true" : "false"},
        {"preconnect_on_slow_connections", "true"},
        {"preresolve_on_slow_connections", "false"},
        {"should_probe_origin", "true"},
        {"origin_probe_timeout_ms", "500"},
    };

    scoped_parameterized_feature_list_.emplace();
    scoped_parameterized_feature_list_->InitAndEnableFeatureWithParameters(
        previews::features::kLitePageServerPreviews, feature_parameters);

    scoped_feature_list_.emplace();
    scoped_feature_list_->InitWithFeatures(
        {previews::features::kPreviews,
         optimization_guide::features::kOptimizationHints,
         previews::features::kResourceLoadingHints,
         data_reduction_proxy::features::
             kDataReductionProxyEnabledWithNetworkService,
         network::features::kReporting},
        {network::features::kNetworkErrorLogging});

    drp_holdback_feature_list_.emplace();
    drp_holdback_feature_list_->InitWithFeatureState(
        data_reduction_proxy::features::kDataReductionProxyHoldback,
        ShouldEnableDRPHoldback());
  }

  void TearDown() override {
    drp_holdback_feature_list_.reset();
    scoped_feature_list_.reset();
    scoped_parameterized_feature_list_.reset();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    InitializeOptimizationHints();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);

    // Some tests shouldn't bother with the notification InfoBar. Just set the
    // state on the decider so it isn't required.
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(browser()->profile());
    PreviewsLitePageRedirectDecider* decider =
        previews_service->previews_lite_page_redirect_decider();
    decider->SetUserHasSeenUINotification();

    decider->BlacklistBypassedHost(kBlacklistedHost,
                                   base::TimeDelta::FromHours(1));

    net::ReportingPolicy policy;
    policy.delivery_interval = base::TimeDelta::FromSeconds(0);
    net::ReportingPolicy::UsePolicyForTesting(policy);

    WaitForServerProbe();
  }

  void WaitForServerProbe() {
    DataReductionProxyChromeSettings* drp_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            browser()->profile());

    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(browser()->profile());
    PreviewsLitePageRedirectDecider* decider =
        previews_service->previews_lite_page_redirect_decider();

    // Wait for a completed probe to the litepages server if needed.
    while (drp_settings->IsDataReductionProxyEnabled()) {
      if (decider->IsServerProbeResultAvailable()) {
        break;
      }
      base::RunLoop().RunUntilIdle();
    }
  }

  void ProcessHintsComponent(
      const optimization_guide::HintsComponentInfo& component_info) {
    base::HistogramTester histogram_tester;

    g_browser_process->optimization_guide_service()
        ->MaybeUpdateHintsComponentOnUIThread(component_info);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);
  }

  void InitializeOptimizationHints() {
    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {"doesntmatter.com"}, "*",
            {}));
  }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  InfoBarService* GetInfoBarService() {
    return InfoBarService::FromWebContents(GetWebContents());
  }

  void ExecuteScript(const std::string& script) const {
    EXPECT_TRUE(content::ExecuteScript(GetWebContents(), script));
  }

  // Returns the loaded, non-virtual URL, of the current visible
  // NavigationEntry.
  GURL GetLoadedURL() const {
    return GetWebContents()->GetController().GetVisibleEntry()->GetURL();
  }

  void VerifyPreviewLoaded() const {
    // The Virtual URL is set in a WebContentsObserver::OnFinishNavigation.
    // Since |ui_test_utils::NavigationToURL| uses the same signal to stop
    // waiting, there is sometimes a race condition between the two, causing
    // this validation to flake. Waiting for the load stop on the page will
    // ensure that the Virtual URL has been set.
    base::RunLoop().RunUntilIdle();
    content::WaitForLoadStop(GetWebContents());

    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(GetWebContents());
    EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());

    previews::PreviewsUserData* previews_data =
        ui_tab_helper->previews_user_data();
    EXPECT_TRUE(previews_data->HasCommittedPreviewsType());
    EXPECT_EQ(previews_data->CommittedPreviewsType(),
              previews::PreviewsType::LITE_PAGE_REDIRECT);

    const GURL loaded_url = GetLoadedURL();
    EXPECT_TRUE(loaded_url.DomainIs(previews_server_url().host()));
    EXPECT_EQ(loaded_url.EffectiveIntPort(),
              previews_server_url().EffectiveIntPort());

    content::NavigationEntry* entry =
        GetWebContents()->GetController().GetVisibleEntry();

    EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());
    const GURL virtual_url = entry->GetVirtualURL();

    // The loaded url should be the previews version of the virtual url.
    EXPECT_EQ(loaded_url, previews::GetLitePageRedirectURLForURL(virtual_url));

    EXPECT_FALSE(virtual_url.DomainIs(previews_server_url().host()) &&
                 virtual_url.EffectiveIntPort() ==
                     previews_server_url().EffectiveIntPort());
  }

  void VerifyPreviewNotLoaded() const {
    // The Virtual URL is set in a |WebContentsObserver::OnFinishNavigation|.
    // Since |ui_test_utils::NavigationToURL| uses the same signal to stop
    // waiting, there is sometimes a race condition between the two, causing
    // this validation to flake. Waiting for the load stop on the page will
    // ensure that the Virtual URL has been set.
    base::RunLoop().RunUntilIdle();
    content::WaitForLoadStop(GetWebContents());

    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(GetWebContents());
    EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());

    previews::PreviewsUserData* previews_data =
        ui_tab_helper->previews_user_data();
    EXPECT_FALSE(previews_data->HasCommittedPreviewsType());
    EXPECT_NE(previews_data->CommittedPreviewsType(),
              previews::PreviewsType::LITE_PAGE_REDIRECT);

    const GURL loaded_url = GetLoadedURL();
    EXPECT_FALSE(loaded_url.DomainIs(previews_server_url().host()) &&
                 loaded_url.EffectiveIntPort() ==
                     previews_server_url().EffectiveIntPort());

    content::NavigationEntry* entry =
        GetWebContents()->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());

    // The Virtual URL and the loaded URL should be the same.
    EXPECT_EQ(loaded_url, entry->GetVirtualURL());
  }

  void VerifyErrorPageLoaded() const {
    const GURL loaded_url = GetLoadedURL();
    EXPECT_FALSE(loaded_url.DomainIs(previews_server_url().host()) &&
                 loaded_url.EffectiveIntPort() ==
                     previews_server_url().EffectiveIntPort());

    content::NavigationEntry* entry =
        GetWebContents()->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
  }

  void ResetDataSavings() const {
    DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
        browser()->profile())
        ->data_reduction_proxy_service()
        ->compression_stats()
        ->ResetStatistics();
  }

  // Gets the data usage recorded against the host the origin server runs on.
  uint64_t GetDataUsage() const {
    const auto& data_usage_map =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            browser()->profile())
            ->data_reduction_proxy_service()
            ->compression_stats()
            ->DataUsageMapForTesting();
    const auto& it = data_usage_map.find(kOriginHost);
    if (it != data_usage_map.end())
      return it->second->data_used();
    return 0;
  }

  // Gets the data usage recorded against all hosts.
  uint64_t GetTotalDataUsage() const {
    return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
               browser()->profile())
        ->data_reduction_proxy_service()
        ->compression_stats()
        ->GetHttpReceivedContentLength();
  }

  // Gets the original content length recorded against all hosts.
  uint64_t GetTotalOriginalContentLength() const {
    return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
               browser()->profile())
        ->data_reduction_proxy_service()
        ->compression_stats()
        ->GetHttpOriginalContentLength();
  }

  base::Value ParsedInterventionReport() const {
    base::Value parsed_payload =
        base::test::ParseJson(intervention_report_content_);
    // Clear out any non-reproducible fields.
    for (auto& report : parsed_payload.GetList()) {
      report.RemoveKey("age");
      report.RemoveKey("user_agent");
    }
    return parsed_payload;
  }

  // Returns a HTTP URL that will respond with the given action and headers when
  // used by the previews server. The response can be delayed a number of
  // milliseconds by passing a value > 0 for |delay_ms| or pass -1 to make the
  // response hang indefinitely.
  GURL HttpLitePageURL(PreviewsServerAction action,
                       std::string* headers = nullptr,
                       int delay_ms = 0) const {
    std::string query = "resp=" + base::NumberToString(action);
    if (delay_ms != 0)
      query += "&delay_ms=" + base::NumberToString(delay_ms);
    if (headers)
      query += "&headers=" + *headers;
    GURL::Replacements replacements;
    replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
    return base_http_lite_page_url().ReplaceComponents(replacements);
  }

  // Returns a HTTPS URL that will respond with the given action and headers
  // when used by the previews server. The response can be delayed a number of
  // milliseconds by passing a value > 0 for |delay_ms| or pass -1 to make the
  // response hang indefinitely.
  GURL HttpsLitePageURL(PreviewsServerAction action,
                        std::string* headers = nullptr,
                        int delay_ms = 0) const {
    std::string query = "resp=" + base::NumberToString(action);
    if (delay_ms != 0)
      query += "&delay_ms=" + base::NumberToString(delay_ms);
    if (headers)
      query += "&headers=" + *headers;
    GURL::Replacements replacements;
    replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
    return base_https_lite_page_url().ReplaceComponents(replacements);
  }

  void ClearDeciderState() const {
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(previews_service);
    PreviewsLitePageRedirectDecider* decider =
        previews_service->previews_lite_page_redirect_decider();
    ASSERT_TRUE(decider);
    decider->ClearStateForTesting();
  }

  virtual GURL previews_server_url() const { return previews_server_url_; }

  const GURL& blacklisted_https_url() const { return blacklisted_https_url_; }

  const GURL& https_url() const { return https_url_; }
  const GURL& base_https_lite_page_url() const {
    return base_https_lite_page_url_;
  }
  const GURL& https_media_url() const { return https_media_url_; }
  const GURL& http_url() const { return http_url_; }
  const GURL& slow_http_url() const { return slow_http_url_; }
  const GURL& base_http_lite_page_url() const {
    return base_http_lite_page_url_;
  }
  const GURL& http_to_https_redirect_url() const {
    return http_to_https_redirect_url_;
  }
  const GURL& https_to_https_redirect_url() const {
    return https_to_https_redirect_url_;
  }
  const GURL& http_redirect_loop_url() const { return http_redirect_loop_url_; }
  const GURL& https_redirect_loop_url() const {
    return https_redirect_loop_url_;
  }
  const GURL& client_redirect_url() const { return client_redirect_url_; }
  const GURL& http_subframe_url() const { return http_subframe_url_; }
  const GURL& https_subframe_url() const { return https_subframe_url_; }

  int origin_probe_count() const { return origin_probe_count_; }
  void set_origin_probe_success(bool success) {
    origin_probe_success_ = success;
  }

  uint64_t got_page_id() const { return got_page_id_; }
  int subresources_requested() const { return subresources_requested_; }
  int previews_server_connections() const {
    return previews_server_connections_.size();
  }

  void WaitForInterventionReport() {
    if (!intervention_report_content_.empty())
      return;

    base::RunLoop run_loop;
    waiting_for_report_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 protected:
  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleOriginRequest(
      const net::test_server::HttpRequest& request) {
    if (request.method == net::test_server::METHOD_HEAD) {
      origin_probe_count_++;
      if (origin_probe_success_) {
        std::unique_ptr<net::test_server::BasicHttpResponse> response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_NO_CONTENT);
        return std::move(response);
      }
      return std::make_unique<net::test_server::HungResponse>();
    }

    if (request.GetURL().spec().find("to_https_redirect") !=
        std::string::npos) {
      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->set_content_type("text/html");
      response->AddCustomHeader("Location", https_url().spec());
      return std::move(response);
    }

    if (request.GetURL().spec().find("client_redirect") != std::string::npos) {
      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content("<html><body><script>window.location = \"" +
                            HttpsLitePageURL(kSuccess).spec() +
                            "\";</script></body></html>");
      return std::move(response);
    }

    if (request.GetURL().spec().find("redirect_loop") != std::string::npos) {
      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->set_content_type("text/html");

      if (request.GetURL().SchemeIsCryptographic()) {
        response->AddCustomHeader("Location", http_redirect_loop_url().spec());
      } else {
        // Provide a way out. If this request wasn't forward by the previews
        // server, end the loop.
        if (request.GetURL().spec().find("from_previews_server") !=
            std::string::npos) {
          response->AddCustomHeader("Location",
                                    https_redirect_loop_url().spec());
        } else {
          response->set_code(net::HttpStatusCode::HTTP_OK);
        }
      }

      return std::move(response);
    }

    return nullptr;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSlowResourceRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::DelayedHttpResponse> response =
        std::make_unique<net::test_server::DelayedHttpResponse>(
            base::TimeDelta::FromMilliseconds(500));
    response->set_code(net::HttpStatusCode::HTTP_OK);
    return std::move(response);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleResourceRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    // If this request is for a subresource, record if the X-Client-Data header
    // exists.
    if (request.GetURL().spec().find("subresource.png") != std::string::npos) {
      if (request.headers.find("X-Client-Data") != request.headers.end()) {
        subresources_requested_++;
      }
      response->set_code(net::HTTP_OK);
      return response;
    }

    // If this request is for a intervention report, record the content.
    if (request.GetURL().spec().find("upload_report") != std::string::npos) {
      intervention_report_content_ = request.content;
      response->set_code(net::HTTP_NO_CONTENT);
      if (waiting_for_report_closure_)
        std::move(waiting_for_report_closure_).Run();
      return response;
    }

    response->set_content_type("text/html");

    std::string original_url_str;

    // EmbeddedTestServer's request URL is 127.0.0.1 which causes
    // |ExtractOriginalURLFromLitePageRedirectURL| to fail. So, if the ports
    // match, fix up the url to have the preview hostname for the call to
    // |ExtractOriginalURLFromLitePageRedirectURL|.
    GURL url = request.GetURL();
    if (url.EffectiveIntPort() == previews_server_url().EffectiveIntPort()) {
      url = GURL(previews_server_url().spec() + url.path() + "?" + url.query());
    }
    // Ignore anything that's not a previews request with an unused status.
    if (!previews::ExtractOriginalURLFromLitePageRedirectURL(
            url, &original_url_str)) {
      response->set_code(net::HttpStatusCode::HTTP_BAD_REQUEST);
      return response;
    }
    GURL original_url = GURL(original_url_str);

    // Reject anything that doesn't have the DataSaver headers.
    const std::string want_headers[] = {"chrome-proxy-ect", "chrome-proxy"};
    for (const std::string& header : want_headers) {
      if (request.headers.find(header) == request.headers.end()) {
        response->set_code(
            net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED);
        return response;
      }
    }

    // The chrome-proxy header should have the pid  or s option.
    if (request.headers.find("chrome-proxy")->second.find("s=") ==
            std::string::npos ||
        request.headers.find("chrome-proxy")->second.find("pid=") ==
            std::string::npos) {
      response->set_code(
          net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED);
      return response;
    }
    net::HttpRequestHeaders headers;
    headers.AddHeaderFromString("chrome-proxy: " +
                                request.headers.find("chrome-proxy")->second);
    got_page_id_ = data_reduction_proxy::DataReductionProxyRequestOptions::
                       GetPageIdFromRequestHeaders(headers)
                           .value();

    if (request.GetURL().spec().find("redirect_loop") != std::string::npos) {
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader("Location", http_redirect_loop_url().spec() +
                                                "?from_previews_server=true");
      return std::move(response);
    }

    std::string delay_query_param;
    int delay_ms = 0;

    // Determine whether to delay the preview response using the |original_url|.
    if (net::GetValueForKeyInQuery(original_url, "delay_ms",
                                   &delay_query_param)) {
      base::StringToInt(delay_query_param, &delay_ms);
    }

    if (delay_ms == -1) {
      return std::make_unique<net::test_server::HungResponse>();
    }
    if (delay_ms > 0) {
      response = std::make_unique<net::test_server::DelayedHttpResponse>(
          base::TimeDelta::FromMilliseconds(delay_ms));
      response->set_content_type("text/html");
    }

    std::string code_query_param;
    int return_code = 0;
    if (net::GetValueForKeyInQuery(original_url, "resp", &code_query_param))
      base::StringToInt(code_query_param, &return_code);

    GURL subresource_url("https://foo." + std::string(kPreviewsHost) + ":" +
                         previews_server_url().port() + "/subresource.png");
    std::string subresource_body = "<html><body><img src=\"" +
                                   subresource_url.spec() +
                                   "\"/></body></html>";
    switch (return_code) {
      case kSuccess:
        response->set_code(net::HTTP_OK);
        response->set_content("porgporgporgporgporg" /* length = 20 */);
        response->AddCustomHeader("chrome-proxy", "ofcl=60");
        // Use the Host header for the report because CORS.
        response->AddCustomHeader(
            "Report-To",
            base::StringPrintf("{\"endpoints\":[{\"url\":\"https://%s/"
                               "?upload_report=true\"}],\"max_age\":86400}",
                               request.headers.find("Host")->second.c_str()));
        break;
      case kRedirectNonPreview:
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location", blacklisted_https_url().spec());
        break;
      case kRedirectPreview:
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location",
                                  HttpsLitePageURL(kSuccess).spec());
        break;
      case kRedirectLoop:
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location", https_redirect_loop_url().spec());
        break;
      case kBypass:
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        // This will not cause a redirect loop because on following this
        // redirect, the URL will no longer be a preview URL and the embedded
        // test server will respond with HTTP 400.
        response->AddCustomHeader("Location", original_url.spec());
        break;
      case kBypassAndBlacklistOriginHost:
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        // This will not cause a redirect loop because on following this
        // redirect, the URL will no longer be a preview URL and the embedded
        // test server will respond with HTTP 400.
        response->AddCustomHeader("Location", original_url.spec());
        response->AddCustomHeader("chrome-proxy", "host-blacklisted");
        break;
      case kAuthFailure:
        response->set_code(net::HTTP_FORBIDDEN);
        break;
      case kLoadshed:
        response->set_code(net::HTTP_SERVICE_UNAVAILABLE);
        break;
      case kSubresources:
        response->set_content(subresource_body);
        break;
      default:
        response->set_code(net::HTTP_OK);
        break;
    }

    std::string headers_query_param;
    if (net::GetValueForKeyInQuery(original_url, "headers",
                                   &headers_query_param)) {
      net::HttpRequestHeaders headers;
      headers.AddHeadersFromString(headers_query_param);
      net::HttpRequestHeaders::Iterator iter(headers);
      while (iter.GetNext())
        response->AddCustomHeader(iter.name(), iter.value());
    }
    return std::move(response);
  }

  // net::test_server::EmbeddedTestServerConnectionListener:
  void ReadFromSocket(const net::StreamSocket& socket, int rv) override {}
  void AcceptedSocket(const net::StreamSocket& socket) override {
    // AcceptedSocket is called every time there is a new HTTP request/response
    // on a socket, even if the socket is being reused. So instead of
    // incrementing a counter, keep track of the addresses of the socket in use.
    net::IPEndPoint server_end_point;
    socket.GetLocalAddress(&server_end_point);

    net::IPEndPoint remote_end_point;
    socket.GetPeerAddress(&remote_end_point);

    std::string unique_socket_id =
        server_end_point.ToString() + remote_end_point.ToString();
    previews_server_connections_.insert(unique_socket_id);
  }

  base::Optional<base::test::ScopedFeatureList>
      scoped_parameterized_feature_list_;
  base::Optional<base::test::ScopedFeatureList> scoped_feature_list_;
  base::Optional<base::test::ScopedFeatureList> drp_holdback_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> previews_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> slow_http_server_;
  GURL https_url_;
  GURL blacklisted_https_url_;
  GURL base_https_lite_page_url_;
  GURL https_media_url_;
  GURL http_url_;
  GURL base_http_lite_page_url_;
  GURL http_to_https_redirect_url_;
  GURL http_redirect_loop_url_;
  GURL https_redirect_loop_url_;
  GURL https_to_https_redirect_url_;
  GURL client_redirect_url_;
  GURL http_subframe_url_;
  GURL https_subframe_url_;
  GURL previews_server_url_;
  GURL slow_http_url_;
  bool origin_probe_success_ = true;
  int origin_probe_count_ = 0;
  uint64_t got_page_id_ = 0;
  int subresources_requested_ = 0;
  std::unordered_set<std::string> previews_server_connections_;
  std::string intervention_report_content_;
  base::OnceClosure waiting_for_report_closure_;
};

// Param is true if DRP holdback should be enabled.
class PreviewsLitePageRedirectServerBrowserTest
    : public BasePreviewsLitePageRedirectServerBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool ShouldEnableDRPHoldback() const override { return GetParam(); }
};

// Param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectServerBrowserTest,
    ::testing::Bool());

// Previews InfoBar (which these tests trigger) does not work on Mac.
// See https://crbug.com/782322 for detail.
// Also occasional flakes on win7 (https://crbug.com/789542).
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsTriggering)) {
  // TODO(crbug.com/874150): Use ExpectUniqueSample in these tests.
  // The histograms in these tests can only be checked by the expected bucket,
  // and not by a unique sample. This is because each navigation to a preview
  // will cause two navigations and two records, one for the original navigation
  // under test, and another one for loading the preview.

  {
    // Verify the preview is not triggered on HTTP pageloads.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpLitePageURL(kSuccess));
    VerifyPreviewNotLoaded();
    ClearDeciderState();
  }

  {
    // Verify the preview is triggered on HTTPS pageloads.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
    VerifyPreviewLoaded();
  }

  {
    // Verify the preview is not triggered when loading a media resource.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), https_media_url());
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.EligibilityReason.LitePageRedirect",
        static_cast<int>(
            previews::PreviewsEligibilityReason::EXCLUDED_BY_MEDIA_SUFFIX),
        1);
  }

  {
    // Verify the preview is not triggered for POST navigations.
    std::string post_data = "helloworld";
    NavigateParams params(browser(), https_url(), ui::PAGE_TRANSITION_LINK);
    params.window_action = NavigateParams::SHOW_WINDOW;
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.is_renderer_initiated = false;
    params.post_data = network::ResourceRequestBody::CreateFromBytes(
        post_data.data(), post_data.size());

    ui_test_utils::NavigateToURL(&params);
    base::RunLoop().RunUntilIdle();
    content::WaitForLoadStop(GetWebContents());

    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(GetWebContents());
    EXPECT_FALSE(ui_tab_helper->displayed_preview_ui());
    EXPECT_FALSE(ui_tab_helper->previews_user_data());

    ClearDeciderState();
  }

  {
    // Verify the preview is not triggered when navigating to the previews
    // server.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(
        browser(),
        previews::GetLitePageRedirectURLForURL(HttpsLitePageURL(kSuccess)));
    VerifyPreviewNotLoaded();
  }

  {
    // Verify the preview is not triggered when navigating to a private IP.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), GURL("https://0.0.0.0/"));
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.BlacklistReasons",
        previews::LitePageRedirectBlacklistReason::kNavigationToPrivateDomain,
        1);
    VerifyErrorPageLoaded();
  }

  {
    // Verify the preview is not triggered when navigating to a domain without a
    // dot.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), GURL("https://no-dots-here/"));
    VerifyErrorPageLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.BlacklistReasons",
        previews::LitePageRedirectBlacklistReason::kNavigationToPrivateDomain,
        1);
  }

  {
    // Verify a preview is only shown on slow networks.
    base::HistogramTester histogram_tester;
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_3G);

    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));

    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectTotalCount(
        "Previews.ServerLitePage.PreresolvedToPreviewServer", 0);

    // Reset ECT for future tests.
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);
  }

  {
    // Verify a preview is not shown for an unknown ECT.
    base::HistogramTester histogram_tester;
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));

    VerifyPreviewNotLoaded();
    ClearDeciderState();

    // Reset ECT for future tests.
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  }

  {
    // Verify a preview is not shown if cookies are blocked.
    base::HistogramTester histogram_tester;
    int before_subresources_requested = subresources_requested();
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSubresources));
    VerifyPreviewLoaded();
    EXPECT_EQ(before_subresources_requested + 1, subresources_requested());

    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSubresources));
    VerifyPreviewNotLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.IneligibleReasons",
        previews::LitePageRedirectIneligibleReason::kCookiesBlocked, 1);

    // Reset state for other tests.
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  }
  {
    // Verify a preview is not shown for a redirect loop.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kRedirectLoop));

    // Make sure we're done with all the navigation restarts before running
    // checks.
    for (int i = 0; i < kRedirectLoopCount + 1; i++) {
      base::RunLoop().RunUntilIdle();
      content::WaitForLoadStop(GetWebContents());
    }

    VerifyPreviewNotLoaded();
    ClearDeciderState();
  }

  {
    // Verify a subframe navigation does not trigger a preview on an ineligible
    // page.
    ui_test_utils::NavigateToURL(browser(), http_subframe_url());
    VerifyPreviewNotLoaded();
  }

  {
    // Verify a subframe navigation does not trigger a preview on a preview
    // page..
    ui_test_utils::NavigateToURL(browser(), https_subframe_url());
    VerifyPreviewLoaded();
  }

  {
    base::HistogramTester histogram_tester;
    NavigateParams params(browser(), https_url(), ui::PAGE_TRANSITION_FROM_API);
    ui_test_utils::NavigateToURL(&params);
    VerifyPreviewNotLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.IneligibleReasons",
        previews::LitePageRedirectIneligibleReason::kAPIPageTransition, 1);
  }
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsOriginProbe_Success)) {
  set_origin_probe_success(true);

  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
  EXPECT_EQ(1, origin_probe_count());
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsOriginProbe_Fail)) {
  set_origin_probe_success(false);

  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  EXPECT_EQ(1, origin_probe_count());
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsReloadSoftOptOut)) {
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();

  GetWebContents()->GetController().Reload(content::ReloadType::NORMAL, false);
  VerifyPreviewNotLoaded();
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsNoChromeProxyHeader)) {
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();

  // Mimic a bad proxy header update.
  net::HttpRequestHeaders empty;
  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(browser()->profile());
  PreviewsLitePageRedirectDecider* decider =
      previews_service->previews_lite_page_redirect_decider();
  decider->OnProxyRequestHeadersChanged(empty);

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();

  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.IneligibleReasons",
      static_cast<int>(
          previews::LitePageRedirectIneligibleReason::kInvalidProxyHeaders),
      1);
}

class PreviewsLitePageRedirectServerBrowserTestWithAlwaysHoldback
    : public PreviewsLitePageRedirectServerBrowserTest {
 public:
  PreviewsLitePageRedirectServerBrowserTestWithAlwaysHoldback() {
    feature_list_.InitAndEnableFeatureWithParameters(
        {previews::features::kCoinFlipHoldback},
        {{"force_coin_flip_always_holdback", "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTestWithAlwaysHoldback,
    DISABLE_ON_WIN_MAC_CHROMEOS(CoinFlipHoldbackTriggering)) {
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
}

IN_PROC_BROWSER_TEST_P(PreviewsLitePageRedirectServerBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(PredictorShownAndHidden)) {
  base::HistogramTester histogram_tester;
  GetWebContents()->WasHidden();
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
  histogram_tester.ExpectTotalCount(
      "Previews.ServerLitePage.PreconnectedToPreviewServer", 0);

  GetWebContents()->WasShown();
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
  // We expect a value of 2 because the timer will be triggered twice, once when
  // the web contents is shown, and once when a new page is committed.
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PreconnectedToPreviewServer", false, 2);
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsLoadOriginal)) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();

  PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext()))
      ->previews_ui_service()
      ->previews_decider_impl()
      ->SetIgnorePreviewsBlacklistDecision(false /* ignored */);

  PreviewsUITabHelper::FromWebContents(GetWebContents())
      ->ReloadWithoutPreviews();
  VerifyPreviewNotLoaded();
}

IN_PROC_BROWSER_TEST_P(PreviewsLitePageRedirectServerBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsRedirect)) {
  {
    // Verify the preview is triggered when an HTTP page redirects to HTTPS.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), http_to_https_redirect_url());
    VerifyPreviewLoaded();
  }

  {
    // Verify the preview is triggered when an HTTPS page redirects to HTTPS.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), https_to_https_redirect_url());
    VerifyPreviewLoaded();
  }

  {
    // Verify the preview is not triggered when the previews server redirects to
    // a non-preview page.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(),
                                 HttpsLitePageURL(kRedirectNonPreview));
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        previews::LitePageRedirectServerResponse::kRedirect, 1);
  }

  {
    // Verify the preview is triggered when the previews server redirects to a
    // preview page.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kRedirectPreview));
    VerifyPreviewLoaded();
    ClearDeciderState();

    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        previews::LitePageRedirectServerResponse::kRedirect, 1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        previews::LitePageRedirectServerResponse::kOk, 1);
  }
}

IN_PROC_BROWSER_TEST_P(PreviewsLitePageRedirectServerBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsResponse)) {
  {
    // Verify the preview is not triggered when the server responds with bypass
    // 307.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kBypass));
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        previews::LitePageRedirectServerResponse::kPreviewUnavailable, 1);

    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.HostBlacklistedOnBypass", false, 1);
  }

  {
    // Verify the preview is not triggered when the server responds with bypass
    // 307 and host-blacklist.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(
        browser(), HttpsLitePageURL(kBypassAndBlacklistOriginHost));
    VerifyPreviewNotLoaded();

    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        previews::LitePageRedirectServerResponse::kPreviewUnavailable, 1);
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.HostBlacklistedOnBypass", true, 1);

    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
    VerifyPreviewNotLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.BlacklistReasons",
        previews::LitePageRedirectBlacklistReason::kHostBypassBlacklisted, 1);
    ClearDeciderState();

    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
    VerifyPreviewLoaded();
  }

  {
    // Verify the preview is not triggered when the server responds with 503.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kLoadshed));
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        previews::LitePageRedirectServerResponse::kServiceUnavailable, 1);
  }
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsAuthFailure)) {
  // Verify the preview is not triggered when the server responds with 403.
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kAuthFailure));
  VerifyPreviewNotLoaded();
  ClearDeciderState();
  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.ServerResponse",
      previews::LitePageRedirectServerResponse::kAuthFailure, 1);

  // DRP config is invalid, a subsequent preview page load should fail.
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();

  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.IneligibleReasons",
      previews::LitePageRedirectIneligibleReason::kInvalidProxyHeaders, 1);
}

IN_PROC_BROWSER_TEST_P(PreviewsLitePageRedirectServerBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsLoadshed)) {
  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(previews_service);
  PreviewsLitePageRedirectDecider* decider =
      previews_service->previews_lite_page_redirect_decider();
  ASSERT_TRUE(decider);

  std::unique_ptr<base::SimpleTestTickClock> clock =
      std::make_unique<base::SimpleTestTickClock>();
  decider->SetClockForTesting(clock.get());

  // Send a loadshed response. Client should not retry for a randomly chosen
  // duration [1 min, 5 mins).
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kLoadshed));
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  clock->Advance(base::TimeDelta::FromMinutes(1));
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  clock->Advance(base::TimeDelta::FromMinutes(4));
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();

  // Send a loadshed response with a specific time duration, 30 seconds, to
  // retry after.
  std::string headers = "Retry-After: 30";
  ui_test_utils::NavigateToURL(browser(),
                               HttpsLitePageURL(kLoadshed, &headers));
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();

  clock->Advance(base::TimeDelta::FromSeconds(31));
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePageURLNotReportedToHistory)) {
  base::CancelableTaskTracker tracker_;
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);

  // Verify the lite pages URL doesn't make it into the History Service via
  // the committed URL.
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
  {
    base::RunLoop loop;
    history_service->QueryURL(
        HttpsLitePageURL(kSuccess), false /* want_visits */,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          EXPECT_TRUE(result.success);
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
  }
  {
    base::RunLoop loop;
    history_service->QueryURL(
        previews::GetLitePageRedirectURLForURL(HttpsLitePageURL(kSuccess)),
        false /* want_visits */,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          EXPECT_FALSE(result.success);
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
  }

  // Verify the lite pages URL doesn't make it into the History Service via the
  // redirect chain.
  ui_test_utils::NavigateToURL(browser(),
                               HttpsLitePageURL(kRedirectNonPreview));
  VerifyPreviewNotLoaded();
  {
    base::RunLoop loop;
    history_service->QueryRedirectsFrom(
        HttpsLitePageURL(kRedirectNonPreview),
        base::BindLambdaForTesting([&](history::RedirectList redirects) {
          EXPECT_FALSE(redirects.empty());
          for (const GURL& url : redirects) {
            EXPECT_FALSE(previews::IsLitePageRedirectPreviewURL(url));
          }
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
  }
  ClearDeciderState();
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsReportSavings)) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(data_reduction_proxy::prefs::kDataUsageReportingEnabled,
                    true);
  // Give the setting notification a chance to propagate.
  base::RunLoop().RunUntilIdle();

  ResetDataSavings();

  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();

  base::RunLoop().RunUntilIdle();

  // Navigate to an untracked (no preview) page before checking reported savings
  // to reduce flakiness.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  EXPECT_EQ(GetTotalOriginalContentLength() - GetTotalDataUsage(), 40U);
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsClientRedirect)) {
  // Navigate to a non-preview first.
  ui_test_utils::NavigateToURL(browser(), https_media_url());
  VerifyPreviewNotLoaded();

  // Navigate to a page that causes a client redirect to a page that will
  // get a preview.
  ui_test_utils::NavigateToURL(browser(), client_redirect_url());
  VerifyPreviewLoaded();
  EXPECT_EQ(GetWebContents()->GetController().GetEntryAtOffset(-1)->GetURL(),
            https_media_url());
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsNavigation)) {
  // Use a sequence of different URLs, otherwise forward/back doesn't work as
  // expected.
  const auto url1 = GURL(HttpsLitePageURL(kSuccess).spec() + "&z=1");
  const auto url2 = GURL(HttpsLitePageURL(kSuccess).spec() + "&z=2");
  const auto url3 = GURL(HttpsLitePageURL(kSuccess).spec() + "&z=3");
  const auto url4 = GURL(HttpsLitePageURL(kSuccess).spec() + "&z=4");

  // 2g so we get previews.
  auto* nqt = g_browser_process->network_quality_tracker();
  nqt->ReportEffectiveConnectionTypeForTesting(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  {
    SCOPED_TRACE("First preview load");
    ui_test_utils::NavigateToURL(browser(), url1);
    VerifyPreviewLoaded();
  }

  {
    SCOPED_TRACE("Second preview load");
    ui_test_utils::NavigateToURL(browser(), url2);
    VerifyPreviewLoaded();
  }

  // 4g so we don't get previews.
  nqt->ReportEffectiveConnectionTypeForTesting(
      net::EFFECTIVE_CONNECTION_TYPE_4G);

  {
    SCOPED_TRACE("First non-preview load");
    ui_test_utils::NavigateToURL(browser(), url3);
    VerifyPreviewNotLoaded();
  }

  {
    SCOPED_TRACE("Second non-preview load");
    ui_test_utils::NavigateToURL(browser(), url4);
    VerifyPreviewNotLoaded();
  }

  // Note: |VerifyPreviewLoaded| calls |content::WaitForLoadStop()| so these are
  // safe.

  // 2g to re-enable previews. However, since all of the following navigations
  // are forward/back, they should use reuse the same previews state from the
  // corresponding original navigation.
  nqt->ReportEffectiveConnectionTypeForTesting(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  {
    SCOPED_TRACE("Navigate back to first non-preview load");
    base::HistogramTester histogram_tester;
    GetWebContents()->GetController().GoBack();
    VerifyPreviewNotLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.IneligibleReasons",
        previews::LitePageRedirectIneligibleReason::kForwardBackPageTransition,
        1);
  }

  {
    SCOPED_TRACE("Navigate back to second preview load");
    GetWebContents()->GetController().GoBack();
    VerifyPreviewLoaded();
  }

  {
    SCOPED_TRACE("Navigate back to first preview load");
    GetWebContents()->GetController().GoBack();
    VerifyPreviewLoaded();
  }

  {
    SCOPED_TRACE("Navigate forward to second preview load");
    GetWebContents()->GetController().GoForward();
    VerifyPreviewLoaded();
  }

  {
    SCOPED_TRACE("Navigate forward to first non-preview load");
    base::HistogramTester histogram_tester;
    GetWebContents()->GetController().GoForward();
    VerifyPreviewNotLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.IneligibleReasons",
        previews::LitePageRedirectIneligibleReason::kForwardBackPageTransition,
        1);
  }

  {
    SCOPED_TRACE("Navigate back again to second preview load");
    GetWebContents()->GetController().GoBack();
    VerifyPreviewLoaded();
  }
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePageSendsInterventionReport)) {
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
  WaitForInterventionReport();

  base::Value expected = base::test::ParseJson(base::StringPrintf(
      R"text(
        [
          {
            "body": {
              "id": "LitePageServed",
              "message": "Modified page load behavior on the page because )text"
      R"text(the page was expected to take a long amount of time to load. )text"
      R"text(https://www.chromestatus.com/feature/5148050062311424"
            },
            "type": "intervention",
            "url": "%s",
          }
        ]
      )text",
      previews::GetLitePageRedirectURLForURL(HttpsLitePageURL(kSuccess))
          .spec()
          .c_str()));

  EXPECT_EQ(expected, ParsedInterventionReport());
}

class PreviewsLitePageRedirectServerTimeoutBrowserTest
    : public PreviewsLitePageRedirectServerBrowserTest {
 public:
  PreviewsLitePageRedirectServerTimeoutBrowserTest() = default;

  ~PreviewsLitePageRedirectServerTimeoutBrowserTest() override = default;

  void SetUp() override {
    SetUpLitePageTest(true /* use_timeout */, false /* is_control */);

    InProcessBrowserTest::SetUp();
  }
};

// Param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectServerTimeoutBrowserTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(PreviewsLitePageRedirectServerTimeoutBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsTimeout)) {
  {
    // Ensure that a hung previews navigation doesn't wind up at the previews
    // server.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(),
                                 HttpsLitePageURL(kSuccess, nullptr, -1));
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.ServerResponse",
        previews::LitePageRedirectServerResponse::kTimeout, 1);
  }

  {
    // Ensure that a hung normal navigation eventually loads.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), slow_http_url());
    VerifyPreviewNotLoaded();
    ClearDeciderState();
    histogram_tester.ExpectTotalCount("Previews.ServerLitePage.ServerResponse",
                                      0);
  }
}

// Disabled previously on WIN/MAC/CHROMEOS, flakiness still occurred on various
// linux bots, so this test is now fully disabled due to that flakiness. See
// https://crbug.com/1024824.
IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerTimeoutBrowserTest,
    DISABLED_LitePagePreviewsOriginProbe_ExternalFailureReported) {
  set_origin_probe_success(true);

  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(),
                               HttpsLitePageURL(kSuccess, nullptr, -1));
  VerifyPreviewNotLoaded();
  ClearDeciderState();
  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.ServerResponse",
      previews::LitePageRedirectServerResponse::kTimeout, 1);

  WaitForServerProbe();

  histogram_tester.ExpectUniqueSample(
      "Availability.Prober.DidSucceed.AfterReportedFailure.Litepages", true, 1);
}

class PreviewsLitePageRedirectServerBadServerBrowserTest
    : public PreviewsLitePageRedirectServerBrowserTest {
 public:
  PreviewsLitePageRedirectServerBadServerBrowserTest() = default;

  ~PreviewsLitePageRedirectServerBadServerBrowserTest() override = default;

  // Override the previews_server URL so that a bad value will be configured.
  GURL previews_server_url() const override {
    return GURL("https://bad-server.com");
  }
};

// Param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectServerBadServerBrowserTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(PreviewsLitePageRedirectServerBadServerBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsBadServer)) {
  // TODO(crbug.com/874150): Use ExpectUniqueSample in this tests.
  // The histograms in this tests can only be checked by the expected bucket,
  // and not by a unique sample. This is because each navigation to a preview
  // will cause two navigations and two records, one for the original navigation
  // under test, and another one for loading the preview.

  {
    // Verify the preview is not shown on a bad previews server because it was
    // never triggered due to a failed probe.
    base::HistogramTester histogram_tester;
    ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
    VerifyPreviewNotLoaded();
    histogram_tester.ExpectBucketCount(
        "Previews.ServerLitePage.IneligibleReasons",
        previews::LitePageRedirectIneligibleReason::kServiceProbeFailed, 1);
  }
}

class PreviewsLitePageRedirectServerDataSaverBrowserTest
    : public PreviewsLitePageRedirectServerBrowserTest {
 public:
  PreviewsLitePageRedirectServerDataSaverBrowserTest() = default;

  ~PreviewsLitePageRedirectServerDataSaverBrowserTest() override = default;

  // Overrides the cmd line in PreviewsLitePageRedirectServerBrowserTest and
  // leave out the flag to enable DataSaver.
  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitch("ignore-litepage-redirect-optimization-blacklist");
  }
};

// Param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectServerDataSaverBrowserTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerDataSaverBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsDSTriggering)) {
  // Verify the preview is not triggered on HTTPS pageloads without DataSaver.
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();
}

class PreviewsLitePageRedirectServerNoDataSaverHeaderBrowserTest
    : public PreviewsLitePageRedirectServerBrowserTest {
 public:
  PreviewsLitePageRedirectServerNoDataSaverHeaderBrowserTest() = default;

  ~PreviewsLitePageRedirectServerNoDataSaverHeaderBrowserTest() override =
      default;

  // Overrides the command line in PreviewsLitePageRedirectServerBrowserTest to
  // leave out the flag that manually adds the chrome-proxy header.
  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitch("enable-spdy-proxy-auth");
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitch("ignore-litepage-redirect-optimization-blacklist");
  }
};

// Param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectServerNoDataSaverHeaderBrowserTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerNoDataSaverHeaderBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsDSNoHeaderTriggering)) {
  // Verify the preview is not triggered on HTTPS pageloads without data saver.
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();
}

class PreviewsLitePageRedirectNotificationDSEnabledBrowserTest
    : public PreviewsLitePageRedirectServerBrowserTest {
 public:
  PreviewsLitePageRedirectNotificationDSEnabledBrowserTest() = default;

  ~PreviewsLitePageRedirectNotificationDSEnabledBrowserTest() override =
      default;

  void SetUp() override {
    SetUpLitePageTest(false /* use_timeout */, false /* is_control */);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    InitializeOptimizationHints();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);

    WaitForServerProbe();
  }
};

// Pecond param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectNotificationDSEnabledBrowserTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectNotificationDSEnabledBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsInfoBarDataSaverUser)) {
  // Ensure the preview is not shown the first time before the infobar is shown
  // for users who have DRP enabled.
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));

  VerifyPreviewNotLoaded();
  ClearDeciderState();
  ASSERT_EQ(1U, GetInfoBarService()->infobar_count());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_LITE_PAGE_PREVIEWS_MESSAGE),
            static_cast<ConfirmInfoBarDelegate*>(
                GetInfoBarService()->infobar_at(0)->delegate())
                ->GetMessageText());
  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.IneligibleReasons",
      previews::LitePageRedirectIneligibleReason::kInfoBarNotSeen, 1);
}

class PreviewsLitePageRedirectDSDisabledBrowserTest
    : public PreviewsLitePageRedirectServerBrowserTest {
 public:
  PreviewsLitePageRedirectDSDisabledBrowserTest() = default;

  ~PreviewsLitePageRedirectDSDisabledBrowserTest() override = default;

  void SetUp() override {
    SetUpLitePageTest(false /* use_timeout */, false /* is_control */);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);
  }

  // Overrides the cmd line in PreviewsLitePageRedirectServerBrowserTest and
  // leave out the flag to enable DataSaver.
  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitchASCII("force-effective-connection-type", "Slow-2G");
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitch("ignore-litepage-redirect-optimization-blacklist");
  }
};

// Param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectDSDisabledBrowserTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectDSDisabledBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsInfoBarNonDataSaverUser)) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();
  EXPECT_EQ(0U, GetInfoBarService()->infobar_count());
  histogram_tester.ExpectTotalCount(
      "Previews.ServerLitePage.PreresolvedToPreviewServer", 0);
}

class PreviewsLitePageRedirectControlBrowserTest
    : public PreviewsLitePageRedirectServerBrowserTest {
 public:
  PreviewsLitePageRedirectControlBrowserTest() = default;

  ~PreviewsLitePageRedirectControlBrowserTest() override = default;

  void SetUp() override {
    SetUpLitePageTest(false /* use_timeout */, true /* is_control */);

    InProcessBrowserTest::SetUp();
  }
};

// Param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectControlBrowserTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectControlBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsControlGroup)) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewNotLoaded();
  ClearDeciderState();
}

enum class NetworkIsolationKeyMode {
  kNone,
  kTopFrameOrigin,
  kTopFrameAndFrameOrigins,
};

class PreviewsLitePageRedirectServerNetworkIsolationBrowserTest
    : public BasePreviewsLitePageRedirectServerBrowserTest,
      public testing::WithParamInterface<NetworkIsolationKeyMode> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case NetworkIsolationKeyMode::kNone:
        break;
      case NetworkIsolationKeyMode::kTopFrameOrigin:
        scoped_feature_list_.InitWithFeatures(
            // enabled_features
            {net::features::kPartitionConnectionsByNetworkIsolationKey,
             // While these tests are focusing on partitioning the socket pools,
             // some depend on cache behavior, and it would be
             // unfortunate if splitting the cache by the key as well broke
             // them.
             net::features::kSplitCacheByNetworkIsolationKey},
            // disabled_features
            {net::features::kAppendFrameOriginToNetworkIsolationKey});
        break;
      case NetworkIsolationKeyMode::kTopFrameAndFrameOrigins:
        scoped_feature_list_.InitWithFeatures(
            // enabled_features
            {net::features::kPartitionConnectionsByNetworkIsolationKey,
             net::features::kSplitCacheByNetworkIsolationKey,
             net::features::kAppendFrameOriginToNetworkIsolationKey},
            // disabled_features
            {});
        break;
    }

    BasePreviewsLitePageRedirectServerBrowserTest::SetUp();
  }

  bool ShouldEnableDRPHoldback() const override { return false; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectServerNetworkIsolationBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PreconnectToPreviewsServer)) {
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), https_media_url());
  VerifyPreviewNotLoaded();

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PredictorToggled", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.PreconnectedToPreviewServer", true, 1);

  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
  WaitForInterventionReport();

  // Proving which socket is reused for which purpose is tricky in a browsertest
  // because everything happens so quickly. Instead, just prove by
  // pigeon-holing. These connections are sent to server, and we expect at least
  // one to be reused:
  // * Startup probe
  // * Preconnect
  // * Main page request
  // * Favicon request
  // * Browser intervention report
  EXPECT_LT(previews_server_connections(), 5);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectServerNetworkIsolationBrowserTest,
    ::testing::Values(NetworkIsolationKeyMode::kNone,
                      NetworkIsolationKeyMode::kTopFrameOrigin,
                      NetworkIsolationKeyMode::kTopFrameAndFrameOrigins));

class PreviewsLitePageRedirectAndPageHintsBrowserTest
    : public PreviewsLitePageRedirectServerBrowserTest {
 public:
  PreviewsLitePageRedirectAndPageHintsBrowserTest() = default;

  ~PreviewsLitePageRedirectAndPageHintsBrowserTest() override = default;

  void SetResourceLoadingHints(const std::vector<std::string>& hints_sites) {
    std::vector<std::string> resource_patterns;
    resource_patterns.push_back("foo.jpg");
    resource_patterns.push_back("png");
    resource_patterns.push_back("woff2");

    ProcessHintsComponent(
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::RESOURCE_LOADING, hints_sites, "*",
            resource_patterns));
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    PreviewsLitePageRedirectServerBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitch("optimization-guide-disable-installer");
    cmd->AppendSwitch("purge_hint_cache_store");
  }

  void WriteConfigToFile(const optimization_guide::proto::Configuration& config,
                         const base::FilePath& filePath) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_EQ(static_cast<int32_t>(serialized_config.length()),
              base::WriteFile(filePath, serialized_config.data(),
                              serialized_config.length()));
  }
};

// Param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PreviewsLitePageRedirectAndPageHintsBrowserTest,
    ::testing::Bool());

// Regression test for crbug.com/954554.
IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectAndPageHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PreviewsServerIsInBloomFilter)) {
  optimization_guide::BloomFilter blacklist_bloom_filter(7, 511);
  blacklist_bloom_filter.Add(previews_server_url().host());
  blacklist_bloom_filter.Add("subdomain." + previews_server_url().host());

  std::string blacklist_data((char*)&blacklist_bloom_filter.bytes()[0],
                             blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config.add_optimization_blacklists();
  blacklist_proto->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(7);
  bloom_filter_proto->set_num_bits(511);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  optimization_guide::HintsComponentInfo info(
      base::Version("2.0.0"),
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("somefile.pb")));
  ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));
  ProcessHintsComponent(info);

  ui_test_utils::NavigateToURL(browser(), HttpsLitePageURL(kSuccess));
  VerifyPreviewLoaded();
}

IN_PROC_BROWSER_TEST_P(
    PreviewsLitePageRedirectAndPageHintsBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(LitePagePreviewsDoesNotOverridePageHints)) {
  base::HistogramTester histogram_tester;

  // Whitelist test URL for resource loading hints.
  GURL url = HttpsLitePageURL(kSuccess);
  SetResourceLoadingHints({url.host()});

  ui_test_utils::NavigateToURL(browser(), url);

  base::RunLoop().RunUntilIdle();
  content::WaitForLoadStop(GetWebContents());

  // Verify the committed previews type is resource loading hints.
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(GetWebContents());
  EXPECT_TRUE(ui_tab_helper->displayed_preview_ui());
  previews::PreviewsUserData* previews_data =
      ui_tab_helper->previews_user_data();
  EXPECT_TRUE(previews_data->HasCommittedPreviewsType());
  EXPECT_EQ(previews_data->CommittedPreviewsType(),
            previews::PreviewsType::RESOURCE_LOADING_HINTS);

  ClearDeciderState();
}

class CoinFlipHoldbackExperimentBrowserTest
    : public PreviewsLitePageRedirectAndPageHintsBrowserTest {
 public:
  CoinFlipHoldbackExperimentBrowserTest() {
    ukm_feature_list_.InitAndEnableFeature(ukm::kUkmFeature);
  }

  ~CoinFlipHoldbackExperimentBrowserTest() override = default;

  void SetUp() override {
    PreviewsLitePageRedirectAndPageHintsBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    PreviewsLitePageRedirectAndPageHintsBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitch("litepage_redirect_overrides_page_hints");
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // |redirect_navigation|: Whether a preview should only be eligible on a
  // redirect.
  //
  // |allow_lite_page_redirect|: Whether a lite page redirect preview should be
  // eligible.
  //
  // |allow_resource_loading|: Whether a resource loading preview should be
  // eligible.
  void RunTest(bool redirect_navigation,
               bool allow_lite_page_redirect,
               bool allow_resource_loading) {
    ukm::InitializeSourceUrlRecorderForWebContents(GetWebContents());

    GURL final_url = blacklisted_https_url();
    GURL starting_url = redirect_navigation
                            ? HttpsLitePageURL(kRedirectNonPreview)
                            : blacklisted_https_url();

    if (allow_lite_page_redirect) {
      final_url = HttpsLitePageURL(kSuccess);
      starting_url = redirect_navigation ? http_to_https_redirect_url()
                                         : HttpsLitePageURL(kSuccess);
    }

    if (allow_resource_loading)
      SetResourceLoadingHints({final_url.host()});

    ASSERT_EQ(redirect_navigation, starting_url != final_url);

    ui_test_utils::NavigateToURL(browser(), starting_url);
    base::RunLoop().RunUntilIdle();
    content::WaitForLoadStop(GetWebContents());
  }

  void ValidateResult(bool want_lite_page_redirect_committed,
                      bool want_resource_loading_committed,
                      int want_ukm_coin_flip_holdback_result,
                      bool want_ukm_previews_likely) {
    PreviewsUITabHelper* ui_tab_helper =
        PreviewsUITabHelper::FromWebContents(GetWebContents());
    EXPECT_EQ(
        want_lite_page_redirect_committed || want_resource_loading_committed,
        ui_tab_helper->displayed_preview_ui());
    previews::PreviewsUserData* previews_data =
        ui_tab_helper->previews_user_data();

    if (want_lite_page_redirect_committed) {
      VerifyPreviewLoaded();
      EXPECT_NE(previews_data->coin_flip_holdback_result(),
                previews::CoinFlipHoldbackResult::kHoldback);
    }

    if (want_resource_loading_committed) {
      EXPECT_EQ(previews_data->CommittedPreviewsType(),
                previews::PreviewsType::RESOURCE_LOADING_HINTS);
    }

    EXPECT_EQ(want_ukm_coin_flip_holdback_result,
              static_cast<int>(previews_data->coin_flip_holdback_result()));

    ValidateUKM(want_lite_page_redirect_committed,
                want_resource_loading_committed,
                want_ukm_coin_flip_holdback_result, want_ukm_previews_likely);
  }

  void ValidateUKM(bool want_lite_page_redirect_committed,
                   bool want_resource_loading_committed,
                   int want_ukm_coin_flip_holdback_result,
                   bool want_ukm_previews_likely) {
    if (!want_lite_page_redirect_committed &&
        !want_resource_loading_committed) {
      return;
    }

    if (want_ukm_coin_flip_holdback_result ==
        static_cast<int>(previews::CoinFlipHoldbackResult::kNotSet)) {
      return;
    }

    {
      using UkmEntry = ukm::builders::PreviewsCoinFlip;
      auto entries = ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName);
      // EXPECT_EQ(1u, entries.size());
      for (auto* entry : entries) {
        ukm_recorder_->ExpectEntryMetric(entry, UkmEntry::kcoin_flip_resultName,
                                         want_ukm_coin_flip_holdback_result);
      }
    }

    // Navigate to a non-preview page to trigger the UKM PLM Observer to
    // record.
    ui_test_utils::NavigateToURL(browser(), GURL("http://nopreviews"));
    base::RunLoop().RunUntilIdle();
    content::WaitForLoadStop(GetWebContents());

    {
      using UkmEntry = ukm::builders::Previews;
      auto entries = ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName);
      EXPECT_EQ(1u, entries.size());
      for (auto* entry : entries) {
        ukm_recorder_->ExpectEntryMetric(entry, UkmEntry::kpreviews_likelyName,
                                         want_ukm_previews_likely ? 1 : 0);
      }
    }
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  base::test::ScopedFeatureList ukm_feature_list_;
};

class CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip
    : public CoinFlipHoldbackExperimentBrowserTest {
 public:
  CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip() {
    feature_list_.InitAndEnableFeatureWithParameters(
        previews::features::kCoinFlipHoldback,
        {{"force_coin_flip_always_holdback", "true"},
         {"force_coin_flip_always_allow", "false"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip
    : public CoinFlipHoldbackExperimentBrowserTest {
 public:
  CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip() {
    feature_list_.InitAndEnableFeatureWithParameters(
        previews::features::kCoinFlipHoldback,
        {{"force_coin_flip_always_holdback", "false"},
         {"force_coin_flip_always_allow", "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled
    : public CoinFlipHoldbackExperimentBrowserTest {
 public:
  CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled() {
    feature_list_.InitAndDisableFeature(previews::features::kCoinFlipHoldback);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Param is true if DRP holdback should be enabled.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CoinFlipHoldbackExperimentBrowserTest,
    ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip,
    ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip,
    ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoPreviews_NoCoinFlip)) {
  // Set ECT so that we are sure to not trigger any preview.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);

  RunTest(false /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          false /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 false /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled,
    DISABLE_ON_WIN_MAC_CHROMEOS(BothPreviewsAllowedWantLPR_NoCoinFlip)) {
  RunTest(false /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(true /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled,
    DISABLE_ON_WIN_MAC_CHROMEOS(LPRAllowed_NoCoinFlip)) {
  RunTest(false /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          false /* allow_resource_loading*/);

  ValidateResult(true /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled,
    DISABLE_ON_WIN_MAC_CHROMEOS(RLHAllowed_NoCoinFlip)) {
  RunTest(false /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 true /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoPreviews_WithRedirect_NoCoinFlip)) {
  // Set ECT so that we are sure to not trigger any preview.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);

  RunTest(true /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          false /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 false /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        BothPreviewsAllowedWantLPR_WithRedirect_NoCoinFlip)) {
  RunTest(true /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(true /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled,
    DISABLE_ON_WIN_MAC_CHROMEOS(LPRAllowed_WithRedirect_NoCoinFlip)) {
  RunTest(true /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          false /* allow_resource_loading */);

  ValidateResult(true /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithCoinFlipHoldbackDisabled,
    DISABLE_ON_WIN_MAC_CHROMEOS(RLHAllowed_WithRedirect_NoCoinFlip)) {
  RunTest(true /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 true /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoPreviews_CoinFlipEnabled_Allowed)) {
  // Set ECT so that we are sure to not trigger any preview.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);

  RunTest(false /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          false /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 false /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        BothPreviewsAllowedWantLPR_CoinFlipEnabled_Allowed)) {
  RunTest(false /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(true /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 1 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(LPRAllowed_CoinFlipEnabled_Allowed)) {
  RunTest(false /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          false /* allow_resource_loading*/);

  ValidateResult(true /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 1 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(RLHAllowed_CoinFlipEnabled_Allowed)) {
  RunTest(false /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 true /* want_resource_loading_committed */,
                 1 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        NoPreviews_WithRedirect_CoinFlipEnabled_Allowed)) {
  // Set ECT so that we are sure to not trigger any preview.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);

  RunTest(true /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          false /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 false /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        BothPreviewsAllowedWantLPR_WithRedirect_CoinFlipEnabled_Allowed)) {
  RunTest(true /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(true /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 1 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        LPRAllowed_WithRedirect_CoinFlipEnabled_Allowed)) {
  RunTest(true /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          false /* allow_resource_loading */);

  ValidateResult(true /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 1 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithoutRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        RLHAllowed_WithRedirect_CoinFlipEnabled_Allowed)) {
  RunTest(true /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 true /* want_resource_loading_committed */,
                 1 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoPreviews_CoinFlipEnabled_Holdback)) {
  // Set ECT so that we are sure to not trigger any preview.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);

  RunTest(false /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          false /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 0 /* want_ukm_coin_flip_holdback_result */,
                 false /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        BothPreviewsAllowedWantLPR_CoinFlipEnabled_Holdback)) {
  RunTest(false /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 2 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(LPRAllowed_CoinFlipEnabled_Holdback)) {
  RunTest(false /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          false /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 2 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(RLHAllowed_CoinFlipEnabled_Holdback)) {
  RunTest(false /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 2 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        BothPreviewsAllowedWantLPR_WithRedirect_CoinFlipEnabled_Holdback)) {
  RunTest(true /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 2 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        LPRAllowed_WithRedirect_CoinFlipEnabled_Holdback)) {
  RunTest(true /* redirect_navigation*/, true /* allow_lite_page_redirect*/,
          false /* allow_resource_loading */);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 2 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}

IN_PROC_BROWSER_TEST_P(
    CoinFlipHoldbackExperimentBrowserTestWithRandomNavigationCoinFlip,
    DISABLE_ON_WIN_MAC_CHROMEOS(
        RLHAllowed_WithRedirect_CoinFlipEnabled_Holdback)) {
  RunTest(true /* redirect_navigation*/, false /* allow_lite_page_redirect*/,
          true /* allow_resource_loading*/);

  ValidateResult(false /* want_lite_page_redirect_committed */,
                 false /* want_resource_loading_committed */,
                 2 /* want_ukm_coin_flip_holdback_result */,
                 true /* want_ukm_previews_likely */);
}
