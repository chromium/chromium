// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/google/core/common/google_switches.h"
#include "components/google/core/common/google_util.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/optimization_guide/core/hints_component_info.h"
#include "components/optimization_guide/core/hints_component_util.h"
#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/core/test_hints_component_creator.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/hashing.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "third_party/blink/public/common/features.h"

namespace {

constexpr char kGoogleHost[] = "www.google.com";

constexpr char kGoogleSearchUrlPath[] = "/search?q=search_results_page.html";

// Modifies |relative_url|:
// Scheme of the returned URL matches the scheme of the |server|.
// Host of the returned URL matches kGoogleHost.
// Port number of the returned URL matches the port at which |server| is
// listening.
// Path of the returned URL is set to |relative_url|.
GURL GetURLWithGoogleHost(net::EmbeddedTestServer* server,
                          const std::string& relative_url) {
  GURL server_base_url = server->base_url();
  GURL base_url =
      GURL(base::StrCat({server_base_url.scheme(), "://", kGoogleHost, ":",
                         server_base_url.port()}));
  EXPECT_TRUE(base_url.is_valid()) << base_url.possibly_invalid_spec();
  return base_url.Resolve(relative_url);
}

// Handles the server request to Google search URL.
std::unique_ptr<net::test_server::HttpResponse> HandleGoogleSearchUrlRequest(
    const net::test_server::HttpRequest& request) {
  if (base::EqualsCaseInsensitiveASCII(request.relative_url,
                                       kGoogleSearchUrlPath)) {
    // Serve the SRP file.
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    std::string file_contents;
    base::FilePath test_data_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
    if (base::ReadFileToString(test_data_directory.AppendASCII(
                                   "previews/search_results_page.html"),
                               &file_contents)) {
      response->set_content(file_contents);
      response->set_code(net::HTTP_OK);
      return std::move(response);
    }
  }
  return nullptr;
}

}  // namespace

// This test class sets up everything but does not enable any
// HintsFetcher-related features. The parameter selects whether the
// OptimizationGuideKeyedService is enabled (tests should pass in the same way
// for both cases).
class HintsFetcherDisabledBrowserTest : public InProcessBrowserTest {
 public:
  HintsFetcherDisabledBrowserTest() = default;

  HintsFetcherDisabledBrowserTest(const HintsFetcherDisabledBrowserTest&) =
      delete;
  HintsFetcherDisabledBrowserTest& operator=(
      const HintsFetcherDisabledBrowserTest&) = delete;

