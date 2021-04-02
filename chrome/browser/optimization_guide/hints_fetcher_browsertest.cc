// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/google/core/common/google_util.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/hints_component_info.h"
#include "components/optimization_guide/core/hints_component_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/core/test_hints_component_creator.h"
#include "components/optimization_guide/core/top_host_provider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/hashing.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/features.h"

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run (such as chromeos) there
// might be two profiles created, and this will return the total sample count
// across profiles.
int GetTotalHistogramSamples(const base::HistogramTester* histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  int total = 0;
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;

    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

enum class HintsFetcherRemoteResponseType {
  kSuccessful = 0,
  kUnsuccessful = 1,
  kMalformed = 2,
  kHung = 3,
};

constexpr char kGoogleHost[] = "www.google.com";

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

}  // namespace

// This test class sets up everything but does not enable any
// HintsFetcher-related features. The parameter selects whether the
// OptimizationGuideKeyedService is enabled (tests should pass in the same way
// for both cases).
class HintsFetcherDisabledBrowserTest : public InProcessBrowserTest {
 public:
  HintsFetcherDisabledBrowserTest() = default;
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
    origin_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");

    ASSERT_TRUE(origin_server_->Start());

    https_url_ = origin_server_->GetURL("/hint_setup.html");
    ASSERT_TRUE(https_url().SchemeIs(url::kHttpsScheme));

    search_results_page_url_ =
        GetURLWithGoogleHost(origin_server_.get(), "/search_results_page.html");
    ASSERT_TRUE(search_results_page_url_.is_valid() &&
                search_results_page_url_.SchemeIs(url::kHttpsScheme) &&
                google_util::IsGoogleHostname(search_results_page_url_.host(),
                                              google_util::DISALLOW_SUBDOMAIN));

    hints_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    hints_server_->ServeFilesFromSourceDirectory("chrome/test/data/previews");
    hints_server_->RegisterRequestHandler(base::BindRepeating(
        &HintsFetcherDisabledBrowserTest::HandleGetHintsRequest,
        base::Unretained(this)));

    ASSERT_TRUE(hints_server_->Start());

    param_feature_list_.InitWithFeatures(
        {blink::features::kNavigationPredictor}, {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("ignore-certificate-errors");

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
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitchASCII("force-variation-ids", "4");

    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "example1.com, example2.com");

    cmd->AppendSwitch(optimization_guide::switches::kFetchHintsOverrideTimer);
  }

  // Creates hint data for the |hint_setup_url|'s so that the fetching of the
  // hints is triggered.
  void SetUpComponentUpdateHints(const GURL& hint_setup_url) {
    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {hint_setup_url.host()}, "*");

