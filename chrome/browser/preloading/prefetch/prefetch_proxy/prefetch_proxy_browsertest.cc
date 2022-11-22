// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <set>
#include <string>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_test_utils.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_canary_checker.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_origin_prober.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_status.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_proxy_configurator.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service_factory.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_subresource_manager.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_tab_helper.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_test_utils.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_url_loader_interceptor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/certificate_reporting_test_utils.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/language/core/browser/pref_names.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_blocking_page_base.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_state/core/security_state.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/variations_params_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/page_type.h"
#include "content/public/common/user_agent.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/proxy_string_util.h"
#include "net/base/url_util.h"
#include "net/cert/cert_database.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_util.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char kAllowedUAClientHint[] = "sec-ch-ua";
const char kAllowedUAMobileClientHint[] = "sec-ch-ua-mobile";
const char kAllowedUAPlatformClientHint[] = "sec-ch-ua-platform";

class TestCustomProxyConfigClient
    : public network::mojom::CustomProxyConfigClient {
 public:
  explicit TestCustomProxyConfigClient(
      mojo::PendingReceiver<network::mojom::CustomProxyConfigClient>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  // network::mojom::CustomProxyConfigClient:
  void OnCustomProxyConfigUpdated(
      network::mojom::CustomProxyConfigPtr proxy_config,
      OnCustomProxyConfigUpdatedCallback callback) override {
    config_ = std::move(proxy_config);
    std::move(callback).Run();
  }
  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override {}
  void ClearBadProxiesCache() override {}

  network::mojom::CustomProxyConfigPtr config_;

 private:
  mojo::Receiver<network::mojom::CustomProxyConfigClient> receiver_;
};

class AuthChallengeObserver : public content::NotificationObserver {
 public:
  explicit AuthChallengeObserver(content::WebContents* web_contents) {
    registrar_.Add(this, chrome::NOTIFICATION_AUTH_NEEDED,
                   content::Source<content::NavigationController>(
                       &web_contents->GetController()));
  }
  ~AuthChallengeObserver() override = default;

  bool GotAuthChallenge() const { return got_auth_challenge_; }

  void Reset() { got_auth_challenge_ = false; }

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    got_auth_challenge_ |= type == chrome::NOTIFICATION_AUTH_NEEDED;
  }

 private:
  content::NotificationRegistrar registrar_;
  bool got_auth_challenge_ = false;
};

// Runs a closure when all expected URLs have been fetched successfully.
class TestTabHelperObserver : public PrefetchProxyTabHelper::Observer {
 public:
  explicit TestTabHelperObserver(PrefetchProxyTabHelper* tab_helper)
      : tab_helper_(tab_helper) {
    tab_helper_->AddObserverForTesting(this);
  }
  ~TestTabHelperObserver() { tab_helper_->RemoveObserverForTesting(this); }

  void SetDecoyPrefetchClosure(base::OnceClosure closure) {
    on_decoy_prefetch_closure_ = std::move(closure);
  }

  void SetOnPrefetchSuccessfulClosure(base::OnceClosure closure) {
    on_successful_prefetch_closure_ = std::move(closure);
  }

  void SetOnPrefetchErrorClosure(base::OnceClosure closure) {
    on_prefetch_error_closure_ = std::move(closure);
  }

  void SetExpectedSuccessfulURLs(const std::set<GURL>& expected_urls) {
    expected_successful_prefetch_urls_ = expected_urls;
  }

  void SetExpectedPrefetchErrors(
      const std::set<std::pair<GURL, int>> expected_prefetch_errors) {
    expected_prefetch_errors_ = expected_prefetch_errors;
  }

  void SetOnNSPFinishedClosure(base::OnceClosure closure) {
    on_nsp_finished_closure_ = std::move(closure);
  }

  void SetOnCookiesChangedForPrefetchAfterInitialCheckClosure(
      base::OnceClosure closure) {
    on_cookies_changed_after_initial_check_ = std::move(closure);
  }

  void SetExpectedURLCookiesChangedForPrefetchAfterInitialCheck(
      const GURL& url) {
    expected_url_cookies_changed_after_initial_check_ = url;
  }

  // PrefetchProxyTabHelper::Observer:
  void OnDecoyPrefetchCompleted(const GURL& url) override {
    if (on_decoy_prefetch_closure_) {
      std::move(on_decoy_prefetch_closure_).Run();
    }
  }

  void OnPrefetchCompletedSuccessfully(const GURL& url) override {
    auto it = expected_successful_prefetch_urls_.find(url);
    if (it != expected_successful_prefetch_urls_.end()) {
      expected_successful_prefetch_urls_.erase(it);
    }

    if (!expected_successful_prefetch_urls_.empty())
      return;

    if (!on_successful_prefetch_closure_)
      return;

    std::move(on_successful_prefetch_closure_).Run();
  }

  void OnPrefetchCompletedWithError(const GURL& url, int error_code) override {
    std::pair<GURL, int> error_pair = {url, error_code};
    auto it = expected_prefetch_errors_.find(error_pair);
    if (it != expected_prefetch_errors_.end()) {
      expected_prefetch_errors_.erase(it);
    }

    if (!expected_prefetch_errors_.empty())
      return;

    if (!on_prefetch_error_closure_)
      return;

    std::move(on_prefetch_error_closure_).Run();
  }

  void OnNoStatePrefetchFinished() override {
    if (on_nsp_finished_closure_) {
      std::move(on_nsp_finished_closure_).Run();
    }
  }

  void OnCookiesChangedForPrefetchAfterInitialCheck(const GURL& url) override {
    if (url != expected_url_cookies_changed_after_initial_check_)
      return;

    if (!on_cookies_changed_after_initial_check_)
      return;

    std::move(on_cookies_changed_after_initial_check_).Run();
  }

 private:
  raw_ptr<PrefetchProxyTabHelper> tab_helper_;

  base::OnceClosure on_decoy_prefetch_closure_;

  base::OnceClosure on_successful_prefetch_closure_;
  std::set<GURL> expected_successful_prefetch_urls_;

  base::OnceClosure on_prefetch_error_closure_;
  std::set<std::pair<GURL, int>> expected_prefetch_errors_;

  base::OnceClosure on_nsp_finished_closure_;

  base::OnceClosure on_cookies_changed_after_initial_check_;
  GURL expected_url_cookies_changed_after_initial_check_;
};

// A stub ClientCertStore that returns a FakeClientCertIdentity.
class ClientCertStoreStub : public net::ClientCertStore {
 public:
  explicit ClientCertStoreStub(net::ClientCertIdentityList list)
      : list_(std::move(list)) {}

  ~ClientCertStoreStub() override = default;

  // net::ClientCertStore:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(list_));
  }

 private:
  net::ClientCertIdentityList list_;
};

std::unique_ptr<net::ClientCertStore> CreateCertStore() {
  base::FilePath certs_dir = net::GetTestCertsDirectory();

  net::ClientCertIdentityList cert_identity_list;

  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::unique_ptr<net::FakeClientCertIdentity> cert_identity =
        net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
            certs_dir, "client_1.pem", "client_1.pk8");
    EXPECT_TRUE(cert_identity.get());
    if (cert_identity)
      cert_identity_list.push_back(std::move(cert_identity));
  }

  return std::unique_ptr<net::ClientCertStore>(
      new ClientCertStoreStub(std::move(cert_identity_list)));
}

class CustomProbeOverrideDelegate
    : public PrefetchProxyOriginProber::ProbeURLOverrideDelegate {
 public:
  explicit CustomProbeOverrideDelegate(const GURL& override_url)
      : url_(override_url) {}
  ~CustomProbeOverrideDelegate() = default;

  GURL OverrideProbeURL(const GURL& url) override { return url_; }

 private:
  GURL url_;
};

class TestServerConnectionCounter
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  TestServerConnectionCounter() = default;
  ~TestServerConnectionCounter() override = default;

  size_t count() const { return count_; }

 private:
  void ReadFromSocket(const net::StreamSocket& connection, int rv) override {}
  std::unique_ptr<net::StreamSocket> AcceptedSocket(
      std::unique_ptr<net::StreamSocket> socket) override {
    count_++;
    return socket;
  }

  size_t count_ = 0;
};

// Reading the output of |testing::UnorderedElementsAreArray| is impossible.
std::string ActualHumanReadableMetricsToDebugString(
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> entries) {
  std::string result = "Actual Entries:\n";

  if (entries.empty()) {
    result = "<empty>";
  }

  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    result += base::StringPrintf("=== Entry #%zu\n", i);
    result += base::StringPrintf("Source ID: %d\n",
                                 static_cast<int>(entry.source_id));
    for (const auto& metric : entry.metrics) {
      result += base::StringPrintf("Metric '%s' = %d\n", metric.first.c_str(),
                                   static_cast<int>(metric.second));
    }
    result += "\n";
  }
  result += "\n";
  return result;
}

std::vector<testing::Matcher<ukm::TestUkmRecorder::HumanReadableUkmEntry>>
BuildPrefetchResourceMatchers(
    const std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>& entries) {
  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  auto matchers = std::vector<testing::Matcher<UkmEntry>>{};

  for (const auto& entry : entries) {
    auto source_id_matcher =
        testing::Field(&ukm::TestUkmRecorder::HumanReadableUkmEntry::source_id,
                       entry.source_id);

    auto metrics_pairs =
        std::vector<testing::Matcher<std::pair<std::string, int64_t>>>{};
    for (const auto& metric : entry.metrics) {
      std::string name = metric.first;
      int64_t value = metric.second;

      if (name == "DataLength" || name == "FetchDurationMS" ||
          name == "NavigationStartToFetchStartMS") {
        // This matcher only needs to check for a positive value since checking
        // the exact value will be flaky.
        metrics_pairs.push_back(testing::Pair(name, testing::Gt(0L)));
      } else if (name == "ISPFilteringStatus") {
        // Treat TLS Success and DNS Success as the same since the exact check
        // done is flaky in tests. No probe should always match.
        if (value == 0) {
          metrics_pairs.push_back(testing::Pair(name, 0));
        } else if (value == 2 || value == 4) {
          metrics_pairs.push_back(testing::Pair(name, testing::AnyOf(2, 4)));
        } else if (value == 1 || value == 3) {
          metrics_pairs.push_back(testing::Pair(name, testing::AnyOf(1, 3)));
        } else {
          NOTREACHED();
        }
      } else {
        metrics_pairs.push_back(testing::Pair(name, value));
      }
    }

    matchers.push_back(testing::AllOf(
        source_id_matcher,
        testing::Field(
            &UkmEntry::metrics,
            testing::WhenSorted(testing::ElementsAreArray(metrics_pairs)))));
  }
  return matchers;
}

}  // namespace

// Occasional flakes on Windows (https://crbug.com/1045971).
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