  ~HintsFetcherDisabledBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_2G);

    // Ensure that kGoogleHost resolves to the localhost where the embedded test
    // server is listening.
    host_resolver()->AddRule("*", "127.0.0.1");

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUp() override {
    origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    net::EmbeddedTestServer::ServerCertificateConfig origin_server_cert_config;
    origin_server_cert_config.dns_names = {kGoogleHost};
    origin_server_cert_config.ip_addresses = {net::IPAddress::IPv4Localhost()};
    origin_server_->SetSSLConfig(origin_server_cert_config);
    origin_server_->RegisterRequestHandler(
        base::BindRepeating(&HandleGoogleSearchUrlRequest));
    origin_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");

    ASSERT_TRUE(origin_server_->Start());

    https_url_ = origin_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(https_url().SchemeIs(url::kHttpsScheme));

    search_results_page_url_ =
        GetURLWithGoogleHost(origin_server_.get(), kGoogleSearchUrlPath);

    hints_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);

    net::EmbeddedTestServer::ServerCertificateConfig hints_server_cert_config;
    hints_server_cert_config.dns_names = {
        GURL(optimization_guide::kOptimizationGuideServiceGetHintsDefaultURL)
            .host()};
    hints_server_->SetSSLConfig(hints_server_cert_config);
    hints_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    hints_server_->RegisterRequestHandler(base::BindRepeating(
        &HintsFetcherDisabledBrowserTest::HandleGetHintsRequest,
        base::Unretained(this)));

    ASSERT_TRUE(hints_server_->Start());

    std::map<std::string, std::string> params;
    params["random_anchor_sampling_period"] = "1";
    params["traffic_client_enabled_percent"] = "100";
    param_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor, params);

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("purge_hint_cache_store");

    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableCheckingUserPermissionsForTesting);

    // Set up OptimizationGuideServiceURL, this does not enable HintsFetching,
    // only provides the URL.
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceGetHintsURL,
        hints_server_
            ->GetURL(GURL(optimization_guide::
                              kOptimizationGuideServiceGetHintsDefaultURL)
                         .host(),
                     "/")
            .spec());
    cmd->AppendSwitchASCII("force-variation-ids", "4");

    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "example1.com, example2.com");

    cmd->AppendSwitch(optimization_guide::switches::kFetchHintsOverrideTimer);

    // Ignore the port numbers for the Google Search URL check.
    cmd->AppendSwitch(switches::kIgnoreGooglePortNumbers);
  }

  // Creates hint data for the |hint_setup_url|'s so that the fetching of the
  // hints is triggered.
  void SetUpComponentUpdateHints(const GURL& hint_setup_url) {
    optimization_guide::RetryForHistogramUntilCountReached(
        GetHistogramTester(),
        "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {hint_setup_url.host()}, "*");

    base::HistogramTester histogram_tester;

    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(component_info);

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester,
        optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);
  }

  void SetNetworkConnectionOffline() {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
  }

  void SetNetworkConnectionOnline() {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_2G);
  }

  void SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType response_type) {
    response_type_ = response_type;
  }

  void LoadHintsForUrl(const GURL& url) {
    base::HistogramTester histogram_tester;

    // Navigate to |url| to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
  }

  const GURL& https_url() const { return https_url_; }
  const base::HistogramTester* GetHistogramTester() {
    return &histogram_tester_;
  }

  const GURL& search_results_page_url() const {
    return search_results_page_url_;
  }

  void SetExpectedHintsRequestForHostsAndUrls(
      const base::flat_set<std::string>& hosts_or_urls) {
    base::AutoLock lock(lock_);
    expect_hints_request_for_hosts_and_urls_ = hosts_or_urls;
  }

  size_t count_hints_requests_received() {
    base::AutoLock lock(lock_);
    return count_hints_requests_received_;
  }

  void increase_count_hints_requests_received() {
    {
      base::AutoLock lock(lock_);
      ++count_hints_requests_received_;
    }

    if (quit_closure_) {
      content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                   std::move(quit_closure_));
    }
  }

  void SetExpectedBearerAccessToken(
      const std::string& expected_bearer_access_token) {
    expected_bearer_access_token_ = expected_bearer_access_token;
  }

  void WaitUntilHintsFetcherRequestReceived() {
    {
      // Acquire the |lock_| inside to avoid starving other consumers of the
      // lock.
      base::AutoLock lock(lock_);
      if (count_hints_requests_received_ > 0)
        return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ResetCountHintsRequestsReceived() {
    base::AutoLock lock(lock_);
    count_hints_requests_received_ = 0;
  }

  // Triggers nostate prefetch of |url|.
  void TriggerNoStatePrefetch(const GURL& url) {
    prerender::NoStatePrefetchManager* no_state_prefetch_manager =
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            browser()->profile());
    ASSERT_TRUE(no_state_prefetch_manager);

    prerender::test_utils::TestNoStatePrefetchContentsFactory*
        no_state_prefetch_contents_factory =
            new prerender::test_utils::TestNoStatePrefetchContentsFactory();
    no_state_prefetch_manager->SetNoStatePrefetchContentsFactoryForTest(
        no_state_prefetch_contents_factory);

    content::SessionStorageNamespace* storage_namespace =
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetController()
            .GetDefaultSessionStorageNamespace();
    ASSERT_TRUE(storage_namespace);

    std::unique_ptr<prerender::test_utils::TestPrerender> test_prerender =
        no_state_prefetch_contents_factory->ExpectNoStatePrefetchContents(
            prerender::FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);

    std::unique_ptr<prerender::NoStatePrefetchHandle> no_state_prefetch_handle =
        no_state_prefetch_manager->AddSameOriginSpeculation(
            url, storage_namespace, gfx::Size(640, 480),
            url::Origin::Create(url));
    ASSERT_EQ(no_state_prefetch_handle->contents(), test_prerender->contents());

    // The final status may be either  FINAL_STATUS_NOSTATE_PREFETCH_FINISHED or
    // FINAL_STATUS_RECENTLY_VISITED.
    test_prerender->contents()->set_skip_final_checks(true);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  std::unique_ptr<net::EmbeddedTestServer> hints_server_;
  optimization_guide::HintsFetcherRemoteResponseType response_type_ =
      optimization_guide::HintsFetcherRemoteResponseType::kSuccessful;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleOriginRequest(
      const net::test_server::HttpRequest& request) {
    EXPECT_EQ(request.method, net::test_server::METHOD_GET);
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);

    return std::move(response);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleGetHintsRequest(
      const net::test_server::HttpRequest& request) {
    increase_count_hints_requests_received();
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    // If the request is a GET, it corresponds to a navigation so return a
    // normal response.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_NE(request.headers.end(), request.headers.find("X-Client-Data"));

    EXPECT_EQ(expected_bearer_access_token_.empty(),
              !base::Contains(request.headers,
                              net::HttpRequestHeaders::kAuthorization));
    if (!expected_bearer_access_token_.empty()) {
      EXPECT_EQ(expected_bearer_access_token_,
                request.headers.at(net::HttpRequestHeaders::kAuthorization));
    }

    // Make sure only one of API key or access token is sent.
    EXPECT_EQ(base::Contains(request.headers, "X-Goog-Api-Key"),
              expected_bearer_access_token_.empty());

    optimization_guide::proto::GetHintsRequest hints_request;
    EXPECT_TRUE(hints_request.ParseFromString(request.content));
    EXPECT_FALSE(hints_request.hosts().empty() && hints_request.urls().empty());
    EXPECT_GE(optimization_guide::features::
                  MaxHostsForOptimizationGuideServiceHintsFetch(),
              static_cast<size_t>(hints_request.hosts().size()));

    // Only verify the hints if there are hosts in the request.
    if (!hints_request.hosts().empty())
      VerifyHintsMatchExpectedHostsAndUrls(hints_request);

    if (response_type_ ==
        optimization_guide::HintsFetcherRemoteResponseType::kSuccessful) {
      response->set_code(net::HTTP_OK);

      optimization_guide::proto::GetHintsResponse get_hints_response;

      optimization_guide::proto::Hint* hint = get_hints_response.add_hints();
      hint->set_key_representation(optimization_guide::proto::HOST);
      hint->set_key(search_results_page_url_.host());
      hint->add_allowlisted_optimizations()->set_optimization_type(
          optimization_guide::proto::OptimizationType::NOSCRIPT);
      optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
      page_hint->set_page_pattern("page pattern");

      std::string serialized_request;
      get_hints_response.SerializeToString(&serialized_request);
      response->set_content(serialized_request);
    } else if (response_type_ ==
               optimization_guide::HintsFetcherRemoteResponseType::
                   kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);

    } else if (response_type_ ==
               optimization_guide::HintsFetcherRemoteResponseType::kMalformed) {
      response->set_code(net::HTTP_OK);

      std::string serialized_request = "Not a proto";
      response->set_content(serialized_request);
    } else if (response_type_ ==
               optimization_guide::HintsFetcherRemoteResponseType::kHung) {
      return std::make_unique<net::test_server::HungResponse>();
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    return std::move(response);
  }

  // Verifies that the hosts present in |hints_request| match the expected set
  // of hosts present in |expect_hints_request_for_hosts_|. The ordering of the
  // hosts in not matched.
  void VerifyHintsMatchExpectedHostsAndUrls(
      const optimization_guide::proto::GetHintsRequest& hints_request) const {
    if (!expect_hints_request_for_hosts_and_urls_)
      return;

    base::flat_set<std::string> hosts_and_urls_requested;
    for (const auto& host : hints_request.hosts()) {
      hosts_and_urls_requested.insert(host.host());
    }
    for (const auto& url : hints_request.urls()) {
      // TODO(crbug.com/40118423):  Remove normalization step once nav predictor
      // provides predictable URLs.
      hosts_and_urls_requested.insert(GURL(url.url()).GetAsReferrer().spec());
    }

    EXPECT_EQ(expect_hints_request_for_hosts_and_urls_.value().size(),
              hosts_and_urls_requested.size());
    for (const auto& host_or_url :
         expect_hints_request_for_hosts_and_urls_.value()) {
      hosts_and_urls_requested.erase(host_or_url);
    }
    EXPECT_EQ(0u, hosts_and_urls_requested.size());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(origin_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(hints_server_->ShutdownAndWaitUntilComplete());

    InProcessBrowserTest::TearDownOnMainThread();
  }

  base::test::ScopedFeatureList param_feature_list_;

  GURL https_url_;

  GURL search_results_page_url_;

  base::HistogramTester histogram_tester_;

  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;

  base::Lock lock_;

  // Guarded by |lock_|.
  // Count of hints requests received so far by |hints_server_|.
  size_t count_hints_requests_received_ = 0;

  base::OnceClosure quit_closure_;

  // Guarded by |lock_|. Set of hosts and URLs for which a hints request is
  // expected to arrive. This set is verified to match with the set of hosts and
  // URLs present in the hints request. If null, then the verification is not
  // done.
  std::optional<base::flat_set<std::string>>
      expect_hints_request_for_hosts_and_urls_;

  // The expected authorization header holding the bearer access token.
  std::string expected_bearer_access_token_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(HintsFetcherDisabledBrowserTest, HintsFetcherDisabled) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Expect that the histogram for HintsFetcher to be 0 because the OnePlatform
  // is not enabled.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}

// This test class enables OnePlatform Hints.
class HintsFetcherBrowserTest : public HintsFetcherDisabledBrowserTest {
 public:
  HintsFetcherBrowserTest() = default;

  HintsFetcherBrowserTest(const HintsFetcherBrowserTest&) = delete;
  HintsFetcherBrowserTest& operator=(const HintsFetcherBrowserTest&) = delete;

  ~HintsFetcherBrowserTest() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {optimization_guide::features::kOptimizationHints, {}},
        {
            optimization_guide::features::kRemoteOptimizationGuideFetching,
            {{"max_concurrent_page_navigation_fetches", "2"},
             {"max_urls_for_optimization_guide_service_hints_fetch", "30"},
             // This delay is set to 0 to avoid flaky timeouts in
             // HintsFetcherSearchPagePrerenderingBrowserTest.
             {"onload_delay_for_hints_fetching_ms", "0"},
             {"batch_update_hints_for_top_hosts", "true"}},
        }};
    PopulateEnabledFeatures(&enabled_features);

    // Enable OptimizationHintsFetching with |kRemoteOptimizationGuideFetching|.
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features_);
    // Call to inherited class to match same set up with feature flags added.
    HintsFetcherDisabledBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Register an optimization type, so hints will be fetched at page
    // navigation.
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});

    HintsFetcherDisabledBrowserTest::SetUpOnMainThread();
  }

  // Allows subclasses to expand on the features to enable.
  virtual void PopulateEnabledFeatures(
      std::vector<base::test::FeatureRefAndParams>* enabled_features) {}

  optimization_guide::TopHostProvider* top_host_provider() {
    OptimizationGuideKeyedService* keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    return keyed_service->GetTopHostProvider();
  }

  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
          callback) {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->CanApplyOptimizationOnDemand(
            urls, optimization_types,
            optimization_guide::proto::CONTEXT_BOOKMARKS, callback);
  }

 protected:
  std::vector<base::test::FeatureRef> disabled_features_;
};