    base::HistogramTester histogram_tester;

    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(component_info);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);
  }

  void SetNetworkConnectionOffline() {
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
  }

  void SetResponseType(HintsFetcherRemoteResponseType response_type) {
    response_type_ = response_type;
  }

  // Seeds the Site Engagement Service with two HTTP and two HTTPS sites for the
  // current profile.
  void SeedSiteEngagementService() {
    auto* service = site_engagement::SiteEngagementService::Get(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()));
    GURL https_url1("https://images.google.com/");
    service->AddPointsForTesting(https_url1, 15);

    GURL https_url2("https://news.google.com/");
    service->AddPointsForTesting(https_url2, 3);

    GURL http_url1("http://photos.google.com/");
    service->AddPointsForTesting(http_url1, 21);

    GURL http_url2("http://maps.google.com/");
    service->AddPointsForTesting(http_url2, 2);
  }

  void SetTopHostBlocklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState
          blocklist_state) {
    Profile::FromBrowserContext(browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetBrowserContext())
        ->GetPrefs()
        ->SetInteger(
            optimization_guide::prefs::kHintsFetcherTopHostBlocklistState,
            static_cast<int>(blocklist_state));
  }

  void LoadHintsForUrl(const GURL& url) {
    base::HistogramTester histogram_tester;

    // Navigate to |url| to prime the OptimizationGuide hints for the
    // url's host and ensure that they have been loaded from the store (via
    // histogram) prior to the navigation that tests functionality.
    ui_test_utils::NavigateToURL(browser(), url);

    RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
  }

  // Returns the count of top hosts that are blocklisted by reading the relevant
  // pref.
  size_t GetTopHostBlocklistSize() const {
    PrefService* pref_service = browser()->profile()->GetPrefs();
    const base::DictionaryValue* top_host_blocklist =
        pref_service->GetDictionary(
            optimization_guide::prefs::kHintsFetcherTopHostBlocklist);
    return top_host_blocklist->size();
  }

  // Adds |host_count| HTTPS origins to site engagement service.
  void AddHostsToSiteEngagementService(size_t host_count) {
    auto* service = site_engagement::SiteEngagementService::Get(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()));
    for (size_t i = 0; i < host_count; ++i) {
      GURL https_url("https://myfavoritesite" + base::NumberToString(i) +
                     ".com/");
      service->AddPointsForTesting(https_url, 15);
    }
  }

  // Returns the number of hosts known to the site engagement service. The value
  // is obtained by querying the site engagement service.
  size_t GetCountHostsKnownToSiteEngagementService() const {
    auto* service = site_engagement::SiteEngagementService::Get(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()));
    return service->GetAllDetails().size();
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

  void WaitUntilHintsFetcherRequestReceived() {
    while (true) {
      {
        // Acquire the |lock_| inside to avoid starving other consumers of the
        // lock.
        base::AutoLock lock(lock_);
        if (count_hints_requests_received_ > 0)
          return;
      }
      base::RunLoop().RunUntilIdle();
    }
  }

  void ResetCountHintsRequestsReceived() {
    base::AutoLock lock(lock_);
    count_hints_requests_received_ = 0;
  }

  // Wait for page layout to happen. This is needed in some tests since the
  // anchor elements are extracted from the webpage after page layout finishes.
  void WaitForPageLayout() {
    const char* entry_name =
        ukm::builders::NavigationPredictorPageLinkMetrics::kEntryName;

    if (ukm_recorder_->GetEntriesByName(entry_name).empty()) {
      base::RunLoop run_loop;
      ukm_recorder_->SetOnAddEntryCallback(entry_name, run_loop.QuitClosure());
      run_loop.Run();
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  std::unique_ptr<net::EmbeddedTestServer> hints_server_;
  HintsFetcherRemoteResponseType response_type_ =
      HintsFetcherRemoteResponseType::kSuccessful;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleOriginRequest(
      const net::test_server::HttpRequest& request) {
    EXPECT_EQ(request.method, net::test_server::METHOD_GET);
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);

    return std::move(response);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleGetHintsRequest(
      const net::test_server::HttpRequest& request) {
    base::AutoLock lock(lock_);

    ++count_hints_requests_received_;
    std::unique_ptr<net::test_server::BasicHttpResponse> response;

    response = std::make_unique<net::test_server::BasicHttpResponse>();
    // If the request is a GET, it corresponds to a navigation so return a
    // normal response.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_NE(request.headers.end(), request.headers.find("X-Client-Data"));

    optimization_guide::proto::GetHintsRequest hints_request;
    EXPECT_TRUE(hints_request.ParseFromString(request.content));
    EXPECT_FALSE(hints_request.hosts().empty() && hints_request.urls().empty());
    EXPECT_GE(optimization_guide::features::
                  MaxHostsForOptimizationGuideServiceHintsFetch(),
              static_cast<size_t>(hints_request.hosts().size()));

    // Only verify the hints if there are hosts in the request.
    if (!hints_request.hosts().empty())
      VerifyHintsMatchExpectedHostsAndUrls(hints_request);

    if (response_type_ == HintsFetcherRemoteResponseType::kSuccessful) {
      response->set_code(net::HTTP_OK);

      optimization_guide::proto::GetHintsResponse get_hints_response;

      optimization_guide::proto::Hint* hint = get_hints_response.add_hints();
      hint->set_key_representation(optimization_guide::proto::HOST);
      hint->set_key(https_url_.host());
      optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
      page_hint->set_page_pattern("page pattern");

      std::string serialized_request;
      get_hints_response.SerializeToString(&serialized_request);
      response->set_content(serialized_request);
    } else if (response_type_ ==
               HintsFetcherRemoteResponseType::kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);

    } else if (response_type_ == HintsFetcherRemoteResponseType::kMalformed) {
      response->set_code(net::HTTP_OK);

      std::string serialized_request = "Not a proto";
      response->set_content(serialized_request);
    } else if (response_type_ == HintsFetcherRemoteResponseType::kHung) {
      return std::make_unique<net::test_server::HungResponse>();
    } else {
      NOTREACHED();
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
    for (const auto& host : hints_request.hosts())
      hosts_and_urls_requested.insert(host.host());
    for (const auto& url : hints_request.urls()) {
      // TODO(crbug/1051365):  Remove normalization step once nav predictor
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

    // We only expect 1 field trial to be allowed and sent up.
    EXPECT_EQ(1, hints_request.active_field_trials_size());
    EXPECT_EQ(variations::HashName(
                  "scoped_feature_list_trial_for_OptimizationHintsFetching"),
              hints_request.active_field_trials(0).name_hash());
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

  // Guarded by |lock_|. Set of hosts and URLs for which a hints request is
  // expected to arrive. This set is verified to match with the set of hosts and
  // URLs present in the hints request. If null, then the verification is not
  // done.
  base::Optional<base::flat_set<std::string>>
      expect_hints_request_for_hosts_and_urls_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(HintsFetcherDisabledBrowserTest);
};

// This test class enables OnePlatform Hints.
class HintsFetcherBrowserTest : public HintsFetcherDisabledBrowserTest {
 public:
  HintsFetcherBrowserTest() = default;

  ~HintsFetcherBrowserTest() override = default;

  void SetUp() override {
    // Enable OptimizationHintsFetching with |kRemoteOptimizationGuideFetching|.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {optimization_guide::features::kOptimizationHints, {}},
            {optimization_guide::features::kRemoteOptimizationGuideFetching,
             {{"max_concurrent_page_navigation_fetches", "2"}}},
            {optimization_guide::features::kOptimizationHintsFieldTrials,
             {{"allowed_field_trial_names",
               "scoped_feature_list_trial_for_OptimizationHintsFetching"}}},
        },
        {});
    // Call to inherited class to match same set up with feature flags added.
    HintsFetcherDisabledBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Register an optimization type, so hints will be fetched at page
    // navigation.
    OptimizationGuideKeyedServiceFactory::GetForProfile(
        Profile::FromBrowserContext(browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetBrowserContext()))
        ->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});

    HintsFetcherDisabledBrowserTest::SetUpOnMainThread();
  }

  optimization_guide::TopHostProvider* top_host_provider() {
    OptimizationGuideKeyedService* keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    return keyed_service->GetTopHostProvider();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HintsFetcherBrowserTest);
};