class PrefetchProxyBrowserTest
    : public InProcessBrowserTest,
      public prerender::NoStatePrefetchHandle::Observer,
      public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  PrefetchProxyBrowserTest() {
    origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    origin_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    origin_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    origin_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    origin_server_->RegisterRequestHandler(
        base::BindRepeating(&PrefetchProxyBrowserTest::HandleOriginRequest,
                            base::Unretained(this)));
    EXPECT_TRUE(origin_server_->Start());

    referring_page_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    referring_page_server_->SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    referring_page_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    referring_page_server_->SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    EXPECT_TRUE(referring_page_server_->Start());

    proxy_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    proxy_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    proxy_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    proxy_server_->RegisterRequestHandler(base::BindRepeating(
        &PrefetchProxyBrowserTest::HandleProxyRequest, base::Unretained(this)));
    proxy_server_->SetConnectionListener(this);
    EXPECT_TRUE(proxy_server_->Start());

    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    EXPECT_TRUE(http_server_->Start());
  }

  void SetUp() override {
    SetFeatures();
    InProcessBrowserTest::SetUp();
  }

  // This browsertest uses a separate method to handle enabling/disabling
  // features since order is tricky when doing different feature lists between
  // base and derived classes.
  virtual void SetFeatures() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"use_speculation_rules", "false"}, {"max_srp_prefetches", "1"}});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // So that we can test for client hints.
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    // Ensure the service gets created before the tests start.
    PrefetchProxyService* service =
        PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
    // Override the system's connection type to avoid flakes due to changing
    // connection type during testing.
    network_connection_tracker_ =
        network::TestNetworkConnectionTracker::CreateInstance();
    network_connection_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_3G);
    PrefetchProxyCanaryChecker* dns_checker =
        service->origin_prober()->GetDNSCanaryCheckerForTesting();
    if (dns_checker) {
      dns_checker->SetNetworkConnectionTrackerForTesting(
          network_connection_tracker_.get());
    }
    PrefetchProxyCanaryChecker* tls_checker =
        service->origin_prober()->GetTLSCanaryCheckerForTesting();
    if (tls_checker) {
      tls_checker->SetNetworkConnectionTrackerForTesting(
          network_connection_tracker_.get());
    }

    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("proxy.a.test", "127.0.0.1");
    host_resolver()->AddRule("insecure.com", "127.0.0.1");
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
    host_resolver()->AddRule("resolve-success.com", "127.0.0.1");

    host_resolver()->AddSimulatedFailure("resolve-fail.com");

    PrefetchProxyTabHelper::SetHostNonUniqueFilterForTest(
        [](base::StringPiece host) {
          // Treat *.test as real public hostnames, even though they aren't.
          return net::IsHostnameNonUnique(std::string{host}) &&
                 !net::IsSubdomainOf(host, "test");
        });
  }

  void TearDownOnMainThread() override {
    PrefetchProxyTabHelper::ResetHostNonUniqueFilterForTest();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("prefetch-proxy-never-send-decoy-requests-for-testing");
    // For the proxy.
    cmd->AppendSwitch("ignore-certificate-errors");
    cmd->AppendSwitchASCII("isolated-prerender-tunnel-proxy",
                           GetProxyURL().spec());
    cmd->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                           "SpeculationRulesPrefetchProxy,"
                           "SpeculationRulesPrefetchWithSubresources");
  }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void MakeNavigationPrediction(const GURL& doc_url,
                                const std::vector<GURL>& predicted_urls) {
    NavigationPredictorKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OnPredictionUpdated(
            GetWebContents(), doc_url,
            NavigationPredictorKeyedService::PredictionSource::
                kAnchorElementsParsedFromWebPage,
            predicted_urls);
  }

  void InsertSpeculation(bool subresources,
                         bool use_prefetch_proxy,
                         const std::vector<GURL>& prefetch_urls) {
    std::string speculation_script = R"(
      var script = document.createElement('script');
      script.type = 'speculationrules';
      script.text = `{)";
    if (subresources) {
      speculation_script.append(R"("prefetch_with_subresources": [{)");
    } else {
      speculation_script.append(R"("prefetch": [{)");
    }
    speculation_script.append(R"("source": "list",
          "urls": [)");

    bool first = true;
    for (const GURL& url : prefetch_urls) {
      if (!first)
        speculation_script.append(",");
      first = false;
      speculation_script.append("\"").append(url.spec()).append("\"");
    }
    speculation_script.append("]");

    if (use_prefetch_proxy)
      speculation_script.append(R"(,
          "requires": ["anonymous-client-ip-when-cross-origin"])");
    speculation_script.append(R"(
        }]
      }`;
      document.head.appendChild(script);)");

    EXPECT_TRUE(ExecuteScript(GetWebContents(), speculation_script));
  }

  network::mojom::CustomProxyConfigPtr WaitForUpdatedCustomProxyConfig() {
    PrefetchProxyService* prefetch_proxy_service =
        PrefetchProxyServiceFactory::GetForProfile(browser()->profile());

    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CustomProxyConfigClient> client_remote;
    TestCustomProxyConfigClient config_client(
        client_remote.BindNewPipeAndPassReceiver());
    prefetch_proxy_service->proxy_configurator()->AddCustomProxyConfigClient(
        std::move(client_remote), run_loop.QuitClosure());
    run_loop.Run();

    return std::move(config_client.config_);
  }

  bool RequestHasClientHints(const net::test_server::HttpRequest& request) {
    for (const auto& elem : network::GetClientHintToNameMap()) {
      const auto& header = elem.second;
      // The UA {mobile} Client Hint is whitelisted so we don't check it.
      if (header == std::string(kAllowedUAClientHint)) {
        continue;
      }

      if (header == std::string(kAllowedUAMobileClientHint)) {
        continue;
      }

      if (header == std::string(kAllowedUAPlatformClientHint)) {
        continue;
      }

      if (base::Contains(request.headers, header)) {
        LOG(WARNING) << "request has " << header;

        return true;
      }
    }
    return false;
  }

  void VerifyProxyConfig(network::mojom::CustomProxyConfigPtr config,
                         bool want_empty = false) {
    ASSERT_TRUE(config);

    EXPECT_EQ(config->rules.type,
              net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME);
    EXPECT_FALSE(config->should_override_existing_config);
    EXPECT_FALSE(config->allow_non_idempotent_methods);

    if (want_empty) {
      EXPECT_EQ(config->rules.proxies_for_https.size(), 0U);
    } else {
      ASSERT_EQ(config->rules.proxies_for_https.size(), 1U);
      EXPECT_EQ(GURL(net::ProxyServerToProxyUri(
                    config->rules.proxies_for_https.Get())),
                GetProxyURL());
    }
  }

  bool CheckForResourceInIsolatedCache(const GURL& prefetch_url,
                                       const GURL& resource_url) {
    PrefetchProxyTabHelper* tab_helper =
        PrefetchProxyTabHelper::FromWebContents(GetWebContents());
    DCHECK(tab_helper);
    DCHECK(tab_helper->GetIsolatedContextForTesting(prefetch_url));
    return net::OK ==
           content::LoadBasicRequest(
               tab_helper->GetIsolatedContextForTesting(prefetch_url),
               resource_url, net::LOAD_ONLY_FROM_CACHE);
  }

  absl::optional<int64_t> GetUKMMetric(const GURL& url,
                                       const std::string& event_name,
                                       const std::string& metric_name) {
    SCOPED_TRACE(metric_name);

    auto entries = ukm_recorder_->GetEntriesByName(event_name);
    DCHECK_EQ(1U, entries.size());

    const auto* entry = entries.front();

    ukm_recorder_->ExpectEntrySourceHasUrl(entry, url);

    const int64_t* value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);

    if (value == nullptr) {
      return absl::nullopt;
    }
    return absl::optional<int64_t>(*value);
  }

  void VerifyNoUKMEvent(const std::string& event_name) {
    SCOPED_TRACE(event_name);

    auto entries = ukm_recorder_->GetEntriesByName(event_name);
    EXPECT_TRUE(entries.empty());
  }

  void VerifyUKMOnSRP(const GURL& url,
                      const std::string& metric_name,
                      absl::optional<int64_t> expected) {
    SCOPED_TRACE(metric_name);
    auto actual = GetUKMMetric(url, ukm::builders::PrefetchProxy::kEntryName,
                               metric_name);
    EXPECT_EQ(actual, expected);
  }

  void VerifyUKMAfterSRP(const GURL& url,
                         const std::string& metric_name,
                         absl::optional<int64_t> expected) {
    SCOPED_TRACE(metric_name);
    auto actual = GetUKMMetric(
        url, ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
        metric_name);
    EXPECT_EQ(actual, expected);
  }

  // Verifies that the entries for |ukm_source_id| match |url| and then returns
  // all the prefetched resource metrics.
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
  GetAndVerifyPrefetchedResourceUKM(const GURL& url,
                                    ukm::SourceId ukm_source_id) {
    const ukm::UkmSource* source =
        ukm_recorder_->GetSourceForSourceId(ukm_source_id);
    DCHECK(source);
    EXPECT_TRUE(base::Contains(source->urls(), url));

    return ukm_recorder_->GetEntries("PrefetchProxy.PrefetchedResource",
                                     {
                                         "DataLength",
                                         "FetchDurationMS",
                                         "ISPFilteringStatus",
                                         "LinkClicked",
                                         "LinkPosition",
                                         "NavigationStartToFetchStartMS",
                                         "ResourceType",
                                         "Status",
                                     });
  }

  // Uses the url's path to match requests since the host from
  // EmbeddedTestServer is always 127.0.0.1.
  void VerifyOriginRequestsAreIsolated(const std::set<std::string>& paths) {
    size_t verified_url_count = 0;
    for (const auto& request : origin_server_requests()) {
      const GURL& url = request.GetURL();
      if (paths.find(url.path()) == paths.end()) {
        continue;
      }

      SCOPED_TRACE(request.GetURL().spec());
      EXPECT_EQ(request.headers.find("user-agent")->second,
                content::GetReducedUserAgent(
                    base::CommandLine::ForCurrentProcess()->HasSwitch(
                        switches::kUseMobileUserAgent),
                    version_info::GetMajorVersionNumber()));
      EXPECT_EQ(request.headers.find("purpose")->second, "prefetch");
      verified_url_count++;
    }
    EXPECT_EQ(paths.size(), verified_url_count);
  }

  // Verifies that the "Sec-Purpose" header with the expected value
  // ("prefetch;anonymous-client-ip" for requests that go through the proxy, and
  // "prefetch" for non-private prefetches) was included on all requests for
  // main resources. Subresources are fetched using NSP which does not add the
  // "Sec-Purpose" header.
  void VerifyPrefetchRequestsSecPurposeHeader(
      const std::set<std::string>& main_resource_paths,
      bool are_requests_anonymous_client_ip) {
    size_t verified_header_count = 0;
    for (const auto& request : origin_server_requests()) {
      const GURL& url = request.GetURL();
      if (main_resource_paths.find(url.path()) == main_resource_paths.end()) {
        continue;
      }

      SCOPED_TRACE(request.GetURL().spec());
      EXPECT_EQ(request.headers.find("Sec-Purpose")->second,
                are_requests_anonymous_client_ip
                    ? "prefetch;anonymous-client-ip"
                    : "prefetch");
      verified_header_count++;
    }
    EXPECT_EQ(main_resource_paths.size(), verified_header_count);
  }

  size_t OriginServerRequestCount() const {
    base::RunLoop().RunUntilIdle();
    return origin_server_request_count_;
  }

  const std::vector<net::test_server::HttpRequest>& proxy_server_requests()
      const {
    return proxy_server_requests_;
  }
  const std::vector<net::test_server::HttpRequest>& origin_server_requests()
      const {
    return origin_server_requests_;
  }

  GURL GetProxyURL() const {
    return proxy_server_->GetURL("proxy.a.test", "/");
  }

  GURL GetInsecureURL(const std::string& path) {
    return http_server_->GetURL("insecure.com", path);
  }

  GURL GetLocalhostURL(const std::string& path) {
    return http_server_->GetURL("localhost", path);
  }

  GURL GetOriginServerURL(const std::string& path) const {
    return origin_server_->GetURL("a.test", path);
  }

  GURL GetReferringPageServerURL(const std::string& path) const {
    return referring_page_server_->GetURL("www.google.com", path);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleOriginRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("favicon") != std::string::npos)
      return nullptr;

    SCOPED_TRACE(request.GetURL().spec());

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PrefetchProxyBrowserTest::MonitorOriginResourceRequestOnUIThread,
            base::Unretained(this), request));

    if (request.relative_url == "/auth_challenge") {
      std::unique_ptr<net::test_server::BasicHttpResponse> resp =
          std::make_unique<net::test_server::BasicHttpResponse>();
      resp->set_code(net::HTTP_UNAUTHORIZED);
      resp->AddCustomHeader("www-authenticate", "Basic realm=\"test\"");
      return resp;
    }

    bool is_prefetch =
        request.headers.find("Purpose") != request.headers.end() &&
        request.headers.find("Purpose")->second == "prefetch" &&
        request.headers.find("Sec-Purpose") != request.headers.end() &&
        (request.headers.find("Sec-Purpose")->second == "prefetch" ||
         request.headers.find("Sec-Purpose")->second ==
             "prefetch;anonymous-client-ip");

    if (request.relative_url == "/404_on_prefetch") {
      std::unique_ptr<net::test_server::BasicHttpResponse> resp =
          std::make_unique<net::test_server::BasicHttpResponse>();
      resp->set_code(is_prefetch ? net::HTTP_NOT_FOUND : net::HTTP_OK);
      resp->set_content_type("text/html");
      resp->set_content("<html><body>Test</body></html>");
      return resp;
    }

    return nullptr;
  }

  void OnProxyTunnelDone(TestProxyTunnelConnection* tunnel) {
    auto iter = tunnels_.find(tunnel);
    if (iter != tunnels_.end()) {
      tunnels_.erase(iter);
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleProxyRequest(
      const net::test_server::HttpRequest& request) {
    EXPECT_EQ(request.method, net::test_server::METHOD_CONNECT);

    if (request.relative_url == "auth-challenge.test:443") {
      std::unique_ptr<net::test_server::BasicHttpResponse> resp =
          std::make_unique<net::test_server::BasicHttpResponse>();
      resp->set_code(net::HTTP_UNAUTHORIZED);
      resp->AddCustomHeader("www-authenticate", "Basic realm=\"test\"");
      return resp;
    }

    if (request.relative_url == "error.test:443") {
      std::unique_ptr<net::test_server::BasicHttpResponse> resp =
          std::make_unique<net::test_server::BasicHttpResponse>();
      resp->set_code(net::HTTP_BAD_REQUEST);
      return resp;
    }

    GURL request_origin("https://" + request.relative_url);
    EXPECT_TRUE("a.test" == request_origin.host() ||
                "b.test" == request_origin.host());

    auto iter = request.headers.find("chrome-tunnel");
    bool found_chrome_tunnel_header =
        iter != request.headers.end() &&
        base::Contains(iter->second, "key=" + google_apis::GetAPIKey());
    EXPECT_TRUE(found_chrome_tunnel_header);

    auto new_tunnel = std::make_unique<TestProxyTunnelConnection>();
    new_tunnel->SetOnDoneCallback(
        base::BindOnce(&PrefetchProxyBrowserTest::OnProxyTunnelDone,
                       base::Unretained(this), new_tunnel.get()));
    EXPECT_TRUE(new_tunnel->ConnectToPeerOnLocalhost(
        request_origin.EffectiveIntPort()));

    tunnels_.insert(std::move(new_tunnel));

    // This method is called on embedded test server thread. Post the
    // information on UI thread.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PrefetchProxyBrowserTest::MonitorProxyResourceRequestOnUIThread,
            base::Unretained(this), request));

    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    return resp;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleCanaryRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("favicon") != std::string::npos)
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    // Make sure whitespace is ok, especially trailing newline.
    resp->set_content("   OK\n");
    return resp;
  }

  void MonitorProxyResourceRequestOnUIThread(
      const net::test_server::HttpRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    proxy_server_requests_.push_back(request);
  }

  void MonitorOriginResourceRequestOnUIThread(
      const net::test_server::HttpRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    origin_server_request_count_++;
    origin_server_requests_.push_back(request);

    EXPECT_TRUE(request.headers.find("Accept-Language") !=
                request.headers.end());
    EXPECT_EQ(request.headers.find("Accept-Language")->second,
              net::HttpUtil::GenerateAcceptLanguageHeader(
                  browser()->profile()->GetPrefs()->GetString(
                      language::prefs::kAcceptLanguages)));
  }

  // prerender::NoStatePrefetchHandle::Observer:
  void OnPrefetchNetworkBytesChanged(
      prerender::NoStatePrefetchHandle* handle) override {}
  void OnPrefetchStop(prerender::NoStatePrefetchHandle* handle) override {}

  // net::test_server::EmbeddedTestServerConnectionListener:
  void ReadFromSocket(const net::StreamSocket& socket, int rv) override {}
  std::unique_ptr<net::StreamSocket> AcceptedSocket(
      std::unique_ptr<net::StreamSocket> socket) override {
    return socket;
  }
  void OnResponseCompletedSuccessfully(
      std::unique_ptr<net::StreamSocket> socket) override {
    DCHECK(socket->IsConnected());

    // Find a tunnel that isn't being used already.
    for (const auto& tunnel : tunnels_) {
      if (tunnel->IsReadyForIncomingSocket()) {
        tunnel->StartProxy(std::move(socket));
        return;
      }
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<net::EmbeddedTestServer> proxy_server_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> referring_page_server_;

  std::vector<net::test_server::HttpRequest> origin_server_requests_;
  std::vector<net::test_server::HttpRequest> proxy_server_requests_;

  // These all live on |proxy_server_|'s IO Thread.
  std::set<std::unique_ptr<TestProxyTunnelConnection>,
           base::UniquePtrComparator>
      tunnels_;

  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;

  size_t origin_server_request_count_ = 0;
};

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(ServiceWorkerRegistrationIsNotEligible)) {
  // Load a page that registers a service worker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GetOriginServerURL("/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(GetWebContents(),
                           "register('network_fallback_worker.js');"));

  content::ServiceWorkerContext* service_worker_context_ =
      browser()
          ->profile()
          ->GetDefaultStoragePartition()
          ->GetServiceWorkerContext();
  EXPECT_EQ(
      true,
      service_worker_context_->MaybeHasRegistrationForStorageKey(
          blink::StorageKey(url::Origin::Create(GetOriginServerURL("/")))));
  EXPECT_EQ(false, service_worker_context_->MaybeHasRegistrationForStorageKey(
                       blink::StorageKey(url::Origin::Create(
                           GURL("https://unregistered.com")))));

  GURL prefetch_url = GetOriginServerURL("/title2.html");

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});
  // No run loop is needed here since the service worker check is synchronous.

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 6 = |kPrefetchNotEligibleUserHasServiceWorker|
  EXPECT_EQ(absl::optional<int64_t>(6),
            GetUKMMetric(prefetch_url,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(DRPClientConfigPlumbing)) {
  auto client_config = WaitForUpdatedCustomProxyConfig();
  VerifyProxyConfig(std::move(client_config));
}

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoAuthChallenges_FromProxy)) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  std::unique_ptr<AuthChallengeObserver> auth_observer =
      std::make_unique<AuthChallengeObserver>(GetWebContents());

  // Do a positive test first to make sure we get an auth challenge under these
  // circumstances.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetOriginServerURL("/auth_challenge")));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(auth_observer->GotAuthChallenge());

  // Test that a proxy auth challenge does not show a dialog.
  auth_observer->Reset();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {GURL("https://auth-challenge.test/")});
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(auth_observer->GotAuthChallenge());
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProxyServerBackOff)) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  TestTabHelperObserver tab_helper_observer(tab_helper);
  GURL error_url("https://error.test/");

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchErrorClosure(run_loop.QuitClosure());
  tab_helper_observer.SetExpectedPrefetchErrors(
      {{error_url, net::ERR_TUNNEL_CONNECTION_FAILED}});

  base::HistogramTester histogram_tester;
  GURL doc_url("https://www.google.com/search?q=test");

  MakeNavigationPrediction(doc_url, {error_url});
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("PrefetchProxy.Proxy.RespCode", 400, 1);
  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_attempted_count_);
  EXPECT_EQ(0U, tab_helper->srp_metrics().prefetch_successful_count_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), error_url));
  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      absl::make_optional(PrefetchProxyPrefetchStatus::kPrefetchFailedNetError),
      tab_helper->after_srp_metrics()->prefetch_status_);

  // Doing this prefetch again is immediately skipped because the proxy is not
  // available.
  MakeNavigationPrediction(doc_url, {error_url});
  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(0U, tab_helper->srp_metrics().prefetch_attempted_count_);
  EXPECT_EQ(0U, tab_helper->srp_metrics().prefetch_successful_count_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), error_url));
  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(absl::make_optional(
                PrefetchProxyPrefetchStatus::kPrefetchProxyNotAvailable),
            tab_helper->after_srp_metrics()->prefetch_status_);
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(CookieOnHigherLevelDomain)) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  ASSERT_TRUE(content::SetCookie(browser()->profile(), GURL("https://foo.com"),
                                 "type=PeanutButter"));

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL prefetch_url("https://m.foo.com");
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(0U, tab_helper->srp_metrics().prefetch_eligible_count_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      absl::make_optional(
          PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies),
      tab_helper->after_srp_metrics()->prefetch_status_);
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(CookieOnOtherPath)) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  ASSERT_TRUE(content::SetCookie(browser()->profile(), GURL("https://foo.com"),
                                 "cookietype=PeanutButter;path=/cookiecookie"));

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL prefetch_url("https://foo.com/no-cookies-here");
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(0U, tab_helper->srp_metrics().prefetch_eligible_count_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      absl::make_optional(
          PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies),
      tab_helper->after_srp_metrics()->prefetch_status_);
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ExpiredCookie)) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  ASSERT_TRUE(content::SetCookie(
      browser()->profile(), GetOriginServerURL("/"),
      "cookietype=Stale;Expires=Sat, 1 Jan 2000 00:00:00 GMT"));

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL prefetch_url = GetOriginServerURL("/simple.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({prefetch_url});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});

  run_loop.Run();

  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_eligible_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_successful_count_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      absl::make_optional(PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe),
      tab_helper->after_srp_metrics()->prefetch_status_);

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::SECURE);
}

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(CookieOnNonApplicableDomain)) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  ASSERT_TRUE(content::SetCookie(browser()->profile(), GURL("https://foo.com"),
                                 "cookietype=Oatmeal"));

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL prefetch_url = GetOriginServerURL("/simple.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({prefetch_url});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});

  run_loop.Run();

  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_eligible_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_successful_count_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      absl::make_optional(PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe),
      tab_helper->after_srp_metrics()->prefetch_status_);

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::SECURE);
}

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(CookiesChangedAfterInitialCheck)) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL prefetch_url = GetOriginServerURL("/simple.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({prefetch_url});
  tab_helper_observer.SetExpectedURLCookiesChangedForPrefetchAfterInitialCheck(
      prefetch_url);

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});

  prefetch_run_loop.Run();

  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_eligible_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_successful_count_);

  base::RunLoop cookie_run_loop;
  tab_helper_observer.SetOnCookiesChangedForPrefetchAfterInitialCheckClosure(
      cookie_run_loop.QuitClosure());

  ASSERT_TRUE(content::SetCookie(browser()->profile(), GetOriginServerURL("/"),
                                 "cookietype=Oatmeal"));
  cookie_run_loop.Run();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(absl::make_optional(
                PrefetchProxyPrefetchStatus::kPrefetchNotUsedCookiesChanged),
            tab_helper->after_srp_metrics()->prefetch_status_);
}

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoAuthChallenges_FromOrigin)) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  GURL auth_challenge_url = GetOriginServerURL("/auth_challenge");

  std::unique_ptr<AuthChallengeObserver> auth_observer =
      std::make_unique<AuthChallengeObserver>(GetWebContents());

  // Do a positive test first to make sure we get an auth challenge under these
  // circumstances.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), auth_challenge_url));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(auth_observer->GotAuthChallenge());

  // Test that an origin auth challenge does not show a dialog.
  auth_observer->Reset();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());
  TestTabHelperObserver tab_helper_observer(tab_helper);

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchErrorClosure(run_loop.QuitClosure());
  tab_helper_observer.SetExpectedPrefetchErrors(
      {{auth_challenge_url, net::HTTP_UNAUTHORIZED}});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {auth_challenge_url});

  run_loop.Run();

  EXPECT_FALSE(auth_observer->GotAuthChallenge());
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ConnectProxyEndtoEnd)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GetOriginServerURL("/simple.html")));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());
  TestTabHelperObserver tab_helper_observer(tab_helper);

  GURL prefetch_url = GetOriginServerURL("/title2.html");

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());
  tab_helper_observer.SetExpectedSuccessfulURLs({prefetch_url});

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});

  // This run loop will quit when the prefetch response has been successfully
  // done and processed.
  run_loop.Run();

  EXPECT_EQ(tab_helper->srp_metrics().prefetch_attempted_count_, 1U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_successful_count_, 1U);

  size_t starting_origin_request_count = OriginServerRequestCount();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));
  EXPECT_EQ(u"Title Of Awesomeness", GetWebContents()->GetTitle());

  VerifyOriginRequestsAreIsolated({prefetch_url.path()});
  VerifyPrefetchRequestsSecPurposeHeader(
      {prefetch_url.path()},
      /*are_requests_anonymous_client_ip=*/true);

  // The origin server should not have served this request.
  EXPECT_EQ(starting_origin_request_count, OriginServerRequestCount());

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::SECURE);
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(PrefetchingUKM_Success)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link_1 = GetOriginServerURL("/title1.html");
  GURL eligible_link_2 = GetOriginServerURL("/title2.html");
  GURL eligible_link_3 = GetOriginServerURL("/title3.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs(
      {eligible_link_1, eligible_link_2, eligible_link_3});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  ukm::SourceId srp_source_id =
      GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  base::HistogramTester histogram_tester;

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {
                                        eligible_link_1,
                                        eligible_link_2,
                                        GURL("http://not-eligible.com/1"),
                                        GURL("http://not-eligible.com/2"),
                                        GURL("http://not-eligible.com/3"),
                                        eligible_link_3,
                                    });

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  run_loop.Run();

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    3);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 3);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 3);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 3);

  // Navigate to a prefetched page to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link_2));
  base::RunLoop().RunUntilIdle();

  VerifyOriginRequestsAreIsolated({
      eligible_link_1.path(),
      eligible_link_2.path(),
      eligible_link_3.path(),
  });

  VerifyPrefetchRequestsSecPurposeHeader(
      {
          eligible_link_1.path(),
          eligible_link_2.path(),
          eligible_link_3.path(),
      },
      /*are_requests_anonymous_client_ip=*/true);

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::SECURE);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      1);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyTime", 1);

  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  auto expected_entries = std::vector<UkmEntry>{
      // eligible_link_1
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"LinkClicked", 0},
              {"LinkPosition", 0},
              {"ResourceType", 1},
              {"Status", 14},
          }},
      // eligible_link_2
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"ISPFilteringStatus", 0},
              {"LinkClicked", 1},
              {"LinkPosition", 1},
              {"ResourceType", 1},
              {"Status", 0},
          }},
      // not eligible url #1
      UkmEntry{srp_source_id,
               {
                   {"LinkClicked", 0},
                   {"LinkPosition", 2},
                   {"ResourceType", 1},
                   {"Status", 7},
               }},
      // not eligible url #2
      UkmEntry{srp_source_id,
               {
                   {"LinkClicked", 0},
                   {"LinkPosition", 3},
                   {"ResourceType", 1},
                   {"Status", 7},
               }},
      // not eligible url #3
      UkmEntry{srp_source_id,
               {
                   {"LinkClicked", 0},
                   {"LinkPosition", 4},
                   {"ResourceType", 1},
                   {"Status", 7},
               }},
      // eligible_link_3
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"LinkClicked", 0},
              {"LinkPosition", 5},
              {"ResourceType", 1},
              {"Status", 14},
          }},
  };
  auto actual_entries =
      GetAndVerifyPrefetchedResourceUKM(starting_page, srp_source_id);
  EXPECT_THAT(actual_entries,
              testing::UnorderedElementsAreArray(
                  BuildPrefetchResourceMatchers(expected_entries)))
      << ActualHumanReadableMetricsToDebugString(actual_entries);

  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_eligible_countName, 3);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_attempted_countName,
                 3);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_successful_countName,
                 3);

  VerifyNoUKMEvent(ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName);

  // Navigate to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  base::RunLoop().RunUntilIdle();

  VerifyUKMAfterSRP(
      eligible_link_2,
      ukm::builders::PrefetchProxy_AfterSRPClick::kClickedLinkSRPPositionName,
      1);
  VerifyUKMAfterSRP(
      eligible_link_2,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPPrefetchEligibleCountName,
      3);
  // 0 is the value of |kPrefetchUsedNoProbe|. The enum is not used here
  // intentionally because its value should never change.
  VerifyUKMAfterSRP(
      eligible_link_2,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPClickPrefetchStatusName,
      0);

  EXPECT_EQ(
      absl::nullopt,
      GetUKMMetric(
          eligible_link_2,
          ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
          ukm::builders::PrefetchProxy_AfterSRPClick::kProbeLatencyMsName));
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(Origin503RetryAfter)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link_ok = GetOriginServerURL("/title1.html");
  GURL eligible_link_503 =
      GetOriginServerURL("/prefetch/prefetch_proxy/page503.html");

  // Do a prefetch with 503 Service Unavailable, Retry After 10s.
  {
    base::HistogramTester histogram_tester;
    TestTabHelperObserver tab_helper_observer(tab_helper);
    tab_helper_observer.SetExpectedPrefetchErrors(
        {{eligible_link_503, net::HTTP_SERVICE_UNAVAILABLE}});

    base::RunLoop run_loop;
    tab_helper_observer.SetOnPrefetchErrorClosure(run_loop.QuitClosure());

    GURL doc_url("https://www.google.com/search?q=test");
    MakeNavigationPrediction(doc_url, {eligible_link_503});
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.RespCode",
        net::HTTP_SERVICE_UNAVAILABLE, 1);
    EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
    EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_attempted_count_);
    EXPECT_EQ(0U, tab_helper->srp_metrics().prefetch_successful_count_);
  }

  // Expect that another request is not sent to the origin.
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link_ok});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_attempted_count_);
  EXPECT_EQ(0U, tab_helper->srp_metrics().prefetch_successful_count_);

  // Navigate to the ineligible prefetch page to verify the status.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link_ok));
  EXPECT_EQ(PrefetchProxyPrefetchStatus::kPrefetchIneligibleRetryAfter,
            *tab_helper->after_srp_metrics()->prefetch_status_);

  // Wait 10s and verify we do prefetch afterwards.
  {
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    TestTabHelperObserver tab_helper_observer(tab_helper);
    tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_ok});
    tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());
    MakeNavigationPrediction(doc_url, {eligible_link_ok});
    run_loop.Run();
  }

  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_attempted_count_);
  EXPECT_EQ(1U, tab_helper->srp_metrics().prefetch_successful_count_);
}