// This test creates new browser with no profile and loads a random page with
// the feature flags for OptimizationHintsFetching. We confirm that the
// TopHostProvider is called and does not crash by checking UMA
// histograms for the total number of TopEngagementSites and
// the total number of sites returned controlled by the experiments flag
// |max_oneplatform_update_hosts|.
IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest, HintsFetcherEnabled) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcherFetchedHintsLoaded) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1),
            1);

  LoadHintsForUrl(https_url());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url()));

  // Verifies that the fetched hint is just used in memory and nothing is
  // loaded.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintCache.HintType.Loaded", 0);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcherWithResponsesSuccessful) {
  SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType::kSuccessful);

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcherWithResponsesUnsuccessful) {
  SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType::kUnsuccessful);

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status",
      net::HTTP_NOT_FOUND, 1);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 0);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcherWithResponsesMalformed) {
  SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType::kMalformed);

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 0);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcherWithResponsesUnsuccessfulAtNavigationTime) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType::kUnsuccessful);

  // Set the connection online to force a fetch at navigation time.
  SetNetworkConnectionOnline();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://unsuccessful.com/")));

  // We expect that we requested hints for 1 URL.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1),
            1);
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    HintsFetcherWithResponsesHungShouldRecordWhenActiveRequestCanceled) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  SetResponseType(optimization_guide::HintsFetcherRemoteResponseType::kHung);

  // Set the connection online to force a fetch at navigation time.
  SetNetworkConnectionOnline();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://hung.com/1")));
  WaitUntilHintsFetcherRequestReceived();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://hung.com/2")));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://hung.com/3")));

  // We expect that one request was canceled.
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
      "PageNavigation",
      optimization_guide::FetcherRequestStatus::kRequestCanceled, 1);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       DISABLED_HintsFetcherClearFetchedHints) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as OnePlatform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1),
            1);

  LoadHintsForUrl(https_url());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url()));

  // Verifies that the fetched hint is used in-memory and no hint is loaded
  // from store.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintCache.HintType.Loaded", 0);

  // Wipe the browser history - clear all the fetched hints.
  browser()->profile()->Wipe();

  // Wait until hint cache stabilizes and clears all the fetched hints.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // Try to load the same hint to confirm fetched hints are no longer there.
  LoadHintsForUrl(https_url());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url()));

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(optimization_guide::OptimizationGuideStore::
                           StoreEntryType::kComponentHint),
      1);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest, HintsFetcherOverrideTimer) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kFetchHintsOverride, "whatever.com");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kFetchHintsOverrideTimer);

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as OnePlatform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // There should be 2 sites in the engagement service.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);
  // There should have been 1 hint returned in the response.
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);

  LoadHintsForUrl(https_url());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url()));

  // Verifies that the fetched hint is used from memory and no hints are loaded.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintCache.HintType.Loaded", 0);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest, HintsFetcherFetches) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as hints fetching is enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       OnDemandFetchRepeatedlyNoCache) {
  SetNetworkConnectionOnline();

  SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType::kSuccessful);

  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  ogks->RegisterOptimizationTypes(
      {optimization_guide::proto::OptimizationType::NOSCRIPT});

  GURL url_with_no_hints("https://urlwithnohints.com/notcached");

  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    CanApplyOptimizationOnDemand(
        {url_with_no_hints},
        {optimization_guide::proto::OptimizationType::NOSCRIPT},
        base::BindRepeating(
            [](base::RunLoop* run_loop, const GURL& url,
               const base::flat_map<
                   optimization_guide::proto::OptimizationType,
                   optimization_guide::OptimizationGuideDecisionWithMetadata>&
                   decisions) {
              // Expect one decision per requested type.
              EXPECT_EQ(decisions.size(), 1u);
              auto it = decisions.find(
                  optimization_guide::proto::OptimizationType::NOSCRIPT);
              EXPECT_NE(it, decisions.end());
              EXPECT_EQ(it->second.decision,
                        optimization_guide::OptimizationGuideDecision::kFalse);

              run_loop->Quit();
            },
            run_loop.get()));
    run_loop->Run();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
        "Bookmarks",
        optimization_guide::FetcherRequestStatus::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    CanApplyOptimizationOnDemand(
        {url_with_no_hints},
        {optimization_guide::proto::OptimizationType::NOSCRIPT},
        base::BindRepeating(
            [](base::RunLoop* run_loop, const GURL& url,
               const base::flat_map<
                   optimization_guide::proto::OptimizationType,
                   optimization_guide::OptimizationGuideDecisionWithMetadata>&
                   decisions) {
              // Expect one decision per requested type.
              EXPECT_EQ(decisions.size(), 1u);
              auto it = decisions.find(
                  optimization_guide::proto::OptimizationType::NOSCRIPT);
              EXPECT_NE(it, decisions.end());
              EXPECT_EQ(it->second.decision,
                        optimization_guide::OptimizationGuideDecision::kFalse);

              run_loop->Quit();
            },
            run_loop.get()));
    run_loop->Run();

    // Second time should refetch since on-demand always fetches.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
        "Bookmarks",
        optimization_guide::FetcherRequestStatus::kSuccess, 1);
  }
}