// This test creates new browser with no profile and loads a random page with
// the feature flags for OptimizationHintsFetching. We confirm that the
// TopHostProvider is called and does not crash by checking UMA
// histograms for the total number of TopEngagementSites and
// the total number of sites returned controlled by the experiments flag
// |max_oneplatform_update_hosts|.
IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest, HintsFetcherEnabled) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
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

IN_PROC_BROWSER_TEST_F(HintsFetcherDisabledBrowserTest, HintsFetcherDisabled) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Expect that the histogram for HintsFetcher to be 0 because the OnePlatform
  // is not enabled.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}

// This test creates a new browser and seeds the Site Engagement Service with
// both HTTP and HTTPS sites. The test confirms that top host provider
// is called to provide a list of hosts to HintsFetcher only returns hosts with
// a HTTPS scheme. We verify this with the UMA histogram logged when the
// GetHintsRequest is made to the remote Optimization Guide Service.
IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest, TopHostProviderHTTPSOnly) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Adds two HTTP and two HTTPS sites into the Site Engagement Service.
  SeedSiteEngagementService();

  // This forces the hint cache to be initialized and hints to be fetched.
  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample as
  // Hints Fetching is enabled. This also ensures that the histograms have been
  // updated to verify the correct number of hosts that hints will be requested
  // for.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Only the 2 HTTPS hosts should be requested hints for.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);

  EXPECT_EQ(0u, GetTopHostBlocklistSize());
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcherFetchedHintsLoaded) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1),
            1);

  LoadHintsForUrl(https_url());

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verifies that the fetched hint is just used in memory and nothing is
  // loaded.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintCache.HintType.Loaded", 0);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcherWithResponsesSuccessful) {
  SetResponseType(HintsFetcherRemoteResponseType::kSuccessful);

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  EXPECT_GE(RetryForHistogramUntilCountReached(
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
  SetResponseType(HintsFetcherRemoteResponseType::kUnsuccessful);

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  EXPECT_GE(RetryForHistogramUntilCountReached(
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
  SetResponseType(HintsFetcherRemoteResponseType::kMalformed);

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  EXPECT_GE(RetryForHistogramUntilCountReached(
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

  SetResponseType(HintsFetcherRemoteResponseType::kUnsuccessful);

  // Set the ECT to force a fetch at navigation time.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);

  ui_test_utils::NavigateToURL(browser(), GURL("https://unsuccessful.com/"));

  // We expect that we requested hints for 1 URL.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1),
            1);
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    HintsFetcherWithResponsesHungShouldRecordWhenActiveRequestCanceled) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  SetResponseType(HintsFetcherRemoteResponseType::kHung);

  // Set the ECT to force a fetch at navigation time.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);

  ui_test_utils::NavigateToURL(browser(), GURL("https://hung.com/1"));
  ui_test_utils::NavigateToURL(browser(), GURL("https://hung.com/2"));
  ui_test_utils::NavigateToURL(browser(), GURL("https://hung.com/3"));

  // We expect that one request was canceled.
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.ActiveRequestCanceled."
      "PageNavigation",
      1, 1);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest, HintsFetcherClearFetchedHints) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as OnePlatform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1),
            1);

  LoadHintsForUrl(https_url());

  ui_test_utils::NavigateToURL(browser(), https_url());

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

  ui_test_utils::NavigateToURL(browser(), https_url());

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(optimization_guide::OptimizationGuideStore::
                           StoreEntryType::kComponentHint),
      1);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest, HintsFetcherOverrideTimer) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      optimization_guide::switches::kFetchHintsOverride);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kFetchHintsOverrideTimer);

  SeedSiteEngagementService();

  // Set the blocklist state to initialized so the sites in the engagement
  // service will be used and not blocklisted on the first GetTopHosts request.
  SetTopHostBlocklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlocklistState::kInitialized);

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as OnePlatform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  // There should be 2 sites in the engagement service.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
            1);
  // There should have been 1 hint returned in the response.
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);

  LoadHintsForUrl(https_url());

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verifies that the fetched hint is used from memory and no hints are loaded.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintCache.HintType.Loaded", 0);
}