// 204's don't commit so this is used to test that the AfterSRPMetrics UKM event
// is recorded if the page does not commit. In the wild, we expect this to
// normally occur due to aborted navigations but the end result is the same.
IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(PrefetchingUKM_NoCommit)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link_204 =
      GetOriginServerURL("/prefetch/prefetch_proxy/page204.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_204});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  base::HistogramTester histogram_tester;

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link_204});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", 204, 1);

  // Navigate to a prefetched page to trigger UKM recording. Note that because
  // the navigation is never committed, the UKM recording happens immediately.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link_204));
  base::RunLoop().RunUntilIdle();

  VerifyUKMAfterSRP(
      eligible_link_204,
      ukm::builders::PrefetchProxy_AfterSRPClick::kClickedLinkSRPPositionName,
      0);
  VerifyUKMAfterSRP(
      eligible_link_204,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPPrefetchEligibleCountName,
      1);
  // 0 is the value of |kPrefetchUsedNoProbe|.  The enum is not used here
  // intentionally because its value should never change.
  VerifyUKMAfterSRP(
      eligible_link_204,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPClickPrefetchStatusName,
      0);

  EXPECT_EQ(
      absl::nullopt,
      GetUKMMetric(
          eligible_link_204,
          ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
          ukm::builders::PrefetchProxy_AfterSRPClick::kProbeLatencyMsName));
}

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PrefetchingUKM_PrefetchError)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL prefetch_404_url = GetOriginServerURL("/404_on_prefetch");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedPrefetchErrors(
      {{prefetch_404_url, net::HTTP_NOT_FOUND}});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchErrorClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_404_url});

  // This run loop will quit when all the prefetch responses have been
  // done and processed.
  run_loop.Run();

  // Navigate to the predicted page to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_404_url));
  base::RunLoop().RunUntilIdle();

  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_eligible_countName, 1);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_attempted_countName,
                 1);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_successful_countName,
                 0);

  VerifyNoUKMEvent(ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName);

  // Navigate to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  base::RunLoop().RunUntilIdle();

  VerifyUKMAfterSRP(
      prefetch_404_url,
      ukm::builders::PrefetchProxy_AfterSRPClick::kClickedLinkSRPPositionName,
      0);
  VerifyUKMAfterSRP(
      prefetch_404_url,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPPrefetchEligibleCountName,
      1);
  // 12 is the value of |kPrefetchFailedNon2XX|. The enum is not
  // used here intentionally because its value should never change.
  VerifyUKMAfterSRP(
      prefetch_404_url,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPClickPrefetchStatusName,
      12);

  EXPECT_EQ(
      absl::nullopt,
      GetUKMMetric(
          prefetch_404_url,
          ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
          ukm::builders::PrefetchProxy_AfterSRPClick::kProbeLatencyMsName));
}

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PrefetchingUKM_LinkNotOnSRP)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link = GetOriginServerURL("/title1.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  run_loop.Run();

  GURL link_not_on_srp = GetOriginServerURL("/title2.html");

  // Navigate to the page to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), link_not_on_srp));
  base::RunLoop().RunUntilIdle();

  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_eligible_countName, 1);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_attempted_countName,
                 1);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_successful_countName,
                 1);

  VerifyNoUKMEvent(ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName);

  // Navigate to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  base::RunLoop().RunUntilIdle();

  VerifyUKMAfterSRP(
      link_not_on_srp,
      ukm::builders::PrefetchProxy_AfterSRPClick::kClickedLinkSRPPositionName,
      absl::nullopt);
  VerifyUKMAfterSRP(
      link_not_on_srp,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPPrefetchEligibleCountName,
      1);
  // 15 is the value of |kNavigatedToLinkNotOnSRP|. The enum is not used here
  // intentionally because its value should never change.
  VerifyUKMAfterSRP(
      link_not_on_srp,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPClickPrefetchStatusName,
      15);

  EXPECT_EQ(
      absl::nullopt,
      GetUKMMetric(
          link_not_on_srp,
          ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
          ukm::builders::PrefetchProxy_AfterSRPClick::kProbeLatencyMsName));
}

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PrefetchingUKM_LinkNotEligible)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  GURL ineligible_link = GetInsecureURL("/title1.html");

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {ineligible_link});

  // No run loop is needed here since the eligibility check won't run a cookie
  // check or prefetch, so everything will be synchronous.

  // Navigate to the page to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ineligible_link));
  base::RunLoop().RunUntilIdle();

  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_eligible_countName, 0);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_attempted_countName,
                 0);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_successful_countName,
                 0);

  VerifyNoUKMEvent(ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName);

  // Navigate to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  base::RunLoop().RunUntilIdle();

  VerifyUKMAfterSRP(
      ineligible_link,
      ukm::builders::PrefetchProxy_AfterSRPClick::kClickedLinkSRPPositionName,
      0);
  VerifyUKMAfterSRP(
      ineligible_link,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPPrefetchEligibleCountName,
      0);
  // 7 is the value of |kPrefetchNotEligibleSchemeIsNotHttps|. The enum is not
  // used here intentionally because its value should never change.
  VerifyUKMAfterSRP(
      ineligible_link,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPClickPrefetchStatusName,
      7);

  EXPECT_EQ(
      absl::nullopt,
      GetUKMMetric(
          ineligible_link,
          ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
          ukm::builders::PrefetchProxy_AfterSRPClick::kProbeLatencyMsName));
}

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PrefetchingUKM_PrefetchNotStarted)) {
  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  // By default, only 1 link will be prefetched.
  GURL eligible_link_1 = GetOriginServerURL("/title1.html");
  GURL eligible_link_2 = GetOriginServerURL("/title2.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_1});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {
                                        eligible_link_1,
                                        eligible_link_2,
                                        GURL("http://not-eligible.com/1"),
                                        GURL("http://not-eligible.com/2"),
                                        GURL("http://not-eligible.com/3"),
                                    });

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  run_loop.Run();

  // Navigate to a prefetched page to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link_2));
  base::RunLoop().RunUntilIdle();

  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_eligible_countName, 2);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_attempted_countName,
                 1);
  VerifyUKMOnSRP(starting_page,
                 ukm::builders::PrefetchProxy::kprefetch_successful_countName,
                 1);

  VerifyNoUKMEvent(ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName);

  // Navigate to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  base::RunLoop().RunUntilIdle();

  VerifyUKMAfterSRP(
      eligible_link_2,
      ukm::builders::PrefetchProxy_AfterSRPClick::kClickedLinkSRPPositionName,
      1);
  VerifyUKMAfterSRP(
      eligible_link_2,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPPrefetchEligibleCountName,
      2);
  // 3 is the value of |kPrefetchNotStarted|. The enum is not used here
  // intentionally because its value should never change.
  VerifyUKMAfterSRP(
      eligible_link_2,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPClickPrefetchStatusName,
      3);

  EXPECT_EQ(
      absl::nullopt,
      GetUKMMetric(
          eligible_link_2,
          ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
          ukm::builders::PrefetchProxy_AfterSRPClick::kProbeLatencyMsName));
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(CookiesUsedAndCopied)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  std::vector<net::test_server::HttpRequest> origin_requests_after_prefetch =
      origin_server_requests();

  base::HistogramTester histogram_tester;

  // Navigate to the predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  std::vector<net::test_server::HttpRequest> origin_requests_after_click =
      origin_server_requests();

  // We expect that the image and possibly other resources (NSP not tested here)
  // were loaded.
  EXPECT_GT(origin_requests_after_click.size(),
            origin_requests_after_prefetch.size());

  bool inspected_image_request = false;
  for (size_t i = origin_requests_after_prefetch.size();
       i < origin_requests_after_click.size(); ++i) {
    net::test_server::HttpRequest request = origin_requests_after_click[i];
    if (request.GetURL().path() != "/prefetch/prefetch_proxy/image.png") {
      // Other requests are nice and all, but we're just going to check the
      // image since it won't have been prefetched.
      continue;
    }
    inspected_image_request = true;

    // The prefetched cookie should be present.
    auto cookie_iter = request.headers.find("Cookie");
    ASSERT_FALSE(cookie_iter == request.headers.end());
    EXPECT_EQ(cookie_iter->second, "type=ChocolateChip");
  }

  EXPECT_TRUE(inspected_image_request);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.CookiesToCopy", 1, 1);

  // The cookie from prefetch should also be present in the CookieManager API.
  EXPECT_EQ("type=ChocolateChip",
            content::GetCookies(
                browser()->profile(), eligible_link,
                net::CookieOptions::SameSiteCookieContext::MakeInclusive()));

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::SECURE);
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ClientCertDenied)) {
  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateCertStore));

  WaitForUpdatedCustomProxyConfig();

  // Setup a test server that requires a client cert.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                            ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  GURL client_cert_needed_page = https_server.GetURL("b.test", "/simple.html");

  // Configure the normal profile to automatically satisfy the client cert
  // request.
  std::unique_ptr<base::DictionaryValue> setting =
      std::make_unique<base::DictionaryValue>();
  base::Value* filters = setting->SetKey("filters", base::ListValue());
  filters->Append(base::DictionaryValue());
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetWebsiteSettingDefaultScope(
          client_cert_needed_page, GURL(),
          ContentSettingsType::AUTO_SELECT_CERTIFICATE,
          base::Value::FromUniquePtrValue(std::move(setting)));

  // Navigating to the page should work just fine in the normal profile.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), client_cert_needed_page));
  content::NavigationEntry* entry =
      GetWebContents()->GetController().GetLastCommittedEntry();
  EXPECT_EQ(entry->GetPageType(), content::PAGE_TYPE_NORMAL);

  // Prefetching the page should fail.
  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedPrefetchErrors(
      {{client_cert_needed_page, net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED}});

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchErrorClosure(run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {client_cert_needed_page});

  // This run loop will quit when the prefetch response have been
  // successfully done and processed with the expected error.
  run_loop.Run();
}