// Test that the hints are fetched at the time of the navigation.
IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcher_NavigationFetch_URLKeyedNotRefetched) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  optimization_guide::RetryForHistogramUntilCountReached(
      histogram_tester,
      optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);
  EXPECT_EQ(1u, count_hints_requests_received());

  // Enable the connection so the page navigation race fetch is initiated.
  SetNetworkConnectionOnline();
  std::string full_url("https://foo.com/test/");
  {
    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as a race for both the host and the URL.
    base::flat_set<std::string> expected_request;
    expected_request.insert(GURL(full_url).host());
    expected_request.insert(GURL(full_url).spec());
    SetExpectedHintsRequestForHostsAndUrls(expected_request);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(full_url)));

    EXPECT_EQ(2u, count_hints_requests_received());
    optimization_guide::RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
    histogram_tester->ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHostAndURL,
        1);
  }

  // Navigate again to the same webpage, no race should occur.
  {
    // Navigate to a host that was recently fetched. It
    // should be recorded as covered by the hints fetcher.
    base::flat_set<std::string> expected_request;
    SetExpectedHintsRequestForHostsAndUrls(expected_request);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(full_url)));

    // With URL-keyed Hints, every unique URL navigated to will result in a
    // hints fetch if racing is enabled and allowed.
    EXPECT_EQ(2u, count_hints_requests_received());
    optimization_guide::RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        2);

    // Only the host will be attempted to race, the fetcher should block the
    // host from being fetched.
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchNotAttempted,
        1);
  }

  // Incognito page loads should not initiate any fetches.
  {
    base::HistogramTester incognito_histogram_tester;
    // Instantiate off the record Optimization Guide Service.
    OptimizationGuideKeyedServiceFactory::GetForProfile(
        browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true))
        ->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});

    Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(otr_browser, GURL(full_url)));

    // Make sure no additional hints requests were received.
    optimization_guide::RetryForHistogramUntilCountReached(
        &incognito_histogram_tester,
        optimization_guide::kLoadedHintLocalHistogramString, 1);
    EXPECT_EQ(2u, count_hints_requests_received());

    incognito_histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus", 0);
  }
}