// TODO(crbug.com/1177122) Re-enable test
IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       DISABLED_HintsFetcherNetworkOffline) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      optimization_guide::switches::kFetchHintsOverride);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kFetchHintsOverrideTimer);

  // Set the network to be offline.
  SetNetworkConnectionOffline();

  // Set the blocklist state to initialized so the sites in the engagement
  // service will be used and not blocklisted on the first GetTopHosts
  // request.
  SeedSiteEngagementService();

  // Set the blocklist state to initialized so the sites in the engagement
  // service will be used and not blocklisted on the first GetTopHosts request.
  SetTopHostBlocklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlocklistState::kInitialized);

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // No HintsFetch should occur because the connection is offline.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest, HintsFetcherFetches) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as hints fetching is enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
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

// Test that the hints are fetched at the time of the navigation.
IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcher_NavigationFetch_ECT) {
  {
    base::HistogramTester histogram_tester;

    // Whitelist NoScript for https_url()'s' host.
    SetUpComponentUpdateHints(https_url());

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);

    // Expect that the browser initialization will record at least one sample
    // in each of the following histograms as One Platform Hints are enabled.
    EXPECT_GE(
        RetryForHistogramUntilCountReached(
            &histogram_tester,
            "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
        1);

    EXPECT_GE(RetryForHistogramUntilCountReached(
                  &histogram_tester,
                  "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", 1),
              1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.Status", net::HTTP_OK,
        1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.NetErrorCode", net::OK,
        1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HintCount", 1, 1);
    EXPECT_EQ(1u, count_hints_requests_received());
  }

  // Change ECT to a low value. Hints should be fetched at the time of
  // navigation.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    ResetCountHintsRequestsReceived();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);

    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as covered by the hints fetcher due to the race.
    base::flat_set<std::string> expected_request_2g;
    std::string host_2g("https://unseenhost_2g.com/");
    expected_request_2g.insert(GURL(host_2g).host());
    expected_request_2g.insert(GURL(host_2g).spec());
    SetExpectedHintsRequestForHostsAndUrls(expected_request_2g);
    ui_test_utils::NavigateToURL(browser(), GURL(host_2g));

    EXPECT_EQ(1u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
    // Navigate away so metrics are recorded.
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
    ui_test_utils::NavigateToURL(browser(), GURL("http://nohints.com/"));
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::OptimizationGuide::kEntryName);
    EXPECT_EQ(1u, entries.size());
    auto* entry = entries[0];
    EXPECT_TRUE(ukm_recorder.EntryHasMetric(
        entry, ukm::builders::OptimizationGuide::
                   kNavigationHintsFetchRequestLatencyName));
    EXPECT_TRUE(ukm_recorder.EntryHasMetric(
        entry, ukm::builders::OptimizationGuide::
                   kNavigationHintsFetchAttemptStatusName));
    ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::OptimizationGuide::
            kNavigationHintsFetchAttemptStatusName,
        static_cast<int>(optimization_guide::RaceNavigationFetchAttemptStatus::
                             kRaceNavigationFetchHostAndURL));
  }

  // Change ECT to unknown. Hints should not be fetched at the time of
  // navigation as the ECT is unknown so the fetcher should not race.
  {
    base::HistogramTester histogram_tester;
    ResetCountHintsRequestsReceived();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

    base::flat_set<std::string> expected_request_unknown;
    std::string host_unknown_ect("https://unseenhost_unknown_ect.com/");
    expected_request_unknown.insert((GURL(host_unknown_ect).host()));
    expected_request_unknown.insert((GURL(host_unknown_ect).spec()));
    SetExpectedHintsRequestForHostsAndUrls(expected_request_unknown);
    ui_test_utils::NavigateToURL(browser(), GURL(host_unknown_ect));

    EXPECT_EQ(0u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);

    // Navigate away so metrics are recorded.
    base::HistogramTester prev_nav_histogram_tester;
    ukm::TestAutoSetUkmRecorder prev_nav_ukm_recorder;
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
    ui_test_utils::NavigateToURL(browser(), GURL("http://nohints.com/"));
    auto entries = prev_nav_ukm_recorder.GetEntriesByName(
        ukm::builders::OptimizationGuide::kEntryName);
    EXPECT_EQ(1u, entries.size());
    auto* entry = entries[0];
    EXPECT_FALSE(prev_nav_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::OptimizationGuide::
                   kNavigationHintsFetchRequestLatencyName));
    EXPECT_FALSE(prev_nav_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::OptimizationGuide::
                   kNavigationHintsFetchAttemptStatusName));
  }

  // Change ECT back to a low value. Hints should be fetched at the time of
  // navigation.
  {
    base::HistogramTester histogram_tester;
    ResetCountHintsRequestsReceived();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_3G);

    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as not covered by the hints fetcher.
    base::flat_set<std::string> expected_request_3g;
    std::string host_3g("https://unseenhost_3g.com/");
    expected_request_3g.insert(GURL(host_3g).host());
    expected_request_3g.insert(GURL(host_3g).spec());
    SetExpectedHintsRequestForHostsAndUrls(expected_request_3g);
    ui_test_utils::NavigateToURL(browser(), GURL(host_3g));

    EXPECT_EQ(1u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);

    // Navigate away so metrics are recorded.
    base::HistogramTester prev_nav_histogram_tester;
    ukm::TestAutoSetUkmRecorder prev_nav_ukm_recorder;
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
    ui_test_utils::NavigateToURL(browser(), GURL("http://nohints.com/"));
    auto entries = prev_nav_ukm_recorder.GetEntriesByName(
        ukm::builders::OptimizationGuide::kEntryName);
    EXPECT_EQ(1u, entries.size());
    auto* entry = entries[0];
    EXPECT_TRUE(prev_nav_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::OptimizationGuide::
                   kNavigationHintsFetchRequestLatencyName));
    EXPECT_TRUE(prev_nav_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::OptimizationGuide::
                   kNavigationHintsFetchAttemptStatusName));
    prev_nav_ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::OptimizationGuide::
            kNavigationHintsFetchAttemptStatusName,
        static_cast<int>(optimization_guide::RaceNavigationFetchAttemptStatus::
                             kRaceNavigationFetchHostAndURL));
  }

  // Navigate again to a webpage with the
  // same host. Hints should be available at the time of
  // navigation.
  {
    base::HistogramTester histogram_tester;
    ResetCountHintsRequestsReceived();

    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_3G);

    // Navigate to a host that was recently fetched. It
    // should be recorded as covered by the hints fetcher.
    base::flat_set<std::string> expected_request_3g;
    std::string host_3g("https://unseenhost_3g.com");
    expected_request_3g.insert(GURL(host_3g).host());
    expected_request_3g.insert(GURL(host_3g).spec());
    SetExpectedHintsRequestForHostsAndUrls(expected_request_3g);
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("https://unseenhost_3g.com/test1.html"));

    // With URL-keyed Hints, every unique URL navigated to will result in a
    // hints fetch if racing is enabled and allowed.
    EXPECT_EQ(1u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
        &histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        1);
    // Navigate away so metrics are recorded.
    base::HistogramTester prev_nav_histogram_tester;
    ukm::TestAutoSetUkmRecorder prev_nav_ukm_recorder;
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
    ui_test_utils::NavigateToURL(browser(), GURL("http://nohints.com/"));
    auto entries = prev_nav_ukm_recorder.GetEntriesByName(
        ukm::builders::OptimizationGuide::kEntryName);
    EXPECT_EQ(1u, entries.size());
    auto* entry = entries[0];
    EXPECT_TRUE(prev_nav_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::OptimizationGuide::
                   kNavigationHintsFetchRequestLatencyName));
    EXPECT_TRUE(prev_nav_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::OptimizationGuide::
                   kNavigationHintsFetchAttemptStatusName));
    prev_nav_ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::OptimizationGuide::
            kNavigationHintsFetchAttemptStatusName,
        static_cast<int>(optimization_guide::RaceNavigationFetchAttemptStatus::
                             kRaceNavigationFetchHostAndURL));
  }
}