class PrefetchProxyWithDecoyRequestsBrowserTest
    : public PrefetchProxyBrowserTest {
 public:
  PrefetchProxyWithDecoyRequestsBrowserTest() = default;
  ~PrefetchProxyWithDecoyRequestsBrowserTest() override = default;

  // PrefetchProxyBrowserTest:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    PrefetchProxyBrowserTest::SetUpCommandLine(cmd);
    cmd->RemoveSwitch("prefetch-proxy-never-send-decoy-requests-for-testing");
    cmd->AppendSwitch("prefetch-proxy-always-send-decoy-requests-for-testing");
  }
};

IN_PROC_BROWSER_TEST_F(PrefetchProxyWithDecoyRequestsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ServiceWorker)) {
  GURL starting_page =
      GetOriginServerURL("/service_worker/create_service_worker.html");

  // Load a page that registers a service worker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  EXPECT_EQ("DONE", EvalJs(GetWebContents(),
                           "register('network_fallback_worker.js');"));

  content::ServiceWorkerContext* service_worker_context_ =
      browser()
          ->profile()
          ->GetDefaultStoragePartition()
          ->GetServiceWorkerContext();
  ASSERT_TRUE(service_worker_context_->MaybeHasRegistrationForStorageKey(
      blink::StorageKey(url::Origin::Create(starting_page))));

  ukm::SourceId srp_source_id =
      GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  base::RunLoop run_loop;
  TestTabHelperObserver tab_helper_observer(
      PrefetchProxyTabHelper::FromWebContents(GetWebContents()));
  tab_helper_observer.SetDecoyPrefetchClosure(run_loop.QuitClosure());

  size_t starting_origin_request_count = origin_server_requests().size();

  GURL prefetch_url = GetOriginServerURL("/title2.html");
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});
  run_loop.Run();

  size_t after_prefetch_origin_request_count = origin_server_requests().size();
  EXPECT_EQ(starting_origin_request_count + 1,
            after_prefetch_origin_request_count);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  // The prefetch should not have been used, so the webpage should have been
  // requested again.
  EXPECT_GT(origin_server_requests().size(),
            after_prefetch_origin_request_count);

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  auto expected_entries = std::vector<UkmEntry>{
      // prefetch_url
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"LinkClicked", 1},
              {"LinkPosition", 0},
              {"ResourceType", 1},
              {"Status", 29},
          }},
  };
  auto actual_entries =
      GetAndVerifyPrefetchedResourceUKM(starting_page, srp_source_id);
  EXPECT_THAT(actual_entries,
              testing::UnorderedElementsAreArray(
                  BuildPrefetchResourceMatchers(expected_entries)))
      << ActualHumanReadableMetricsToDebugString(actual_entries);

  // 29 = |kPrefetchIsPrivacyDecoy|
  EXPECT_EQ(absl::optional<int64_t>(29),
            GetUKMMetric(prefetch_url,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
  VerifyUKMAfterSRP(
      prefetch_url,
      ukm::builders::PrefetchProxy_AfterSRPClick::kClickedLinkSRPPositionName,
      0);
  VerifyUKMAfterSRP(
      prefetch_url,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPPrefetchEligibleCountName,
      0);
  EXPECT_EQ(
      absl::nullopt,
      GetUKMMetric(
          prefetch_url, ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
          ukm::builders::PrefetchProxy_AfterSRPClick::kProbeLatencyMsName));
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyWithDecoyRequestsBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(Cookie)) {
  GURL starting_page = GetOriginServerURL("/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  ASSERT_TRUE(content::SetCookie(browser()->profile(), GetOriginServerURL("/"),
                                 "cookietype=ChocolateChip"));

  ukm::SourceId srp_source_id =
      GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  base::RunLoop run_loop;
  TestTabHelperObserver tab_helper_observer(
      PrefetchProxyTabHelper::FromWebContents(GetWebContents()));
  tab_helper_observer.SetDecoyPrefetchClosure(run_loop.QuitClosure());

  size_t starting_origin_request_count = origin_server_requests().size();

  GURL prefetch_url = GetOriginServerURL("/title2.html");
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});
  run_loop.Run();

  size_t after_prefetch_origin_request_count = origin_server_requests().size();
  EXPECT_EQ(starting_origin_request_count + 1,
            after_prefetch_origin_request_count);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  // The prefetch should not have been used, so the webpage should have been
  // requested again.
  EXPECT_GT(origin_server_requests().size(),
            after_prefetch_origin_request_count);

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  auto expected_entries = std::vector<UkmEntry>{
      // prefetch_url
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"LinkClicked", 1},
              {"LinkPosition", 0},
              {"ResourceType", 1},
              {"Status", 29},
          }},
  };
  auto actual_entries =
      GetAndVerifyPrefetchedResourceUKM(starting_page, srp_source_id);
  EXPECT_THAT(actual_entries,
              testing::UnorderedElementsAreArray(
                  BuildPrefetchResourceMatchers(expected_entries)))
      << ActualHumanReadableMetricsToDebugString(actual_entries);

  // 29 = |kPrefetchIsPrivacyDecoy|
  EXPECT_EQ(absl::optional<int64_t>(29),
            GetUKMMetric(prefetch_url,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
  VerifyUKMAfterSRP(
      prefetch_url,
      ukm::builders::PrefetchProxy_AfterSRPClick::kClickedLinkSRPPositionName,
      0);
  VerifyUKMAfterSRP(
      prefetch_url,
      ukm::builders::PrefetchProxy_AfterSRPClick::kSRPPrefetchEligibleCountName,
      0);
  EXPECT_EQ(
      absl::nullopt,
      GetUKMMetric(
          prefetch_url, ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
          ukm::builders::PrefetchProxy_AfterSRPClick::kProbeLatencyMsName));
}

class PolicyTestPrefetchProxyBrowserTest : public policy::PolicyTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatePrerenders,
         blink::features::kSpeculationRulesPrefetchProxy},
        {});
    policy::PolicyTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("enable-spdy-proxy-auth");
  }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void InsertSpeculation(bool use_prefetch_proxy,
                         const std::vector<GURL>& prefetch_urls) {
    std::string speculation_script = R"(
      var script = document.createElement('script');
      script.type = 'speculationrules';
      script.text = `{
        "prefetch": [{
          "source": "list",
          "urls": [)";

    bool first = true;
    for (const GURL& url : prefetch_urls) {
      if (!first)
        speculation_script.append(",");
      first = false;
      speculation_script.append("\"").append(url.spec()).append("\"");
    }
    speculation_script.append("]");

    if (use_prefetch_proxy)
      speculation_script.append(R"(,
          "requires": ["anonymous-client-ip-when-cross-origin"])");
    speculation_script.append(R"(
        }]
      }`;
      document.head.appendChild(script);)");

    EXPECT_TRUE(ExecuteScript(GetWebContents(), speculation_script));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Predictions should be ignored when the preload setting is disabled by policy.
IN_PROC_BROWSER_TEST_F(PolicyTestPrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoPrefetching)) {
  policy::PolicyMap policies;
  policies.Set(
      policy::key::kNetworkPredictionOptions, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(
          static_cast<int>(prefetch::NetworkPredictionOptions::kDisabled)),
      nullptr);
  UpdateProviderPolicy(policies);

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL doc_url("https://www.google.com/search?q=test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), doc_url));
  InsertSpeculation(true, {GURL("https://test.com/")});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(tab_helper->srp_metrics().predicted_urls_count_, 0U);
}

// A negative test where the only thing missing is the policy change from
// default, ensure that predictions are getting used.
IN_PROC_BROWSER_TEST_F(PolicyTestPrefetchProxyBrowserTest,
                       DISABLED_PrefetchingWithDefault) {
  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL doc_url("https://www.google.com/search?q=test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), doc_url));
  InsertSpeculation(true, {GURL("https://test.com/")});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(tab_helper->srp_metrics().predicted_urls_count_, 1U);
}

class SSLReportingPrefetchProxyBrowserTest : public PrefetchProxyBrowserTest {
 public:
  SSLReportingPrefetchProxyBrowserTest() {
    // Certificate reports are only sent from official builds, unless this has
    // been called.
    CertReportHelper::SetFakeOfficialBuildForTesting();

    // CertReportHelper::ShouldReportCertificateError checks the value of this
    // variation. Ensure reporting is enabled.
    variations::testing::VariationParamsManager::SetVariationParams(
        "ReportCertificateErrors", "ShowAndPossiblySend",
        {{"sendingThreshold", "1.0"}});
  }

  void SetFeatures() override {
    // Important: Features with parameters can't be used here, because it will
    // cause a failed DCHECK in the SSL reporting test.
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatePrerenders,
         blink::features::kSpeculationRulesPrefetchProxy},
        {});
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    PrefetchProxyBrowserTest::SetUpCommandLine(cmd);
    cmd->RemoveSwitch("ignore-certificate-errors");
  }

  security_interstitials::SecurityInterstitialPage* GetInterstitialPage(
      content::WebContents* tab) {
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            tab);
    if (!helper)
      return nullptr;
    return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SSLReportingPrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoIntersitialSSLErrorReporting)) {
  WaitForUpdatedCustomProxyConfig();

  // Setup a test server that requires a client cert.
  net::EmbeddedTestServer https_expired_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_expired_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_expired_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_expired_server.Start());

  GURL safe_page = GURL("https://www.google.com/search?q=test");

  // Opt in to sending reports for invalid certificate chains.
  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), safe_page));

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link = https_expired_server.GetURL("b.test", "/simple.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  // |ERR_INSECURE_RESPONSE| is set by the URLLoader.
  tab_helper_observer.SetExpectedPrefetchErrors(
      {{eligible_link, net::ERR_INSECURE_RESPONSE}});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchErrorClosure(
      prefetch_run_loop.QuitClosure());

  InsertSpeculation(false, true, {eligible_link});

  // This run loop stops when the prefetches completes with its error.
  prefetch_run_loop.Run();

  // No interstitial should be shown and so no report will be made.
  EXPECT_FALSE(GetInterstitialPage(GetWebContents()));
}