// Test that the hints are fetched at the time of the navigation.
IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    HintsFetcher_NavigationFetch_FetchWithNewlyRegisteredOptType) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  optimization_guide::RetryForHistogramUntilCountReached(
      histogram_tester,
      optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);
  EXPECT_EQ(1u, count_hints_requests_received());

  // Enable the network connection so the page navigation race fetch is
  // initiated.
  SetNetworkConnectionOnline();
  std::string full_url("https://foo.com/test/");
  {
    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as a race for both the host and the URL.
    base::flat_set<std::string> expected_request;
    expected_request.insert(GURL(full_url).host());
    expected_request.insert(GURL(full_url).spec());
    SetExpectedHintsRequestForHostsAndUrls(expected_request);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(full_url)));

    EXPECT_EQ(2u, count_hints_requests_received());
    optimization_guide::RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
    histogram_tester->ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHostAndURL,
        1);
  }

  OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext()))
      ->RegisterOptimizationTypes(
          {optimization_guide::proto::COMPRESS_PUBLIC_IMAGES});

  // Navigate again to the same webpage, the race should occur because the
  // hints have been cleared.
  {
    // Navigate to a host that was recently fetched. It
    // should be recorded as covered by the hints fetcher.
    base::flat_set<std::string> expected_request;
    expected_request.insert(GURL(full_url).host());
    SetExpectedHintsRequestForHostsAndUrls(expected_request);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(full_url)));

    // With URL-keyed Hints, every unique URL navigated to will result in a
    // hints fetch if racing is enabled and allowed.
    EXPECT_EQ(3u, count_hints_requests_received());
    optimization_guide::RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        2);

    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHost,
        1);
  }
}