// Test that the hints are fetched at the time of the navigation.
IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       HintsFetcher_NavigationFetch_URLKeyedNotRefetched) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  RetryForHistogramUntilCountReached(
      histogram_tester,
      optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
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

  // Setting the connection type to be slow so the page navigation race fetch
  // is initiated.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);
  std::string full_url("https://foo.com/test/");
  {
    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as a race for both the host and the URL.
    base::flat_set<std::string> expected_request;
    expected_request.insert(GURL(full_url).host());
    expected_request.insert(GURL(full_url).spec());
    SetExpectedHintsRequestForHostsAndUrls(expected_request);
    ui_test_utils::NavigateToURL(browser(), GURL(full_url));

    EXPECT_EQ(2u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
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
    ui_test_utils::NavigateToURL(browser(), GURL(full_url));

    // With URL-keyed Hints, every unique URL navigated to will result in a
    // hints fetch if racing is enabled and allowed.
    EXPECT_EQ(2u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        2);

    // Only the host will be attempted to race, the fetcher should block the
    // host from being fetched.
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHost,
        1);
  }

  // Incognito page loads should not initiate any fetches.
  {
    base::HistogramTester incognito_histogram_tester;
    // Instantiate off the record Optimization Guide Service.
    OptimizationGuideKeyedServiceFactory::GetForProfile(
        browser()->profile()->GetPrimaryOTRProfile())
        ->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});

    Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
    ui_test_utils::NavigateToURL(otr_browser, GURL(full_url));

    // Make sure no additional hints requests were received.
    RetryForHistogramUntilCountReached(
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

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  RetryForHistogramUntilCountReached(
      histogram_tester,
      optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
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

  // Setting the connection type to be slow so the page navigation race fetch
  // is initiated.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);
  std::string full_url("https://foo.com/test/");
  {
    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as a race for both the host and the URL.
    base::flat_set<std::string> expected_request;
    expected_request.insert(GURL(full_url).host());
    expected_request.insert(GURL(full_url).spec());
    SetExpectedHintsRequestForHostsAndUrls(expected_request);
    ui_test_utils::NavigateToURL(browser(), GURL(full_url));

    EXPECT_EQ(2u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
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
    ui_test_utils::NavigateToURL(browser(), GURL(full_url));

    // With URL-keyed Hints, every unique URL navigated to will result in a
    // hints fetch if racing is enabled and allowed.
    EXPECT_EQ(3u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
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

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  RetryForHistogramUntilCountReached(
      histogram_tester,
      optimization_guide::kComponentHintsUpdatedResultHistogramString, 1);

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
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

  // Setting the connection type to be slow so the page navigation race fetch
  // is initiated.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);
  std::string full_url("https://foo.com/test/");
  {
    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as a race for both the host and the URL.
    base::flat_set<std::string> expected_request;
    expected_request.insert(GURL(full_url).host());
    expected_request.insert(GURL(full_url).spec());
    SetExpectedHintsRequestForHostsAndUrls(expected_request);
    ui_test_utils::NavigateToURL(browser(), GURL(full_url));

    EXPECT_EQ(2u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
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
    ui_test_utils::NavigateToURL(browser(), GURL(full_url));

    // With URL-keyed Hints, every unique URL navigated to will result in a
    // hints fetch if racing is enabled and allowed.
    EXPECT_EQ(2u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        2);

    // Only the host will be attempted to race, the fetcher should block the
    // host from being fetched.
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        optimization_guide::RaceNavigationFetchAttemptStatus::
            kRaceNavigationFetchHost,
        1);
  }
}

class HintsFetcherChangeDefaultBlocklistSizeBrowserTest
    : public HintsFetcherBrowserTest {
 public:
  HintsFetcherChangeDefaultBlocklistSizeBrowserTest() = default;

  ~HintsFetcherChangeDefaultBlocklistSizeBrowserTest() override = default;

  void SetUp() override {
    base::FieldTrialParams optimization_hints_fetching_params;
    optimization_hints_fetching_params["top_host_blacklist_size_multiplier"] =
        "5";

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            /* vector of enabled features along with params */
            {optimization_guide::features::kRemoteOptimizationGuideFetching,
             {optimization_hints_fetching_params}},
            {optimization_guide::features::kOptimizationHints, {}},
        },
        {/* disabled_features */});

    // Call to inherited class to match same set up with feature flags added.
    HintsFetcherDisabledBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    cmd->AppendSwitch("purge_hint_cache_store");

    // Set up OptimizationGuideServiceURL, this does not enable HintsFetching,
    // only provides the URL.
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceGetHintsURL,
        hints_server_->base_url().spec());

    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableCheckingUserPermissionsForTesting);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HintsFetcherChangeDefaultBlocklistSizeBrowserTest);
};