class DomainReliabilityPrefetchProxyBrowserTest
    : public PrefetchProxyBrowserTest {
 public:
  DomainReliabilityPrefetchProxyBrowserTest() = default;

  void SetUp() override {
    ProfileNetworkContextService::SetDiscardDomainReliabilityUploadsForTesting(
        false);
    PrefetchProxyBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    PrefetchProxyBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitch(switches::kEnableDomainReliability);
  }

  network::mojom::NetworkContext* GetNormalNetworkContext() {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext();
  }

  void RequestMonitor(const net::test_server::HttpRequest& request) {
    requests_.push_back(request);
    if (request.GetURL().path() == "/domainreliabilty-upload" &&
        on_got_reliability_report_) {
      std::move(on_got_reliability_report_).Run();
    }
  }

 protected:
  base::OnceClosure on_got_reliability_report_;
  std::vector<net::test_server::HttpRequest> requests_;
};

IN_PROC_BROWSER_TEST_F(
    DomainReliabilityPrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoDomainReliabilityUploads)) {
  WaitForUpdatedCustomProxyConfig();

  net::EmbeddedTestServer https_report_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_report_server.RegisterRequestMonitor(base::BindRepeating(
      &DomainReliabilityPrefetchProxyBrowserTest::RequestMonitor,
      base::Unretained(this)));
  net::test_server::RegisterDefaultHandlers(&https_report_server);
  ASSERT_TRUE(https_report_server.Start());

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    GetNormalNetworkContext()->AddDomainReliabilityContextForTesting(
        https_report_server.GetOrigin("a.test"),
        https_report_server.GetURL("a.test", "/domainreliabilty-upload"));
  }

  // Do a prefetch which will fail.

  // This url will cause the server to close the socket, resulting in a net
  // error.
  GURL error_url = https_report_server.GetURL("a.test", "/close-socket");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedPrefetchErrors(
      {{error_url, net::ERR_EMPTY_RESPONSE}});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchErrorClosure(
      prefetch_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {error_url});

  // This run loop will quit when all the prefetch responses have errored.
  prefetch_run_loop.Run();

  base::RunLoop report_run_loop;
  on_got_reliability_report_ = report_run_loop.QuitClosure();

  // Now navigate to the same page and expect that there will be a single domain
  // reliability report, i.e.: this navigation and not one from the prefetch.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), error_url));

  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    GetNormalNetworkContext()->ForceDomainReliabilityUploadsForTesting();
  }

  // This run loop will quit when the most recent navigation send its
  // reliability report. By this time we expect that if the prefetch would have
  // sent a report, it would have already done so.
  report_run_loop.Run();

  size_t found_reports = 0;
  for (const net::test_server::HttpRequest& request : requests_) {
    if (request.GetURL().path() == "/domainreliabilty-upload") {
      found_reports++;
    }
  }
  EXPECT_EQ(1U, found_reports);
}

class PrefetchProxyBaseProbingBrowserTest : public PrefetchProxyBrowserTest {
 public:
  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  void WaitForTLSCanaryCheck() {
    PrefetchProxyService* service =
        PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
    while (!service->origin_prober()->IsTLSCanaryCheckCompleteForTesting()) {
      base::RunLoop().RunUntilIdle();
    }
  }

  void WaitForDNSCanaryCheck() {
    PrefetchProxyService* service =
        PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
    while (!service->origin_prober()->IsDNSCanaryCheckCompleteForTesting()) {
      base::RunLoop().RunUntilIdle();
    }
  }

  void RunProbeTest(bool wait_for_tls,
                    bool probe_success,
                    bool expect_successful_tls_probe,
                    int64_t expected_status,
                    bool expect_probe) {
    WaitForDNSCanaryCheck();
    if (wait_for_tls) {
      WaitForTLSCanaryCheck();
    }

    // Setup a local probing server so we can watch its accepted socket count.
    TestServerConnectionCounter probe_counter;
    net::EmbeddedTestServer probing_server(net::EmbeddedTestServer::TYPE_HTTPS);
    probing_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    probing_server.SetConnectionListener(&probe_counter);
    ASSERT_TRUE(probing_server.Start());

    GURL starting_page = GetOriginServerURL("/simple.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
    WaitForUpdatedCustomProxyConfig();

    PrefetchProxyTabHelper* tab_helper =
        PrefetchProxyTabHelper::FromWebContents(GetWebContents());

    GURL eligible_link = GetOriginServerURL("/title2.html");

    TestTabHelperObserver tab_helper_observer(tab_helper);
    tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

    base::RunLoop run_loop;
    tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());

    GURL doc_url("https://www.google.com/search?q=test");
    MakeNavigationPrediction(doc_url, {eligible_link});

    // This run loop will quit when all the prefetch responses have been
    // successfully done and processed.
    run_loop.Run();

    CustomProbeOverrideDelegate probe_delegate(
        probe_success ? probing_server.GetURL("a.test", "/")
                      : GURL("http://invalid.com"));

    PrefetchProxyService* service =
        PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
    service->origin_prober()->SetProbeURLOverrideDelegateOverrideForTesting(
        &probe_delegate);

    // Navigate to the prefetched page, this also triggers UKM recording.
    ASSERT_EQ(0U, probe_counter.count());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));
    EXPECT_EQ(expect_successful_tls_probe, 1U == probe_counter.count());

    EXPECT_EQ(u"Title Of Awesomeness", GetWebContents()->GetTitle());

    ASSERT_TRUE(tab_helper->after_srp_metrics());
    ASSERT_TRUE(tab_helper->after_srp_metrics()->prefetch_status_.has_value());
    EXPECT_EQ(expected_status,
              static_cast<int>(
                  tab_helper->after_srp_metrics()->prefetch_status_.value()));

    absl::optional<base::TimeDelta> probe_latency =
        tab_helper->after_srp_metrics()->probe_latency_;
    if (expect_probe) {
      ASSERT_TRUE(probe_latency.has_value());
      EXPECT_GT(probe_latency.value(), base::TimeDelta());
    } else {
      EXPECT_FALSE(probe_latency.has_value());
    }

    // Navigate again to trigger UKM recording.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(
        absl::optional<int64_t>(expected_status),
        GetUKMMetric(eligible_link,
                     ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                     ukm::builders::PrefetchProxy_AfterSRPClick::
                         kSRPClickPrefetchStatusName));

    absl::optional<int64_t> probe_latency_ms = GetUKMMetric(
        eligible_link, ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
        ukm::builders::PrefetchProxy_AfterSRPClick::kProbeLatencyMsName);
    if (expect_probe) {
      EXPECT_NE(absl::nullopt, probe_latency_ms);
    } else {
      EXPECT_EQ(absl::nullopt, probe_latency_ms);
    }
  }

 private:
  base::HistogramTester histogram_tester_;
};