// Test that the hints are fetched at the time of the navigation.
IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    HintsFetcher_NavigationFetch_CacheNotClearedOnLaunchedOptTypes) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  optimization_guide::RetryForHistogramUntilCountReached(
      histogram_tester,
      optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);
  EXPECT_EQ(1u, count_hints_requests_received());

  // Enable the network connection so the page navigation race fetch is
  // initiated.
  std::string full_url("https://foo.com/test/");
  {
    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as a race for both the host and the URL.
    base::flat_set<std::string> expected_request;
    expected_request.insert(GURL(full_url).host());
    expected_request.insert(GURL(full_url).spec());
    SetExpectedHintsRequestForHostsAndUrls(expected_request);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(full_url)));

    EXPECT_EQ(2u, count_hints_requests_received());
    optimization_guide::RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
    histogram_tester->ExpectUniqueSample(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHostAndURL,
        1);
  }

  OptimizationGuideKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext()))
      ->RegisterOptimizationTypes(
          {optimization_guide::proto::DEFER_ALL_SCRIPT});

  // Navigate again to the same webpage, no race should occur.
  {
    // Navigate to a host that was recently fetched. It
    // should be recorded as covered by the hints fetcher.
    base::flat_set<std::string> expected_request;
    SetExpectedHintsRequestForHostsAndUrls(expected_request);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(full_url)));

    // With URL-keyed Hints, every unique URL navigated to will result in a
    // hints fetch if racing is enabled and allowed.
    EXPECT_EQ(2u, count_hints_requests_received());
    optimization_guide::RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        2);

    // Only the host will be attempted to race, the fetcher should block the
    // host from being fetched.
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchNotAttempted,
        1);
  }
}

class HintsFetcherPre3pcdBrowserTest : public HintsFetcherBrowserTest {
 public:
  HintsFetcherPre3pcdBrowserTest() {
    disabled_features_.push_back(
        content_settings::features::kTrackingProtection3pcd);
  }
};

IN_PROC_BROWSER_TEST_F(HintsFetcherPre3pcdBrowserTest,
                       HintsFetcherDoesntFetchOnNSP) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as hints fetching is enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);

  // Initiate a NSP.
  SetNetworkConnectionOnline();
  ResetCountHintsRequestsReceived();
  TriggerNoStatePrefetch(GURL("https://someotherurl.com/"));

  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus", 0);
}

class HintsFetcherSearchPageBrowserTest : public HintsFetcherBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableFetchingHintsAtNavigationStartForTesting);
    HintsFetcherBrowserTest::SetUpCommandLine(cmd);
  }
};

// TODO(crbug.com/40919396): De-leakify and re-enable.
#if BUILDFLAG(IS_LINUX) && defined(LEAK_SANITIZER)
#define MAYBE_HintsFetcher_SRP_Slow_Connection \
  DISABLED_HintsFetcher_SRP_Slow_Connection
#else
#define MAYBE_HintsFetcher_SRP_Slow_Connection HintsFetcher_SRP_Slow_Connection
#endif
IN_PROC_BROWSER_TEST_F(HintsFetcherSearchPageBrowserTest,
                       MAYBE_HintsFetcher_SRP_Slow_Connection) {
  SetNetworkConnectionOnline();

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);

  // Populate expected hosts with hosts contained in the html response of
  // search_results_page_url(). example2.com is contained in the HTML
  // response, but hints for example2.com must not be fetched since they
  // were pushed via kFetchHintsOverride switch above.
  base::flat_set<std::string> expected_hosts_and_urls;
  // Unique hosts.
  expected_hosts_and_urls.insert(GURL("https://foo.com").host());
  expected_hosts_and_urls.insert(GURL("https://example.com").host());
  expected_hosts_and_urls.insert(GURL("https://example3.com").host());
  // Unique URLs.
  expected_hosts_and_urls.insert("https://foo.com/");
  expected_hosts_and_urls.insert(
      "https://foo.com/simple_page_with_anchors.html");
  expected_hosts_and_urls.insert("https://example.com/foo.html");
  expected_hosts_and_urls.insert("https://example.com/bar.html");
  expected_hosts_and_urls.insert("https://example.com/baz.html");
  expected_hosts_and_urls.insert("https://example2.com/foo.html");
  expected_hosts_and_urls.insert("https://example3.com/foo.html");
  SetExpectedHintsRequestForHostsAndUrls(expected_hosts_and_urls);

  histogram_tester->ExpectTotalCount(
      optimization_guide::kLoadedHintLocalHistogramString, 0);

  // Navigate to a host not in the seeded site engagement service; it
  // should be recorded as not covered by the hints fetcher.
  ResetCountHintsRequestsReceived();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), search_results_page_url()));

  WaitUntilHintsFetcherRequestReceived();
  EXPECT_EQ(1u, count_hints_requests_received());
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 3, 1);
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 7, 1);
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
                "BatchUpdateGoogleSRP",
                1),
            1);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
      "BatchUpdateGoogleSRP",
      1);
}