// Changes the default size of the top host blocklist using finch. Also, sets
// the count of hosts previously engaged to a large number and verifies that the
// top host blocklist is correctly populated.
IN_PROC_BROWSER_TEST_F(HintsFetcherChangeDefaultBlocklistSizeBrowserTest,
                       ChangeDefaultBlocklistSize) {
  AddHostsToSiteEngagementService(120u);
  const size_t engaged_hosts = GetCountHostsKnownToSiteEngagementService();
  EXPECT_EQ(120u, engaged_hosts);

  // Ensure everything within the site engagement service fits within the top
  // host blocklist size.
  ASSERT_LE(
      engaged_hosts,
      optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize());

  SetTopHostBlocklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
          kNotInitialized);

  ASSERT_TRUE(top_host_provider());

  std::vector<std::string> top_hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(0u, top_hosts.size());

  top_hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(0u, top_hosts.size());

  // Everything HTTPS origin within the site engagement service should now be
  // in the blocklist.
  EXPECT_EQ(engaged_hosts, GetTopHostBlocklistSize());
}

class HintsFetcherSearchPageBrowserTest : public HintsFetcherBrowserTest {
  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableFetchingHintsAtNavigationStartForTesting);
    cmd->AppendSwitch("ignore-certificate-errors");
    HintsFetcherBrowserTest::SetUpCommandLine(cmd);
  }
};

IN_PROC_BROWSER_TEST_F(HintsFetcherSearchPageBrowserTest,
                       HintsFetcher_SRP_Slow_Connection) {
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);

  const base::HistogramTester* histogram_tester = GetHistogramTester();

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // Expect that the browser initialization will record at least one sample
  // in each of the following histograms as One Platform Hints are enabled.
  EXPECT_GE(RetryForHistogramUntilCountReached(
                histogram_tester,
                "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1),
            1);

  EXPECT_GE(RetryForHistogramUntilCountReached(
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
  ui_test_utils::NavigateToURL(browser(), search_results_page_url());
  WaitForPageLayout();

  RetryForHistogramUntilCountReached(
      histogram_tester,
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge", 1);

  WaitUntilHintsFetcherRequestReceived();
  EXPECT_EQ(1u, count_hints_requests_received());
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 3, 1);
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 7, 1);
}