class ProbingEnabled_CanaryOn_BothCanaryGood_PrefetchProxyBrowserTest
    : public PrefetchProxyBaseProbingBrowserTest {
 public:
  void SetFeatures() override {
    PrefetchProxyBaseProbingBrowserTest::SetFeatures();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerendersMustProbeOrigin,
        {
            {"do_canary", "true"},
            {"do_tls_canary", "true"},
            {"tls_canary_url", "https://resolve-success.com"},
            {"dns_canary_url", "https://resolve-success.com"},
            {"ineligible_decoy_request_probability", "0"},
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ProbingEnabled_CanaryOn_TLSCanaryBad_DNSCanaryBad_PrefetchProxyBrowserTest
    : public PrefetchProxyBaseProbingBrowserTest {
 public:
  void SetFeatures() override {
    PrefetchProxyBaseProbingBrowserTest::SetFeatures();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerendersMustProbeOrigin,
        {
            {"do_canary", "true"},
            {"do_tls_canary", "true"},
            {"tls_canary_url", "https://resolve-fail.com"},
            {"dns_canary_url", "https://resolve-fail.com"},
            {"ineligible_decoy_request_probability", "0"},
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class
    ProbingEnabled_CanaryOn_TLSCanaryBad_DNSCanaryGood_PrefetchProxyBrowserTest
    : public PrefetchProxyBaseProbingBrowserTest {
 public:
  void SetFeatures() override {
    PrefetchProxyBaseProbingBrowserTest::SetFeatures();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerendersMustProbeOrigin,
        {
            {"do_canary", "true"},
            {"do_tls_canary", "true"},
            {"tls_canary_url", "https://resolve-fail.com"},
            {"dns_canary_url", "https://resolve-success.com"},
            {"ineligible_decoy_request_probability", "0"},
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class
    ProbingEnabled_CanaryOn_TLSCanaryBadDisabled_DNSCanaryGood_PrefetchProxyBrowserTest
    : public PrefetchProxyBaseProbingBrowserTest {
 public:
  void SetFeatures() override {
    PrefetchProxyBaseProbingBrowserTest::SetFeatures();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerendersMustProbeOrigin,
        {
            {"do_canary", "true"},
            {"do_tls_canary", "false"},
            {"tls_canary_url", "https://resolve-fail.com"},
            {"dns_canary_url", "https://resolve-success.com"},
            {"ineligible_decoy_request_probability", "0"},
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class
    ProbingEnabled_CanaryOn_TLSCanaryGood_DNSCanaryBad_PrefetchProxyBrowserTest
    : public PrefetchProxyBaseProbingBrowserTest {
 public:
  void SetFeatures() override {
    PrefetchProxyBaseProbingBrowserTest::SetFeatures();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerendersMustProbeOrigin,
        {
            {"do_canary", "true"},
            {"do_tls_canary", "true"},
            {"tls_canary_url", "https://resolve-success.com"},
            {"dns_canary_url", "https://resolve-fail.com"},
            {"ineligible_decoy_request_probability", "0"},
        });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ProbingDisabledPrefetchProxyBrowserTest
    : public PrefetchProxyBaseProbingBrowserTest {
 public:
  void SetFeatures() override {
    PrefetchProxyBaseProbingBrowserTest::SetFeatures();
    scoped_feature_list_.InitAndDisableFeature(
        features::kIsolatePrerendersMustProbeOrigin);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ProbingEnabled_CanaryOn_BothCanaryGood_PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoProbe)) {
  RunProbeTest(/*wait_for_tls=*/true,
               /*probe_success=*/false,
               /*expect_successful_tls_probe=*/false,
               /*expected_status=*/0,
               /*expect_probe=*/false);
}

IN_PROC_BROWSER_TEST_F(
    ProbingEnabled_CanaryOn_TLSCanaryGood_DNSCanaryBad_PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DNSProbeOK)) {
  RunProbeTest(/*wait_for_tls=*/true,
               /*probe_success=*/true,
               /*expect_successful_tls_probe=*/false,
               /*expected_status=*/1,
               /*expect_probe=*/true);
}

IN_PROC_BROWSER_TEST_F(
    ProbingEnabled_CanaryOn_TLSCanaryGood_DNSCanaryBad_PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(DNSProbeBad)) {
  RunProbeTest(/*wait_for_tls=*/true,
               /*probe_success=*/false,
               /*expect_successful_tls_probe=*/false,
               /*expected_status=*/2,
               /*expect_probe=*/true);
}

IN_PROC_BROWSER_TEST_F(
    ProbingEnabled_CanaryOn_TLSCanaryBad_DNSCanaryBad_PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TLSProbeOK)) {
  RunProbeTest(/*wait_for_tls=*/true,
               /*probe_success=*/true,
               /*expect_successful_tls_probe=*/true,
               /*expected_status=*/1,
               /*expect_probe=*/true);
}

IN_PROC_BROWSER_TEST_F(
    ProbingEnabled_CanaryOn_TLSCanaryBad_DNSCanaryBad_PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TLSProbeBad)) {
  RunProbeTest(/*wait_for_tls=*/true,
               /*probe_success=*/false,
               /*expect_successful_tls_probe=*/false,
               /*expected_status=*/2,
               /*expect_probe=*/true);
}

IN_PROC_BROWSER_TEST_F(
    ProbingEnabled_CanaryOn_TLSCanaryBad_DNSCanaryGood_PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TLSProbeOK)) {
  RunProbeTest(/*wait_for_tls=*/true,
               /*probe_success=*/true,
               /*expect_successful_tls_probe=*/true,
               /*expected_status=*/1,
               /*expect_probe=*/true);
}

IN_PROC_BROWSER_TEST_F(
    ProbingEnabled_CanaryOn_TLSCanaryBad_DNSCanaryGood_PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(TLSProbeBad)) {
  RunProbeTest(/*wait_for_tls=*/true,
               /*probe_success=*/false,
               /*expect_successful_tls_probe=*/false,
               /*expected_status=*/2,
               /*expect_probe=*/true);
}

IN_PROC_BROWSER_TEST_F(
    ProbingEnabled_CanaryOn_TLSCanaryBadDisabled_DNSCanaryGood_PrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NoProbe)) {
  RunProbeTest(/*wait_for_tls=*/false,
               /*probe_success=*/false,
               /*expect_successful_tls_probe=*/false,
               /*expected_status=*/0,
               /*expect_probe=*/false);
}

class PrefetchProxyWithNSPBrowserTest : public PrefetchProxyBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    PrefetchProxyBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitch("isolated-prerender-nsp-enabled");
  }

  void SetFeatures() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kIsolatePrerenders,
          {{"use_speculation_rules", "false"},
           {"max_subresource_count_per_prerender", "50"}}},
         {blink::features::kLightweightNoStatePrefetch, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrefetchProxyWithNSPBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(SuccessfulNSPEndToEnd)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  base::HistogramTester histogram_tester;

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS));

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  base::RunLoop nsp_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  std::vector<net::test_server::HttpRequest> origin_requests_before_prerender =
      origin_server_requests();
  std::vector<net::test_server::HttpRequest> proxy_requests_before_prerender =
      proxy_server_requests();

  // This run loop will quit when a NSP finishes.
  nsp_run_loop.Run();

  // Regression test for crbug/1131712.
  WaitForHistoryBackendToRun(browser()->profile());
  ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
  EXPECT_FALSE(base::Contains(enumerator.urls(), eligible_link));

  std::vector<net::test_server::HttpRequest> origin_requests_after_prerender =
      origin_server_requests();
  std::vector<net::test_server::HttpRequest> proxy_requests_after_prerender =
      proxy_server_requests();

  EXPECT_GT(proxy_requests_after_prerender.size(),
            proxy_requests_before_prerender.size());

  for (const net::test_server::HttpRequest& request :
       origin_requests_after_prerender) {
    EXPECT_FALSE(RequestHasClientHints(request));
  }

  // Check that the page's Javascript was NSP'd, but not the mainframe.
  bool found_nsp_javascript = false;
  bool found_nsp_mainframe = false;
  bool found_image = false;
  for (size_t i = origin_requests_before_prerender.size();
       i < origin_requests_after_prerender.size(); ++i) {
    net::test_server::HttpRequest request = origin_requests_after_prerender[i];

    // prefetch_page.html sets a cookie on its response and we should see it
    // here.
    auto cookie_iter = request.headers.find("Cookie");
    ASSERT_FALSE(cookie_iter == request.headers.end());
    EXPECT_EQ(cookie_iter->second, "type=ChocolateChip");

    GURL nsp_url = request.GetURL();
    found_nsp_javascript |=
        nsp_url.path() == "/prefetch/prefetch_proxy/prefetch.js";
    found_nsp_mainframe |= nsp_url.path() == eligible_link.path();
    found_image |= nsp_url.path() == "/prefetch/prefetch_proxy/image.png";
  }
  EXPECT_TRUE(found_nsp_javascript);
  EXPECT_FALSE(found_nsp_mainframe);
  EXPECT_FALSE(found_image);

  VerifyOriginRequestsAreIsolated({
      "/prefetch/prefetch_proxy/prefetch.js",
      eligible_link.path(),
  });
  VerifyPrefetchRequestsSecPurposeHeader(
      {eligible_link.path()},
      /*are_requests_anonymous_client_ip=*/true);

  // Verify the resource load was reported to the subresource manager.
  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
  PrefetchProxySubresourceManager* manager =
      service->GetSubresourceManagerForURL(eligible_link);
  ASSERT_TRUE(manager);

  base::RunLoop().RunUntilIdle();

  std::set<GURL> expected_subresources = {
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch.js"),
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch-redirect-start.js"),
      GetOriginServerURL(
          "/prefetch/prefetch_proxy/prefetch-redirect-middle.js"),
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch-redirect-end.js"),
  };
  EXPECT_EQ(expected_subresources, manager->successfully_loaded_subresources());

  EXPECT_TRUE(CheckForResourceInIsolatedCache(
      eligible_link,
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch.js")));
  EXPECT_TRUE(CheckForResourceInIsolatedCache(
      eligible_link,
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch-redirect-end.js")));

  // Navigate to the predicted site. We expect:
  // * The mainframe HTML will not be requested from the origin server.
  // * The JavaScript will not be requested from the origin server.
  // * The prefetched JavaScript will be executed.
  // * The image will be fetched.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  std::vector<net::test_server::HttpRequest> proxy_requests_after_click =
      proxy_server_requests();

  // Nothing should have gone through the proxy.
  EXPECT_EQ(proxy_requests_after_prerender.size(),
            proxy_requests_after_click.size());

  std::vector<net::test_server::HttpRequest> origin_requests_after_click =
      origin_server_requests();

  // Only one request for the image is expected, and it should have cookies.
  ASSERT_EQ(origin_requests_after_prerender.size() + 1,
            origin_requests_after_click.size());
  net::test_server::HttpRequest request =
      origin_requests_after_click[origin_requests_after_click.size() - 1];
  EXPECT_EQ(request.GetURL().path(), "/prefetch/prefetch_proxy/image.png");
  auto cookie_iter = request.headers.find("Cookie");
  ASSERT_FALSE(cookie_iter == request.headers.end());
  EXPECT_EQ(cookie_iter->second, "type=ChocolateChip");

  // The cookie from prefetch should also be present in the CookieManager API.
  EXPECT_EQ("type=ChocolateChip",
            content::GetCookies(
                browser()->profile(), eligible_link,
                net::CookieOptions::SameSiteCookieContext::MakeInclusive()));

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.CookiesToCopy", 1, 1);

  // Check that the JavaScript ran.
  EXPECT_EQ(u"JavaScript Executed", GetWebContents()->GetTitle());

  // Navigate one more time to destroy the SubresourceManager so that its UMA is
  // recorded and to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 16 = |kPrefetchUsedNoProbeWithNSP|.
  EXPECT_EQ(absl::optional<int64_t>(16),
            GetUKMMetric(eligible_link,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Subresources.NetError", net::OK, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Subresources.Quantity", 4, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Subresources.RespCode", 200, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.Subresources.UsedCache", true, 2);
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyWithNSPBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(StartsSpareRenderer)) {
  // Enable low-end device mode to turn off automatic spare renderers. Note that
  // this will also prevent NSPs from triggering, but the logic under test
  // happens before that anyways.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableLowEndDeviceMode);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-start-spare-renderer");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  base::HistogramTester histogram_tester;

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  // Navigate to trigger the histogram recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.SpareRenderer.CountStartedOnSRP", 1, 1);
}

namespace {
std::unique_ptr<net::test_server::HttpResponse> HandleNonEligibleOrigin(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == "/script.js") {
    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    resp->set_content_type("application/javascript");
    resp->set_content("console.log(0);");
    return resp;
  }
  return nullptr;
}

std::unique_ptr<net::test_server::HttpResponse>
HandleOriginWithIneligibleSubresources(
    net::EmbeddedTestServer* non_eligible_server,
    const net::test_server::HttpRequest& request) {
  GURL url = request.GetURL();

  if (url.path() == "/page.html") {
    GURL same_origin_resource =
        non_eligible_server->GetURL("a.test", "/script.js");

    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    resp->set_content_type("text/html");
    resp->set_content(base::StringPrintf(R"(
        <html>
          <head>
            <script src="%s">
          </head>
          <body>Test</body>
        </html>)",
                                         same_origin_resource.spec().c_str()));
    return resp;
  }
  return nullptr;
}

std::unique_ptr<net::test_server::HttpResponse> HandleEligibleOrigin(
    net::EmbeddedTestServer* eligible_server,
    net::EmbeddedTestServer* non_eligible_server,
    const net::test_server::HttpRequest& request) {
  GURL url = request.GetURL();

  if (url.path() == "/page.html") {
    GURL same_origin_resource = eligible_server->GetURL("a.test", "/script.js");
    GURL redirect_resource = eligible_server->GetURL("a.test", "/redirect.js");

    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    resp->set_content_type("text/html");
    resp->set_content(base::StringPrintf(R"(
        <html>
          <head>
            <script src="%s">
            <script src="%s">
          </head>
          <body>Test</body>
        </html>)",
                                         same_origin_resource.spec().c_str(),
                                         redirect_resource.spec().c_str()));
    return resp;
  }

  if (url.path() == "/script.js") {
    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    resp->set_content_type("application/javascript");
    resp->set_content("console.log(0);");
    return resp;
  }

  if (url.path() == "/redirect.js") {
    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_TEMPORARY_REDIRECT);
    resp->AddCustomHeader(
        "location", non_eligible_server->GetURL("b.test", "/script.js").spec());
    return resp;
  }

  return nullptr;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyWithNSPBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NSPWithIneligibleSubresourceRedirect)) {
  net::EmbeddedTestServer non_eligible_origin(
      net::EmbeddedTestServer::TYPE_HTTPS);
  non_eligible_origin.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  non_eligible_origin.RegisterRequestHandler(
      base::BindRepeating(&HandleNonEligibleOrigin));
  ASSERT_TRUE(non_eligible_origin.Start());

  net::EmbeddedTestServer eligible_origin(net::EmbeddedTestServer::TYPE_HTTPS);
  eligible_origin.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  eligible_origin.RegisterRequestHandler(base::BindRepeating(
      &HandleEligibleOrigin, &eligible_origin, &non_eligible_origin));
  ASSERT_TRUE(eligible_origin.Start());

  content::SetCookie(browser()->profile(),
                     non_eligible_origin.GetURL("b.test", "/"), "cookie=yes");

  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link = eligible_origin.GetURL("a.test", "/page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  base::RunLoop nsp_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  // This run loop will quit when a NSP finishes.
  nsp_run_loop.Run();

  // Verify the resource load was reported to the subresource manager.
  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
  PrefetchProxySubresourceManager* manager =
      service->GetSubresourceManagerForURL(eligible_link);
  ASSERT_TRUE(manager);

  base::RunLoop().RunUntilIdle();

  std::set<GURL> expected_subresources = {
      eligible_origin.GetURL("a.test", "/script.js"),
  };
  EXPECT_EQ(expected_subresources, manager->successfully_loaded_subresources());
}

IN_PROC_BROWSER_TEST_F(
    PrefetchProxyWithNSPBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(NSPWithIneligibleSubresources)) {
  TestServerConnectionCounter http_counter;
  net::EmbeddedTestServer non_eligible_origin(
      net::EmbeddedTestServer::TYPE_HTTP);
  non_eligible_origin.SetConnectionListener(&http_counter);
  ASSERT_TRUE(non_eligible_origin.Start());

  net::EmbeddedTestServer eligible_origin(net::EmbeddedTestServer::TYPE_HTTPS);
  eligible_origin.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  eligible_origin.RegisterRequestHandler(base::BindRepeating(
      &HandleOriginWithIneligibleSubresources, &non_eligible_origin));
  ASSERT_TRUE(eligible_origin.Start());

  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link = eligible_origin.GetURL("a.test", "/page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  base::RunLoop nsp_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  // This run loop will quit when a NSP finishes.
  nsp_run_loop.Run();

  EXPECT_EQ(0U, http_counter.count());

  // Verify the resource load was reported to the subresource manager.
  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
  PrefetchProxySubresourceManager* manager =
      service->GetSubresourceManagerForURL(eligible_link);
  ASSERT_TRUE(manager);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(manager->successfully_loaded_subresources().empty());
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyWithNSPBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(PrefetchButNSPDenied)) {
  // NSP is disabled on low-end devices.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableLowEndDeviceMode);

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  // Navigate to the predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 19 = |kPrefetchUsedNoProbeNSPAttemptDenied|.
  EXPECT_EQ(absl::optional<int64_t>(19),
            GetUKMMetric(eligible_link,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyWithNSPBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(OnlyOneNSP)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link_1 =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");
  GURL eligible_link_2 =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html?page=2");

  TestTabHelperObserver tab_helper_observer(tab_helper);

  // Do the prefetches separately so that we know only the first link will ever
  // get prerendered.
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_1});

  base::RunLoop nsp_run_loop;
  base::RunLoop prefetch_1_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_1_run_loop.QuitClosure());
  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link_1});

  // This run loop will quit when the first prefetch response has been
  // successfully done and processed.
  prefetch_1_run_loop.Run();

  nsp_run_loop.Run();

  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_2});

  base::RunLoop prefetch_2_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_2_run_loop.QuitClosure());

  MakeNavigationPrediction(doc_url, {eligible_link_2});

  // This run loop will quit when the second prefetch response has been
  // successfully done and processed.
  prefetch_2_run_loop.Run();

  // Navigate to the second predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link_2));

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 22 = |kPrefetchUsedNoProbeNSPNotStarted|.
  EXPECT_EQ(absl::optional<int64_t>(22),
            GetUKMMetric(eligible_link_2,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyWithNSPBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoLinkRelSearch)) {
  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/link-rel-search-tag.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  base::RunLoop nsp_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  std::vector<net::test_server::HttpRequest> origin_requests_before_prerender =
      origin_server_requests();

  // This run loop will quit when a NSP finishes.
  nsp_run_loop.Run();

  std::vector<net::test_server::HttpRequest> origin_requests_after_prerender =
      origin_server_requests();

  // There should not have been any additional requests.
  EXPECT_EQ(origin_requests_before_prerender.size(),
            origin_requests_after_prerender.size());
}

IN_PROC_BROWSER_TEST_F(PrefetchProxyWithNSPBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(LimitSubresourceCount)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kIsolatedPrerenderLimitNSPSubresourcesCmdLineFlag, "1");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  base::RunLoop nsp_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when a NSP finishes.
  nsp_run_loop.Run();

  base::HistogramTester histogram_tester;

  // Navigate to the predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  // Checks that only one resource was used from cache.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.Subresources.UsedCache", true, 1);

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 16 = |kPrefetchUsedNoProbeWithNSP|.
  EXPECT_EQ(absl::optional<int64_t>(16),
            GetUKMMetric(eligible_link,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
}

class ProbingAndNSPEnabledPrefetchProxyBrowserTest
    : public PrefetchProxyBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    PrefetchProxyBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitch("isolated-prerender-nsp-enabled");
  }

  void SetFeatures() override {
    PrefetchProxyBrowserTest::SetFeatures();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kIsolatePrerenders,
             {{"use_speculation_rules", "false"},
              {"max_subresource_count_per_prerender", "50"}}},
            {blink::features::kLightweightNoStatePrefetch, {}},
            {features::kIsolatePrerendersMustProbeOrigin,
             {{"do_canary", "false"},
              {"ineligible_decoy_request_probability", "0"}}},
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProbingAndNSPEnabledPrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeGood_NSPSuccess)) {
  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  base::RunLoop nsp_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  ukm::SourceId srp_source_id =
      GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when a NSP finishes.
  nsp_run_loop.Run();

  // This event should not be recorded until after the prefetched page is done.
  VerifyNoUKMEvent(ukm::builders::PrefetchProxy_PrefetchedResource::kEntryName);

  // Navigate to the predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  // This event should not be recorded until after the prefetched page is done.
  VerifyNoUKMEvent(ukm::builders::PrefetchProxy_PrefetchedResource::kEntryName);

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 17 = |kPrefetchUsedProbeSuccessWithNSP|.
  EXPECT_EQ(absl::optional<int64_t>(17),
            GetUKMMetric(eligible_link,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));

  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  auto expected_entries = std::vector<UkmEntry>{
      // eligible_link
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"ISPFilteringStatus", 1},            /* matches either 1 or 3 */
              {"LinkClicked", 1},
              {"LinkPosition", 0},
              {"ResourceType", 1},
              {"Status", 1},
          }},
      // and two subresources
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"ISPFilteringStatus", 1},            /* matches either 1 or 3 */
              {"LinkClicked", 1},
              {"LinkPosition", 0},
              {"ResourceType", 2},
              {"Status", 1},
          }},
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"ISPFilteringStatus", 1},            /* matches either 1 or 3 */
              {"LinkClicked", 1},
              {"LinkPosition", 0},
              {"ResourceType", 2},
              {"Status", 1},
          }},
  };
  auto actual_entries =
      GetAndVerifyPrefetchedResourceUKM(starting_page, srp_source_id);
  EXPECT_THAT(actual_entries,
              testing::UnorderedElementsAreArray(
                  BuildPrefetchResourceMatchers(expected_entries)))
      << ActualHumanReadableMetricsToDebugString(actual_entries);
}