class HintsFetcherSearchPagePrerenderingBrowserTest
    : public HintsFetcherSearchPageBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HintsFetcherSearchPageBrowserTest::SetUpCommandLine(command_line);
    // |prerender_helper_| has a ScopedFeatureList so we needed to delay its
    // creation until now because HintsFetcherDisabledBrowserTest also uses a
    // ScopedFeatureList and initialization order matters.
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(
            &HintsFetcherSearchPagePrerenderingBrowserTest::web_contents,
            base::Unretained(this)));
  }

 protected:
  content::test::PrerenderTestHelper* prerender_helper() {
    return prerender_helper_.get();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

// Tests that fetching the hints for a prerendered page is deferred until the
// page gets activated.
IN_PROC_BROWSER_TEST_F(HintsFetcherSearchPagePrerenderingBrowserTest,
                       HintsFetcherFetchedHintsLoadedAfterActivate) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Allowlist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);
  GURL initial_url =
      GetURLWithGoogleHost(origin_server_.get(), "/iframe_blank.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
      1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);
  histogram_tester->ExpectTotalCount(
      optimization_guide::kLoadedHintLocalHistogramString, 1);

  // Load a page in the prerender.
  GURL prerender_url = search_results_page_url();
  ResetCountHintsRequestsReceived();
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  EXPECT_EQ(0u, count_hints_requests_received());
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0, 0);
  histogram_tester->ExpectTotalCount(
      optimization_guide::kLoadedHintLocalHistogramString, 1);

  // Activate the page from the prerendering.
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  WaitUntilHintsFetcherRequestReceived();
  EXPECT_EQ(1u, count_hints_requests_received());
  optimization_guide::RetryForHistogramUntilCountReached(
      histogram_tester, optimization_guide::kLoadedHintLocalHistogramString, 2);

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 3, 1);
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 7, 1);
}

class HintsFetcherSearchPageDisabledBrowserTest
    : public HintsFetcherDisabledBrowserTest {
 public:
  HintsFetcherSearchPageDisabledBrowserTest() = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // Enabled.
        {{optimization_guide::features::kOptimizationHints, {}},
         {blink::features::kNavigationPredictor,
          {{"random_anchor_sampling_period", "1"},
           {"traffic_client_enabled_percent", "100"}}},
         {
             optimization_guide::features::kRemoteOptimizationGuideFetching,
             {{"max_concurrent_page_navigation_fetches", "2"},
              {"max_urls_for_optimization_guide_service_hints_fetch", "30"},
              {"batch_update_hints_for_top_hosts", "true"}},
         }},
        // Disabled.
        {optimization_guide::features::kOptimizationGuideFetchingForSRP});

    HintsFetcherDisabledBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Register an optimization type, so hints will be fetched at page
    // navigation.
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});

    HintsFetcherDisabledBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(HintsFetcherSearchPageDisabledBrowserTest,
                       HintsFetcherStillFetchesNavigations) {
  SetNetworkConnectionOnline();
  SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType::kSuccessful);

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Navigate to a search results page - should not result in a request with
  // more than 1 url.
  ResetCountHintsRequestsReceived();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), search_results_page_url()));

  // Technically the results page counts as a navigation URL.
  base::flat_set<std::string> srp_request;
  srp_request.insert(GURL(search_results_page_url()).host());
  srp_request.insert(GURL(search_results_page_url()).spec());
  SetExpectedHintsRequestForHostsAndUrls(srp_request);
  EXPECT_EQ(1u, count_hints_requests_received());
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 1);
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1, 1);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
      "PageNavigation",
      1);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
      "BatchUpdateGoogleSRP",
      0);

  // Now go to a regular URL.
  ResetCountHintsRequestsReceived();
  std::string full_url = "https://foo.com/test/";
  base::flat_set<std::string> expected_request;
  expected_request.insert(GURL(full_url).host());
  expected_request.insert(GURL(full_url).spec());
  SetExpectedHintsRequestForHostsAndUrls(expected_request);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(full_url)));

  optimization_guide::RetryForHistogramUntilCountReached(
      histogram_tester, optimization_guide::kLoadedHintLocalHistogramString, 1);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
      "PageNavigation",
      2);
  EXPECT_EQ(1u, count_hints_requests_received());
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 2);
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1, 2);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
      "PageNavigation",
      2);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
      "BatchUpdateGoogleSRP",
      0);
}

// Tests that OptimizationGuideWebContentsObserver limits the results for SRP.
//
// Note that `OptimizationGuideWebContentsObserver::FetchHintsUsingManager`
// limits the urls to `optimization_guide::features::MaxResultsForSRPFetch()`.
// In this test, this method is called with the following URLs but the order
// varies:
//
// - https://example.com/bar.html
// - https://example.com/baz.html
// - https://example.com/foo.html
// - https://example2.com/foo.html
// - https://example3.com/foo.html
// - https://foo.com/
// - https://foo.com/simple_page_with_anchors.html
//
// If we use `max_urls_for_srp_fetch > 1`, the result of
// `OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount` varies. So, we use
// `max_urls_for_srp_fetch = 1`.
class HintsFetcherSearchPageLimitedURLsBrowserTest
    : public HintsFetcherDisabledBrowserTest {
 public:
  HintsFetcherSearchPageLimitedURLsBrowserTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // Enabled.
        {{optimization_guide::features::kOptimizationHints, {}},
         {
             optimization_guide::features::kRemoteOptimizationGuideFetching,
             {{"max_concurrent_page_navigation_fetches", "2"},
              {"max_urls_for_optimization_guide_service_hints_fetch", "30"},
              {"batch_update_hints_for_top_hosts", "true"}},
         },
         {
             optimization_guide::features::kOptimizationGuideFetchingForSRP,
             {{"max_urls_for_srp_fetch", "1"}},
         }},
        // Disabled.
        {});

    HintsFetcherDisabledBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableFetchingHintsAtNavigationStartForTesting);
    HintsFetcherDisabledBrowserTest::SetUpCommandLine(cmd);
  }

  void SetUpOnMainThread() override {
    // Register an optimization type, so hints will be fetched at page
    // navigation.
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});

    HintsFetcherDisabledBrowserTest::SetUpOnMainThread();
  }
};