IN_PROC_BROWSER_TEST_F(ProbingAndNSPEnabledPrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeGood_NSPDenied)) {
  // NSP is disabled on low-end devices.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableLowEndDeviceMode);

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  // Navigate to the predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 20 = |kPrefetchUsedProbeSuccessNSPAttemptDenied|.
  EXPECT_EQ(absl::optional<int64_t>(20),
            GetUKMMetric(eligible_link,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
}

IN_PROC_BROWSER_TEST_F(ProbingAndNSPEnabledPrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeGood_NSPNotStarted)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link_1 =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");
  GURL eligible_link_2 =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html?page=2");

  TestTabHelperObserver tab_helper_observer(tab_helper);

  // Do the prefetches separately so that we know only the first link will ever
  // get prerendered.
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_1});

  base::RunLoop nsp_run_loop;
  base::RunLoop prefetch_1_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_1_run_loop.QuitClosure());
  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link_1});

  // This run loop will quit when the first prefetch response has been
  // successfully done and processed.
  prefetch_1_run_loop.Run();

  nsp_run_loop.Run();

  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_2});

  base::RunLoop prefetch_2_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_2_run_loop.QuitClosure());

  MakeNavigationPrediction(doc_url, {eligible_link_2});

  // This run loop will quit when the second prefetch response has been
  // successfully done and processed.
  prefetch_2_run_loop.Run();

  // Navigate to the second predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link_2));

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 23 = |kPrefetchUsedProbeSuccessNSPNotStarted|.
  EXPECT_EQ(absl::optional<int64_t>(23),
            GetUKMMetric(eligible_link_2,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
}

IN_PROC_BROWSER_TEST_F(ProbingAndNSPEnabledPrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeBad_NSPSuccess)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  base::RunLoop nsp_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  ukm::SourceId srp_source_id =
      GetWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when a NSP finishes.
  nsp_run_loop.Run();

  std::vector<net::test_server::HttpRequest> origin_requests_after_prerender =
      origin_server_requests();
  std::vector<net::test_server::HttpRequest> proxy_requests_after_prerender =
      proxy_server_requests();

  // Override the probing URL.
  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
  CustomProbeOverrideDelegate delegate(GURL("http://invalid.com"));
  service->origin_prober()->SetProbeURLOverrideDelegateOverrideForTesting(
      &delegate);

  // This event should not be recorded until after the prefetched page is done.
  VerifyNoUKMEvent(ukm::builders::PrefetchProxy_PrefetchedResource::kEntryName);

  // Navigate to the predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  std::vector<net::test_server::HttpRequest> origin_requests_after_click =
      origin_server_requests();
  std::vector<net::test_server::HttpRequest> proxy_requests_after_click =
      proxy_server_requests();

  // All the resources should be loaded from the server since nothing was
  // eligible to be reused from the prefetch on a bad probe.
  EXPECT_EQ(origin_requests_after_prerender.size() + 6,
            origin_requests_after_click.size());

  // The proxy should not be used any further.
  EXPECT_EQ(proxy_requests_after_prerender.size(),
            proxy_requests_after_click.size());

  // This event should not be recorded until after the prefetched page is done.
  VerifyNoUKMEvent(ukm::builders::PrefetchProxy_PrefetchedResource::kEntryName);

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 18 = |kPrefetchNotUsedProbeFailedWithNSP|.
  EXPECT_EQ(absl::optional<int64_t>(18),
            GetUKMMetric(eligible_link,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));

  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  auto expected_entries = std::vector<UkmEntry>{
      // eligible_link
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"ISPFilteringStatus", 4},            /* matches either 2 or 4 */
              {"LinkClicked", 1},
              {"LinkPosition", 0},
              {"ResourceType", 1},
              {"Status", 2},
          }},
      // and two subresources
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"ISPFilteringStatus", 4},            /* matches either 2 or 4 */
              {"LinkClicked", 1},
              {"LinkPosition", 0},
              {"ResourceType", 2},
              {"Status", 14},
          }},
      UkmEntry{
          srp_source_id,
          {
              {"DataLength", 0},                    /* only checked for > 0 */
              {"FetchDurationMS", 0},               /* only checked for > 0 */
              {"NavigationStartToFetchStartMS", 0}, /* only checked for > 0 */
              {"ISPFilteringStatus", 4},            /* matches either 2 or 4 */
              {"LinkClicked", 1},
              {"LinkPosition", 0},
              {"ResourceType", 2},
              {"Status", 14},
          }},
  };
  auto actual_entries =
      GetAndVerifyPrefetchedResourceUKM(starting_page, srp_source_id);
  EXPECT_THAT(actual_entries,
              testing::UnorderedElementsAreArray(
                  BuildPrefetchResourceMatchers(expected_entries)))
      << ActualHumanReadableMetricsToDebugString(actual_entries);
}

IN_PROC_BROWSER_TEST_F(ProbingAndNSPEnabledPrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeBad_NSPDenied)) {
  // NSP is disabled on low-end devices.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableLowEndDeviceMode);

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  // Override the probing URL.
  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
  CustomProbeOverrideDelegate delegate(GURL("http://invalid.com"));
  service->origin_prober()->SetProbeURLOverrideDelegateOverrideForTesting(
      &delegate);

  // Navigate to the predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 21 =  |kPrefetchNotUsedProbeFailedNSPAttemptDenied|.
  EXPECT_EQ(absl::optional<int64_t>(21),
            GetUKMMetric(eligible_link,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
}

IN_PROC_BROWSER_TEST_F(ProbingAndNSPEnabledPrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ProbeBad_NSPNotStarted)) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "isolated-prerender-unlimited-prefetches");

  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link_1 =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");
  GURL eligible_link_2 =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html?page=2");

  TestTabHelperObserver tab_helper_observer(tab_helper);

  // Do the prefetches separately so that we know only the first link will ever
  // get prerendered.
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_1});

  base::RunLoop nsp_run_loop;
  base::RunLoop prefetch_1_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_1_run_loop.QuitClosure());
  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link_1});

  // This run loop will quit when the first prefetch response has been
  // successfully done and processed.
  prefetch_1_run_loop.Run();

  nsp_run_loop.Run();

  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link_2});

  base::RunLoop prefetch_2_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_2_run_loop.QuitClosure());

  MakeNavigationPrediction(doc_url, {eligible_link_2});

  // This run loop will quit when the second prefetch response has been
  // successfully done and processed.
  prefetch_2_run_loop.Run();

  // Override the probing URL.
  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
  CustomProbeOverrideDelegate delegate(GURL("http://invalid.com"));
  service->origin_prober()->SetProbeURLOverrideDelegateOverrideForTesting(
      &delegate);

  // Navigate to the second predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link_2));

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 24 = |kPrefetchNotUsedProbeFailedNSPNotStarted|.
  EXPECT_EQ(absl::optional<int64_t>(24),
            GetUKMMetric(eligible_link_2,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));
}

class SpeculationPrefetchProxyTest : public PrefetchProxyBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    PrefetchProxyBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitch("isolated-prerender-nsp-enabled");
  }

  void SetFeatures() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kIsolatePrerenders,
          {{"use_speculation_rules", "true"},
           {"max_srp_prefetches", "3"},
           {"max_subresource_count_per_prerender", "50"}}},
         {blink::features::kLightweightNoStatePrefetch, {}},
         {blink::features::kSpeculationRulesPrefetchProxy, {}}},
        {{features::kLazyImageLoading}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SpeculationPrefetchProxyTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(SuccessfulNSPEndToEnd)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  base::HistogramTester histogram_tester;

  WaitForUpdatedCustomProxyConfig();

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS));

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  base::RunLoop nsp_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  tab_helper_observer.SetOnNSPFinishedClosure(nsp_run_loop.QuitClosure());

  // Make sure we are on a valid referring page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetReferringPageServerURL("/search/q=blah")));
  InsertSpeculation(true, true, {eligible_link});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  std::vector<net::test_server::HttpRequest> origin_requests_before_prerender =
      origin_server_requests();
  std::vector<net::test_server::HttpRequest> proxy_requests_before_prerender =
      proxy_server_requests();

  // This run loop will quit when a NSP finishes.
  nsp_run_loop.Run();

  // Regression test for crbug/1131712.
  WaitForHistoryBackendToRun(browser()->profile());
  ui_test_utils::HistoryEnumerator enumerator(browser()->profile());
  EXPECT_FALSE(base::Contains(enumerator.urls(), eligible_link));

  std::vector<net::test_server::HttpRequest> origin_requests_after_prerender =
      origin_server_requests();
  std::vector<net::test_server::HttpRequest> proxy_requests_after_prerender =
      proxy_server_requests();

  EXPECT_GT(proxy_requests_after_prerender.size(),
            proxy_requests_before_prerender.size());

  for (const net::test_server::HttpRequest& request :
       origin_requests_after_prerender) {
    EXPECT_FALSE(RequestHasClientHints(request));
  }

  // Check that the page's Javascript was NSP'd, but not the mainframe.
  bool found_nsp_javascript = false;
  bool found_nsp_mainframe = false;
  bool found_image = false;
  for (size_t i = origin_requests_before_prerender.size();
       i < origin_requests_after_prerender.size(); ++i) {
    net::test_server::HttpRequest request = origin_requests_after_prerender[i];

    // prefetch_page.html sets a cookie on its response and we should see it
    // here.
    auto cookie_iter = request.headers.find("Cookie");
    ASSERT_FALSE(cookie_iter == request.headers.end());
    EXPECT_EQ(cookie_iter->second, "type=ChocolateChip");

    GURL nsp_url = request.GetURL();
    found_nsp_javascript |=
        nsp_url.path() == "/prefetch/prefetch_proxy/prefetch.js";
    found_nsp_mainframe |= nsp_url.path() == eligible_link.path();
    found_image |= nsp_url.path() == "/prefetch/prefetch_proxy/image.png";
  }
  EXPECT_TRUE(found_nsp_javascript);
  EXPECT_FALSE(found_nsp_mainframe);
  EXPECT_FALSE(found_image);

  VerifyOriginRequestsAreIsolated({
      "/prefetch/prefetch_proxy/prefetch.js",
      eligible_link.path(),
  });
  VerifyPrefetchRequestsSecPurposeHeader(
      {eligible_link.path()},
      /*are_requests_anonymous_client_ip=*/true);

  // Verify the resource load was reported to the subresource manager.
  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(browser()->profile());
  PrefetchProxySubresourceManager* manager =
      service->GetSubresourceManagerForURL(eligible_link);
  ASSERT_TRUE(manager);

  base::RunLoop().RunUntilIdle();

  std::set<GURL> expected_subresources = {
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch.js"),
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch-redirect-start.js"),
      GetOriginServerURL(
          "/prefetch/prefetch_proxy/prefetch-redirect-middle.js"),
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch-redirect-end.js"),
  };
  EXPECT_EQ(expected_subresources, manager->successfully_loaded_subresources());

  EXPECT_TRUE(CheckForResourceInIsolatedCache(
      eligible_link,
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch.js")));
  EXPECT_TRUE(CheckForResourceInIsolatedCache(
      eligible_link,
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch-redirect-end.js")));

  // Navigate to the predicted site. We expect:
  // * The mainframe HTML will not be requested from the origin server.
  // * The JavaScript will not be requested from the origin server.
  // * The prefetched JavaScript will be executed.
  // * The image will be fetched.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  std::vector<net::test_server::HttpRequest> proxy_requests_after_click =
      proxy_server_requests();

  // Nothing should have gone through the proxy.
  EXPECT_EQ(proxy_requests_after_prerender.size(),
            proxy_requests_after_click.size());

  std::vector<net::test_server::HttpRequest> origin_requests_after_click =
      origin_server_requests();

  // Only one request for the image is expected, and it should have cookies.
  EXPECT_EQ(origin_requests_after_prerender.size() + 1,
            origin_requests_after_click.size());
  net::test_server::HttpRequest request =
      origin_requests_after_click[origin_requests_after_click.size() - 1];
  EXPECT_EQ(request.GetURL().path(), "/prefetch/prefetch_proxy/image.png");
  auto cookie_iter = request.headers.find("Cookie");
  ASSERT_FALSE(cookie_iter == request.headers.end());
  EXPECT_EQ(cookie_iter->second, "type=ChocolateChip");

  // The cookie from prefetch should also be present in the CookieManager API.
  EXPECT_EQ("type=ChocolateChip",
            content::GetCookies(
                browser()->profile(), eligible_link,
                net::CookieOptions::SameSiteCookieContext::MakeInclusive()));

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.CookiesToCopy", 1, 1);

  // Check that the JavaScript ran.
  EXPECT_EQ(u"JavaScript Executed", GetWebContents()->GetTitle());

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::SECURE);

  // Navigate one more time to destroy the SubresourceManager so that its UMA is
  // recorded and to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 16 = |kPrefetchUsedNoProbeWithNSP|.
  EXPECT_EQ(absl::optional<int64_t>(16),
            GetUKMMetric(eligible_link,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));

  // In addition to "prefetch.js" and "prefetch-redirect-start.js",
  // "favicon.ico" is also counted here, because the favicon loading is
  // triggered from `FrameLoader::DidFinishNavigation()` at the end of NSP.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Subresources.NetError", net::OK, 3);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Subresources.Quantity", 4, 1);
  histogram_tester.ExpectBucketCount(
      "PrefetchProxy.Prefetch.Subresources.RespCode", 200, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.Subresources.UsedCache", true, 2);
}

IN_PROC_BROWSER_TEST_F(SpeculationPrefetchProxyTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(ConnectProxyEndtoEnd)) {
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());
  TestTabHelperObserver tab_helper_observer(tab_helper);

  GURL prefetch_url = GetOriginServerURL("/title2.html");

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());
  tab_helper_observer.SetExpectedSuccessfulURLs({prefetch_url});

  // Make sure we are on a valid referring page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetReferringPageServerURL("/search/q=blah")));
  InsertSpeculation(false, true, {prefetch_url});

  // This run loop will quit when the prefetch response has been successfully
  // done and processed.
  run_loop.Run();

  EXPECT_EQ(tab_helper->srp_metrics().prefetch_attempted_count_, 1U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_successful_count_, 1U);

  size_t starting_origin_request_count = OriginServerRequestCount();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));
  EXPECT_EQ(u"Title Of Awesomeness", GetWebContents()->GetTitle());

  VerifyOriginRequestsAreIsolated({prefetch_url.path()});
  VerifyPrefetchRequestsSecPurposeHeader(
      {prefetch_url.path()},
      /*are_requests_anonymous_client_ip=*/true);

  // The origin server should not have served this request.
  EXPECT_EQ(starting_origin_request_count, OriginServerRequestCount());

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::SECURE);
}

IN_PROC_BROWSER_TEST_F(SpeculationPrefetchProxyTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(TwoSpeculations)) {
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());
  TestTabHelperObserver tab_helper_observer(tab_helper);

  GURL prefetch_url = GetOriginServerURL("/title2.html");

  base::RunLoop run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop.QuitClosure());
  tab_helper_observer.SetExpectedSuccessfulURLs({prefetch_url});

  // Make sure we are on a valid referring page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetReferringPageServerURL("/search/q=blah")));
  InsertSpeculation(false, true, {prefetch_url});

  // This run loop will quit when the prefetch response has been successfully
  // done and processed.
  run_loop.Run();

  EXPECT_EQ(tab_helper->srp_metrics().prefetch_attempted_count_, 1U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_successful_count_, 1U);

  base::RunLoop run_loop_2;
  GURL prefetch_url_2 = GetOriginServerURL("/title1.html");
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(run_loop_2.QuitClosure());
  tab_helper_observer.SetExpectedSuccessfulURLs({prefetch_url_2});
  InsertSpeculation(false, true, {prefetch_url_2});

  // This run loop will quit when the prefetch response has been successfully
  // done and processed.
  run_loop_2.Run();

  // Verify that we de-dupe and only fetch one new URL.
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_attempted_count_, 2U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_successful_count_, 2U);
}

class PrefetchProxyPrerenderBrowserTest : public PrefetchProxyBrowserTest {
 public:
  PrefetchProxyPrerenderBrowserTest() = default;
  ~PrefetchProxyPrerenderBrowserTest() override = default;
  PrefetchProxyPrerenderBrowserTest(const PrefetchProxyPrerenderBrowserTest&) =
      delete;

  PrefetchProxyPrerenderBrowserTest& operator=(
      const PrefetchProxyPrerenderBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    prerender_test_helper_->SetUp(embedded_test_server());
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    PrefetchProxyBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrefetchProxyBrowserTest::SetUpCommandLine(command_line);
    // |prerender_test_helper_| has a ScopedFeatureList so we needed to delay
    // its creation until now because PrefetchProxyBrowserTest also uses a
    // ScopedFeatureList and initialization order matters.
    prerender_test_helper_ =
        std::make_unique<content::test::PrerenderTestHelper>(
            base::BindRepeating(
                &PrefetchProxyPrerenderBrowserTest::GetWebContents,
                base::Unretained(this)));
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return *prerender_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_test_helper_;
};

IN_PROC_BROWSER_TEST_F(PrefetchProxyPrerenderBrowserTest,
                       ShouldNotAffectPrefetchProxyTabHelperOnPrerendering) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  ASSERT_TRUE(content::SetCookie(browser()->profile(), GURL("https://foo.com"),
                                 "type=PeanutButter"));

  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  // Start prerendering to check if it affects on the prefetch proxy.
  int host_id = prerender_test_helper().AddPrerender(prerender_url);
  content::RenderFrameHost* prerender_render_frame_host =
      prerender_test_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_render_frame_host, nullptr);
  GURL prerender_render_frame_url =
      prerender_render_frame_host->GetLastCommittedURL();
  ASSERT_FALSE(tab_helper->after_srp_metrics());
  EXPECT_EQ(0U, tab_helper->srp_metrics().predicted_urls_count_);

  GURL prefetch_url("https://m.foo.com");
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(0U, tab_helper->srp_metrics().prefetch_eligible_count_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      absl::make_optional(
          PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies),
      tab_helper->after_srp_metrics()->prefetch_status_);

  // Check if the prefetched URL is different from the prerendered frame.
  EXPECT_NE(tab_helper->after_srp_metrics()->url_, prerender_render_frame_url);
}

class PrefetchProxyFencedFrameBrowserTest : public PrefetchProxyBrowserTest {
 public:
  PrefetchProxyFencedFrameBrowserTest() = default;
  ~PrefetchProxyFencedFrameBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    PrefetchProxyBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrefetchProxyBrowserTest::SetUpCommandLine(command_line);
    // |fenced_frame_helper_| has a ScopedFeatureList so we needed to delay
    // its creation until now because PrefetchProxyBrowserTest also uses a
    // ScopedFeatureList and initialization order matters.
    fenced_frame_helper_ =
        std::make_unique<content::test::FencedFrameTestHelper>();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return *fenced_frame_helper_;
  }

 private:
  std::unique_ptr<content::test::FencedFrameTestHelper> fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PrefetchProxyFencedFrameBrowserTest,
                       EnsureFencedFrameDoesNotAffectPrefetchProxyTabHelper) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  WaitForUpdatedCustomProxyConfig();

  ASSERT_TRUE(content::SetCookie(browser()->profile(), GURL("https://foo.com"),
                                 "type=PeanutButter"));

  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  // Create a fenced frame to check if it affects on the prefetch proxy.
  GURL fenced_frame_url(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);
  ASSERT_EQ(fenced_frame_url, fenced_frame_host->GetLastCommittedURL());

  ASSERT_FALSE(tab_helper->after_srp_metrics());
  EXPECT_EQ(0U, tab_helper->srp_metrics().predicted_urls_count_);

  GURL prefetch_url("https://m.foo.com");
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {prefetch_url});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, tab_helper->srp_metrics().predicted_urls_count_);
  EXPECT_EQ(0U, tab_helper->srp_metrics().prefetch_eligible_count_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), prefetch_url));

  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      absl::make_optional(
          PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies),
      tab_helper->after_srp_metrics()->prefetch_status_);

  // Check if the prefetched URL is different from the fenced frame.
  EXPECT_NE(tab_helper->after_srp_metrics()->url_, fenced_frame_url);
}

class ZeroCacheTimePrefetchProxyBrowserTest : public PrefetchProxyBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    PrefetchProxyBrowserTest::SetUpCommandLine(cmd);
  }

  void SetFeatures() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders, {
                                          {"use_speculation_rules", "false"},
                                          {"cacheable_duration", "0"},
                                      });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ZeroCacheTimePrefetchProxyBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PrefetchAfterCacheExpiration)) {
  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);

  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  base::HistogramTester histogram_tester;
  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link});

  // This run loop will quit when the prefetch response has been
  // successfully done and processed.
  prefetch_run_loop.Run();

  // Navigate to the predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));

  // Navigate again to trigger UKM recording.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // 30 = |kPrefetchIsStale|.
  EXPECT_EQ(absl::optional<int64_t>(30),
            GetUKMMetric(eligible_link,
                         ukm::builders::PrefetchProxy_AfterSRPClick::kEntryName,
                         ukm::builders::PrefetchProxy_AfterSRPClick::
                             kSRPClickPrefetchStatusName));

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.CookiesToCopy", 0);
}

class IndividualNetworkContextsPrefetchProxyBrowserTest
    : public PrefetchProxyBrowserTest {
 public:
  void SetFeatures() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        "isolated-prerender-unlimited-prefetches");

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolatePrerenders,
        {{"use_speculation_rules", "false"},
         {"use_individual_network_contexts", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test confirms that, when using separate network contexts for each
// prefetch, we do not encounter the cookie collision issue that is possible
// when using a single network context for all prefetches from the same main
// frame. See the SingleNetworkContextPrefetchProxyBrowserTest.CookieCollision
// test above.
IN_PROC_BROWSER_TEST_F(IndividualNetworkContextsPrefetchProxyBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(NoCookieCollision)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  GURL starting_page = GetOriginServerURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  // The two possible prefetches from the same origin with different cookies.
  GURL eligible_link_1 =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");
  GURL eligible_link_2 = GetOriginServerURL(
      "/prefetch/prefetch_proxy/prefetch_page_different_cookie.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs(
      {eligible_link_1, eligible_link_2});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  GURL doc_url("https://www.google.com/search?q=test");
  MakeNavigationPrediction(doc_url, {eligible_link_1, eligible_link_2});

  // This run loop will quit when all the prefetch responses have been
  // successfully done and processed.
  prefetch_run_loop.Run();

  std::vector<net::test_server::HttpRequest> origin_requests_after_prefetch =
      origin_server_requests();

  base::HistogramTester histogram_tester;

  // Navigate to first predicted site.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link_1));

  std::vector<net::test_server::HttpRequest> origin_requests_after_click =
      origin_server_requests();

  EXPECT_GT(origin_requests_after_click.size(),
            origin_requests_after_prefetch.size());

  // Check the cookies used when requesting the image subresource.
  bool inspected_image_request = false;
  for (size_t i = origin_requests_after_prefetch.size();
       i < origin_requests_after_click.size(); ++i) {
    net::test_server::HttpRequest request = origin_requests_after_click[i];
    if (request.GetURL().path() != "/prefetch/prefetch_proxy/image.png") {
      continue;
    }
    inspected_image_request = true;

    auto cookie_iter = request.headers.find("Cookie");
    ASSERT_FALSE(cookie_iter == request.headers.end());

    // Since each prefetch uses its own network context, when |eligible_link_1|
    // is committed, then we only copy over the cookies added from that prefetch
    // to the default network context. Any cookies from |eligible_link_2| are
    // discarded.
    EXPECT_EQ(cookie_iter->second, "type=ChocolateChip");
  }

  EXPECT_TRUE(inspected_image_request);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.CookiesToCopy", 1, 1);

  EXPECT_EQ("type=ChocolateChip",
            content::GetCookies(
                browser()->profile(), eligible_link_1,
                net::CookieOptions::SameSiteCookieContext::MakeInclusive()));
}

class SpeculationOnlyPrivatePrefetchesPrefetchProxyTest
    : public PrefetchProxyBrowserTest {
 public:
  void SetFeatures() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kIsolatePrerenders,
          {
              {"use_speculation_rules", "true"},
              {"max_srp_prefetches", "3"},
              {"use_individual_network_contexts", "true"},
              {"support_non_private_prefetches", "false"},
          }},
         {blink::features::kLightweightNoStatePrefetch, {}},
         {blink::features::kSpeculationRulesPrefetchProxy, {}}},
        {{features::kLazyImageLoading}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SpeculationOnlyPrivatePrefetchesPrefetchProxyTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(SkipNonPrivatePrefetches)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL starting_page = GetReferringPageServerURL("/search/q=blah");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  // Specify that this speculation does not require use of the prefetch
  // proxy. Since non-private prefetches are disabled, then the speculation rule
  // for |eligible_link| should be ignored, and therefore we won't prefetch
  // |eligible_link| not be prefetched.
  InsertSpeculation(false, false, {eligible_link});
  base::RunLoop().RunUntilIdle();

  // Check that no prefetch was attempted.
  EXPECT_EQ(tab_helper->srp_metrics().predicted_urls_count_, 0U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_attempted_count_, 0U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_successful_count_, 0U);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));
  EXPECT_FALSE(tab_helper->after_srp_metrics());
}

class SpeculationNonPrivatePrefetchesPrefetchProxyTest
    : public PrefetchProxyBrowserTest {
 public:
  void SetFeatures() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kIsolatePrerenders,
          {
              {"use_speculation_rules", "true"},
              {"max_srp_prefetches", "3"},
              {"use_individual_network_contexts", "true"},
              {"support_non_private_prefetches", "true"},
          }},
         {blink::features::kLightweightNoStatePrefetch, {}},
         {blink::features::kSpeculationRulesPrefetchProxy, {}}},
        {{features::kLazyImageLoading}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SpeculationNonPrivatePrefetchesPrefetchProxyTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(SuccessfulNonPrivateCrossOriginPrefetch)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL starting_page = GetReferringPageServerURL("/search/q=blah");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  // Specify that this speculation does not require use of the prefetch
  // proxy.
  InsertSpeculation(false, false, {eligible_link});

  prefetch_run_loop.Run();

  EXPECT_EQ(tab_helper->srp_metrics().prefetch_attempted_count_, 1U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_successful_count_, 1U);

  // The prefetch requests shouldn't use the proxy and should  go directly to
  // the origin server.
  EXPECT_EQ(proxy_server_requests().size(), 0U);

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));
  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      tab_helper->after_srp_metrics()->prefetch_status_,
      absl::make_optional(PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe));

  // The cookies from the prefetch should be copied from the isolated network
  // context to the default network context.
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.CookiesToCopy", 1, 1);

  EXPECT_EQ("type=ChocolateChip",
            content::GetCookies(
                browser()->profile(), eligible_link,
                net::CookieOptions::SameSiteCookieContext::MakeInclusive()));

  VerifyPrefetchRequestsSecPurposeHeader(
      {eligible_link.path()},
      /*are_requests_anonymous_client_ip=*/false);

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::SECURE);
}

IN_PROC_BROWSER_TEST_F(
    SpeculationNonPrivatePrefetchesPrefetchProxyTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(SuccessfulNonPrivateSameOriginPrefetch)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL starting_page = GetOriginServerURL("/search/q=blah");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), starting_page));
  content::OverrideLastCommittedOrigin(GetWebContents()->GetPrimaryMainFrame(),
                                       url::Origin::Create(starting_page));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetOriginServerURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  // Specify that this speculation does not require use of the prefetch proxy..
  InsertSpeculation(false, false, {eligible_link});

  prefetch_run_loop.Run();

  EXPECT_EQ(tab_helper->srp_metrics().prefetch_attempted_count_, 1U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_successful_count_, 1U);

  // The prefetch requests shouldn't use the proxy and should  go directly to
  // the origin server.
  EXPECT_EQ(proxy_server_requests().size(), 0U);

  // The prefetch should be made using the default network context, so the
  // cookie should be present once the prefetch is complete.
  EXPECT_EQ("type=ChocolateChip",
            content::GetCookies(
                browser()->profile(), eligible_link,
                net::CookieOptions::SameSiteCookieContext::MakeInclusive()));

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));
  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      tab_helper->after_srp_metrics()->prefetch_status_,
      absl::make_optional(PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe));

  // The prefetch should be made using the default network context so there
  // should be no copying of cookies.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.CookiesToCopy", 0, 1);

  VerifyPrefetchRequestsSecPurposeHeader(
      {eligible_link.path()},
      /*are_requests_anonymous_client_ip=*/false);

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::SECURE);
}

IN_PROC_BROWSER_TEST_F(SpeculationNonPrivatePrefetchesPrefetchProxyTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(
                           SuccessfulNonPrivateSameOriginPrefetchLocalhost)) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL localhost_url = GetLocalhostURL("/search/q=blah");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), localhost_url));
  content::OverrideLastCommittedOrigin(GetWebContents()->GetPrimaryMainFrame(),
                                       url::Origin::Create(localhost_url));
  WaitForUpdatedCustomProxyConfig();

  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetWebContents());

  GURL eligible_link =
      GetLocalhostURL("/prefetch/prefetch_proxy/prefetch_page.html");

  TestTabHelperObserver tab_helper_observer(tab_helper);
  tab_helper_observer.SetExpectedSuccessfulURLs({eligible_link});

  base::RunLoop prefetch_run_loop;
  tab_helper_observer.SetOnPrefetchSuccessfulClosure(
      prefetch_run_loop.QuitClosure());

  // Specify that this speculation does not require use of the prefetch proxy..
  InsertSpeculation(false, false, {eligible_link});

  prefetch_run_loop.Run();

  EXPECT_EQ(tab_helper->srp_metrics().prefetch_attempted_count_, 1U);
  EXPECT_EQ(tab_helper->srp_metrics().prefetch_successful_count_, 1U);

  // The prefetch requests shouldn't use the proxy and should  go directly to
  // the origin server.
  EXPECT_EQ(proxy_server_requests().size(), 0U);

  // The prefetch should be made using the default network context, so the
  // cookie should be present once the prefetch is complete.
  EXPECT_EQ("type=ChocolateChip",
            content::GetCookies(
                browser()->profile(), eligible_link,
                net::CookieOptions::SameSiteCookieContext::MakeInclusive()));

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), eligible_link));
  ASSERT_TRUE(tab_helper->after_srp_metrics());
  EXPECT_EQ(
      tab_helper->after_srp_metrics()->prefetch_status_,
      absl::make_optional(PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe));

  // The prefetch should be made using the default network context so there
  // should be no copying of cookies.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.CookiesToCopy", 0, 1);

  SecurityStateTabHelper* security_state_tab_helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  EXPECT_EQ(security_state_tab_helper->GetSecurityLevel(),
            security_state::NONE);
}