// TODO(crbug.com/40067071): Disable limited SRP test on Windows/CrOS for now.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_HintsFetcherLimitedResults DISABLED_HintsFetcherLimitedResults
#else
#define MAYBE_HintsFetcherLimitedResults HintsFetcherLimitedResults
#endif
IN_PROC_BROWSER_TEST_F(HintsFetcherSearchPageLimitedURLsBrowserTest,
                       MAYBE_HintsFetcherLimitedResults) {
  SetNetworkConnectionOnline();
  SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType::kSuccessful);

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Navigate to a search results page - should result a batch request with up
  // to 2 urls.
  ResetCountHintsRequestsReceived();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), search_results_page_url()));
  WaitUntilHintsFetcherRequestReceived();

  EXPECT_GE(optimization_guide::RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
                "BatchUpdateGoogleSRP",
                1),
            1);
  EXPECT_EQ(1u, count_hints_requests_received());
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 1);
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1, 1);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus."
      "BatchUpdateGoogleSRP",
      1);
}

class PersonalizedHintsFetcherBrowserTest : public HintsFetcherBrowserTest {
 public:
  PersonalizedHintsFetcherBrowserTest() = default;

  PersonalizedHintsFetcherBrowserTest(
      const PersonalizedHintsFetcherBrowserTest&) = delete;
  PersonalizedHintsFetcherBrowserTest& operator=(
      const HintsFetcherBrowserTest&) = delete;

  ~PersonalizedHintsFetcherBrowserTest() override = default;

  void SetUp() override { HintsFetcherBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    HintsFetcherBrowserTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&PersonalizedHintsFetcherBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void PopulateEnabledFeatures(
      std::vector<base::test::FeatureRefAndParams>* enabled_features) override {
    base::FieldTrialParams personalized_fetching_params = {
        {"allowed_contexts", "CONTEXT_BOOKMARKS"},
    };
    enabled_features->emplace_back(
        optimization_guide::features::kOptimizationGuidePersonalizedFetching,
        personalized_fetching_params);
  }

  void EnableSignin() {
    identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("user@gmail.com",
                                      signin::ConsentLevel::kSignin);
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void CanApplyOptimizationOnDemand(
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::proto::RequestContext request_context) {
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->CanApplyOptimizationOnDemand(
            {GURL("https://example.com")}, {optimization_type}, request_context,
            base::BindRepeating(
                [](optimization_guide::proto::OptimizationType
                       optimization_type,
                   base::RunLoop* run_loop, const GURL& url,
                   const base::flat_map<
                       optimization_guide::proto::OptimizationType,
                       optimization_guide::
                           OptimizationGuideDecisionWithMetadata>& decisions) {
                  // Expect one decision per requested type.
                  EXPECT_EQ(decisions.size(), 1u);
                  auto it = decisions.find(optimization_type);
                  EXPECT_NE(it, decisions.end());
                  EXPECT_EQ(
                      it->second.decision,
                      optimization_guide::OptimizationGuideDecision::kFalse);

                  run_loop->Quit();
                },
                optimization_type, run_loop.get()));
    run_loop->Run();
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(PersonalizedHintsFetcherBrowserTest, NoUserSignIn) {
  base::HistogramTester histogram_tester;
  SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType::kSuccessful);

  // No access token set when user is not signed in.
  SetExpectedBearerAccessToken(std::string());
  CanApplyOptimizationOnDemand(
      optimization_guide::proto::OptimizationType::NOSCRIPT,
      optimization_guide::proto::CONTEXT_BOOKMARKS);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus.Bookmarks",
      optimization_guide::FetcherRequestStatus::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(PersonalizedHintsFetcherBrowserTest, UserSignedIn) {
  base::HistogramTester histogram_tester;
  EnableSignin();
  SetResponseType(
      optimization_guide::HintsFetcherRemoteResponseType::kSuccessful);

  SetExpectedBearerAccessToken("Bearer access_token");
  CanApplyOptimizationOnDemand(
      optimization_guide::proto::OptimizationType::NOSCRIPT,
      optimization_guide::proto::CONTEXT_BOOKMARKS);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus.Bookmarks",
      optimization_guide::FetcherRequestStatus::kSuccess, 1);

  // Only the enabled RequestContext will have access token enabled.
  SetExpectedBearerAccessToken(std::string());
  CanApplyOptimizationOnDemand(
      optimization_guide::proto::OptimizationType::NOSCRIPT,
      optimization_guide::proto::CONTEXT_JOURNEYS);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.RequestStatus.Journeys",
      optimization_guide::FetcherRequestStatus::kSuccess, 1);
}
