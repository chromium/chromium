// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_store.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/test_hints_component_creator.h"
#include "components/optimization_guide/top_host_provider.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_task_traits.h"
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
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

int GetCountBucketSamples(const base::HistogramTester* histogram_tester,
                          const std::string& histogram_name,
                          size_t bucket_min) {
  std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);

  for (const auto& bucket : buckets) {
    if (bucket_min == static_cast<size_t>(bucket.min))
      return bucket.count;
  }

  return 0;
}

enum class HintsFetcherRemoteResponseType {
  kSuccessful = 0,
  kUnsuccessful = 1,
  kMalformed = 2,
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
    origin_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
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

    hints_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
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

    cmd->AppendSwitch("enable-spdy-proxy-auth");

    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitch("purge_hint_cache_store");

    // Set up OptimizationGuideServiceURL, this does not enable HintsFetching,
    // only provides the URL.
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceGetHintsURL,
        hints_server_->base_url().spec());
    cmd->AppendSwitchASCII(optimization_guide::switches::kFetchHintsOverride,
                           "example1.com, example2.com");
    cmd->AppendSwitch(previews::switches::kDoNotRequireLitePageRedirectInfoBar);

    cmd->AppendSwitch(optimization_guide::switches::kFetchHintsOverrideTimer);
  }

  // Creates hint data for the |hint_setup_url|'s so that OnHintsUpdated in
  // Previews Optimization Guide is called and HintsFetch can be tested.
  void SetUpComponentUpdateHints(const GURL& hint_setup_url) {
    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {hint_setup_url.host()}, "*",
            {});

    base::HistogramTester histogram_tester;

    g_browser_process->optimization_guide_service()->MaybeUpdateHintsComponent(
        component_info);

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
    SiteEngagementService* service = SiteEngagementService::Get(
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

  void SetTopHostBlacklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState
          blacklist_state) {
    Profile::FromBrowserContext(browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetBrowserContext())
        ->GetPrefs()
        ->SetInteger(optimization_guide::prefs::
                         kHintsFetcherDataSaverTopHostBlacklistState,
                     static_cast<int>(blacklist_state));
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

  // Returns the count of top hosts that are blacklisted by reading the relevant
  // pref.
  size_t GetTopHostBlacklistSize() const {
    PrefService* pref_service = browser()->profile()->GetPrefs();
    const base::DictionaryValue* top_host_blacklist =
        pref_service->GetDictionary(
            optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklist);
    return top_host_blacklist->size();
  }

  // Adds |host_count| HTTPS origins to site engagement service.
  void AddHostsToSiteEngagementService(size_t host_count) {
    SiteEngagementService* service = SiteEngagementService::Get(
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
    SiteEngagementService* service = SiteEngagementService::Get(
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

  void SetExpectedHintsRequestForHosts(
      const base::flat_set<std::string>& hosts) {
    base::AutoLock lock(lock_);
    expect_hints_request_for_hosts_ = hosts;
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
    response.reset(new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);

    return std::move(response);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleGetHintsRequest(
      const net::test_server::HttpRequest& request) {
    base::AutoLock lock(lock_);

    ++count_hints_requests_received_;
    std::unique_ptr<net::test_server::BasicHttpResponse> response;

    response.reset(new net::test_server::BasicHttpResponse);
    // If the request is a GET, it corresponds to a navigation so return a
    // normal response.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);

    optimization_guide::proto::GetHintsRequest hints_request;
    EXPECT_TRUE(hints_request.ParseFromString(request.content));
    EXPECT_FALSE(hints_request.hosts().empty());
    EXPECT_GE(optimization_guide::features::
                  MaxHostsForOptimizationGuideServiceHintsFetch(),
              static_cast<size_t>(hints_request.hosts().size()));

    VerifyHintsMatchExpectedHosts(hints_request);

    if (response_type_ == HintsFetcherRemoteResponseType::kSuccessful) {
      response->set_code(net::HTTP_OK);

      optimization_guide::proto::GetHintsResponse get_hints_response;

      optimization_guide::proto::Hint* hint = get_hints_response.add_hints();
      hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
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

    } else {
      NOTREACHED();
    }

    return std::move(response);
  }

  // Verifies that the hosts present in |hints_request| match the expected set
  // of hosts present in |expect_hints_request_for_hosts_|. The ordering of the
  // hosts in not matched.
  void VerifyHintsMatchExpectedHosts(
      const optimization_guide::proto::GetHintsRequest& hints_request) const {
    if (!expect_hints_request_for_hosts_)
      return;

    base::flat_set<std::string> hosts_requested;
    for (const auto& host : hints_request.hosts())
      hosts_requested.insert(host.host());

    EXPECT_EQ(expect_hints_request_for_hosts_.value().size(),
              hosts_requested.size());
    for (const auto& host : expect_hints_request_for_hosts_.value()) {
      hosts_requested.erase(host);
    }
    EXPECT_EQ(0u, hosts_requested.size());
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

  // Guarded by |lock_|.
  // Set of hosts for which a hints request is expected to arrive. This set is
  // verified to match with the set of hosts present in the hints request. If
  // null, then the verification is not done.
  base::Optional<base::flat_set<std::string>> expect_hints_request_for_hosts_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(HintsFetcherDisabledBrowserTest);
};

// This test class enables OnePlatform Hints.
class HintsFetcherBrowserTest : public HintsFetcherDisabledBrowserTest {
 public:
  HintsFetcherBrowserTest() = default;

  ~HintsFetcherBrowserTest() override = default;

  void SetUp() override {
    // Enable OptimizationHintsFetching with |kOptimizationHintsFetching|.
    scoped_feature_list_.InitWithFeatures(
        {previews::features::kPreviews, previews::features::kNoScriptPreviews,
         optimization_guide::features::kOptimizationHints,
         previews::features::kResourceLoadingHints,
         optimization_guide::features::kOptimizationHintsFetching,
         data_reduction_proxy::features::
             kDataReductionProxyEnabledWithNetworkService},
        {});
    // Call to inherited class to match same set up with feature flags added.
    HintsFetcherDisabledBrowserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HintsFetcherBrowserTest);
};

// Issues with multiple profiles likely cause the site engagement service-based
// tests to flake.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_WIN_MAC_CHROMEOS(x) x
#endif

// This test creates new browser with no profile and loads a random page with
// the feature flags enables the PreviewsOnePlatformHints. We confirm that the
// top_host_provider_impl executes and does not crash by checking UMA
// histograms for the total number of TopEngagementSites and
// the total number of sites returned controlled by the experiments flag
// |max_oneplatform_update_hosts|.
IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherEnabled)) {
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
// both HTTP and HTTPS sites. The test confirms that PreviewsTopHostProviderImpl
// used by PreviewsOptimizationGuide to provide a list of hosts to HintsFetcher
// only returns HTTPS-schemed hosts. We verify this with the UMA histogram
// logged when the GetHintsRequest is made to the remote Optimization Guide
// Service.
IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(PreviewsTopHostProviderHTTPSOnly)) {
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

  EXPECT_EQ(0u, GetTopHostBlacklistSize());
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherFetchedHintsLoaded)) {
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

  // Verifies that the fetched hint is loaded and not the component hint as
  // fetched hints are prioritized.

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(optimization_guide::OptimizationGuideStore::
                           StoreEntryType::kFetchedHint),
      1);

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(optimization_guide::OptimizationGuideStore::
                           StoreEntryType::kComponentHint),
      0);
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherWithResponsesSuccessful)) {
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

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherWithResponsesUnsuccessful)) {
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

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherWithResponsesMalformed)) {
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

  LoadHintsForUrl(https_url());

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Verifies that no Fetched Hint was added to the store, only the
  // Component hint is loaded.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(optimization_guide::OptimizationGuideStore::
                           StoreEntryType::kComponentHint),
      1);
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(optimization_guide::OptimizationGuideStore::
                           StoreEntryType::kFetchedHint),
      0);
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherClearFetchedHints)) {
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

  // Verifies that the fetched hint is loaded and not the component hint as
  // fetched hints are prioritized.
  EXPECT_LE(1,
            GetCountBucketSamples(
                histogram_tester, "OptimizationGuide.HintCache.HintType.Loaded",
                static_cast<int>(optimization_guide::OptimizationGuideStore::
                                     StoreEntryType::kFetchedHint)));

  EXPECT_EQ(0,
            GetCountBucketSamples(
                histogram_tester, "OptimizationGuide.HintCache.HintType.Loaded",
                static_cast<int>(optimization_guide::OptimizationGuideStore::
                                     StoreEntryType::kComponentHint)));

  // Wipe the browser history - clear all the fetched hints.
  browser()->profile()->Wipe();

  // Try to load the same hint to confirm fetched hints are no longer there.
  LoadHintsForUrl(https_url());

  ui_test_utils::NavigateToURL(browser(), https_url());

  // Fetched Hints count should not change.
  EXPECT_LE(1,
            GetCountBucketSamples(
                histogram_tester, "OptimizationGuide.HintCache.HintType.Loaded",
                static_cast<int>(optimization_guide::OptimizationGuideStore::
                                     StoreEntryType::kFetchedHint)));

  EXPECT_LE(0,
            GetCountBucketSamples(
                histogram_tester, "OptimizationGuide.HintCache.HintType.Loaded",
                static_cast<int>(optimization_guide::OptimizationGuideStore::
                                     StoreEntryType::kComponentHint)));
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherOverrideTimer)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      optimization_guide::switches::kFetchHintsOverride);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kFetchHintsOverrideTimer);

  SeedSiteEngagementService();

  // Set the blacklist state to initialized so the sites in the engagement
  // service will be used and not blacklisted on the first GetTopHosts request.
  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);

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

  // Verifies that the fetched hint is loaded and not the component hint as
  // fetched hints are prioritized.
  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(optimization_guide::OptimizationGuideStore::
                           StoreEntryType::kFetchedHint),
      1);

  histogram_tester->ExpectBucketCount(
      "OptimizationGuide.HintCache.HintType.Loaded",
      static_cast<int>(optimization_guide::OptimizationGuideStore::
                           StoreEntryType::kComponentHint),
      0);
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherNetworkOffline)) {
  const base::HistogramTester* histogram_tester = GetHistogramTester();
  GURL url = https_url();
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      optimization_guide::switches::kFetchHintsOverride);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kFetchHintsOverrideTimer);

  // Set the network to be offline.
  SetNetworkConnectionOffline();

  // Set the blacklist state to initialized so the sites in the engagement
  // service will be used and not blacklisted on the first GetTopHosts
  // request.
  SeedSiteEngagementService();

  // Set the blacklist state to initialized so the sites in the engagement
  // service will be used and not blacklisted on the first GetTopHosts request.
  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);

  // Whitelist NoScript for https_url()'s' host.
  SetUpComponentUpdateHints(https_url());

  // No HintsFetch should occur because the connection is offline.
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}

IN_PROC_BROWSER_TEST_F(HintsFetcherBrowserTest,
                       DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherHostCovered)) {
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

  // Navigation to a host in the seeded site engagement service; it should
  // be recorded as covered by the hints fetcher.
  ui_test_utils::NavigateToURL(browser(), GURL("https://example1.com"));

  RetryForHistogramUntilCountReached(
      histogram_tester,
      "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", 1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", true, 1);
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherHostNotCovered)) {
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

  // Navigate to a host not in the seeded site engagement service; it
  // should be recorded as not covered by the hints fetcher.
  ui_test_utils::NavigateToURL(browser(), GURL("https://unSeenHost.com"));

  RetryForHistogramUntilCountReached(
      histogram_tester,
      "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", 1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", false, 1);
}

// Test that the hints are fetched at the time of the navigation.
IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcher_NavigationFetch_ECT)) {
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

  // Change ECT to a low value. Hints should be fetched at the time of
  // navigation.
  {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_2G);

    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as covered by the hints fetcher due to the race.
    base::flat_set<std::string> expected_hosts_2g;
    std::string host_2g("https://unseenhost_2g.com/");
    expected_hosts_2g.insert(GURL(host_2g).host());
    SetExpectedHintsRequestForHosts(expected_hosts_2g);
    ui_test_utils::NavigateToURL(browser(), GURL(host_2g));

    RetryForHistogramUntilCountReached(
        histogram_tester,
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", 1);

    histogram_tester->ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", false,
        1);
    EXPECT_EQ(2u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        2);
    RetryForHistogramUntilCountReached(
        histogram_tester,
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
        "AtCommit",
        1);
    histogram_tester->ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
        "AtCommit",
        true, 1);
  }

  // Change ECT to a high value. Hints should not be fetched at the time of
  // navigation as the ECT fast so the fetcher should not race.
  {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_4G);

    base::flat_set<std::string> expected_hosts_4g;
    std::string host_4g("https://unseenhost_4g.com/");
    expected_hosts_4g.insert((GURL(host_4g).host()));
    SetExpectedHintsRequestForHosts(expected_hosts_4g);
    ui_test_utils::NavigateToURL(browser(), GURL(host_4g));

    RetryForHistogramUntilCountReached(
        histogram_tester,
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", 2);

    histogram_tester->ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", false,
        2);
    EXPECT_EQ(2u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        2);
    RetryForHistogramUntilCountReached(
        histogram_tester,
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
        "AtCommit",
        2);
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
        "AtCommit",
        true, 1);
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
        "AtCommit",
        false, 1);
  }

  // Change ECT back to a low value. Hints should be fetched at the time of
  // navigation.
  {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(
            net::EFFECTIVE_CONNECTION_TYPE_3G);

    // Navigate to a host not in the seeded site engagement service; it
    // should be recorded as not covered by the hints fetcher.
    base::flat_set<std::string> expected_hosts_3g;
    std::string host_3g("https://unseenhost_3g.com/");
    expected_hosts_3g.insert(GURL(host_3g).host());
    SetExpectedHintsRequestForHosts(expected_hosts_3g);
    ui_test_utils::NavigateToURL(browser(), GURL(host_3g));

    RetryForHistogramUntilCountReached(
        histogram_tester,
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", 3);

    histogram_tester->ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", false,
        3);
    EXPECT_EQ(3u, count_hints_requests_received());
    RetryForHistogramUntilCountReached(
        histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
        3);

    RetryForHistogramUntilCountReached(
        histogram_tester,
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
        "AtCommit",
        3);
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
        "AtCommit",
        true, 2);
    histogram_tester->ExpectBucketCount(
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
        "AtCommit",
        false, 1);
  }

  // Navigate again to a webpage with the
  // same host. Hints should be available at the time of
  // navigation.
  {
    // Navigate to a host that was recently fetched. It
    // should be recorded as covered by the hints fetcher.
    base::flat_set<std::string> expected_hosts_3g;
    std::string host_3g("https://unseenhost_3g.com");
    expected_hosts_3g.insert(GURL(host_3g).host());
    SetExpectedHintsRequestForHosts(expected_hosts_3g);
    ui_test_utils::NavigateToURL(browser(),
                                 GURL("https://unseenhost_3g.com/test1.html"));

    RetryForHistogramUntilCountReached(
        histogram_tester,
        "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", 4);

      // Hints should be available this time for the navigation.
      histogram_tester->ExpectBucketCount(
          "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", false,
          3);
      histogram_tester->ExpectBucketCount(
          "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", true,
          1);
    // Hints should not be fetched for the same host again.
      EXPECT_EQ(3u, count_hints_requests_received());
      RetryForHistogramUntilCountReached(
          histogram_tester, optimization_guide::kLoadedHintLocalHistogramString,
          4);

      RetryForHistogramUntilCountReached(
          histogram_tester,
          "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
          "AtCommit",
          4);
      histogram_tester->ExpectBucketCount(
          "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
          "AtCommit",
          true, 3);
      histogram_tester->ExpectBucketCount(
          "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
          "AtCommit",
          false, 1);
  }
}

IN_PROC_BROWSER_TEST_F(
    HintsFetcherBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcherHostCoveredNotHTTPS)) {
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

  // Navigate to a HTTP host; the navigation should not be recorded.
  ui_test_utils::NavigateToURL(browser(), GURL("http://example1.com"));

  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", 0);
}

class HintsFetcherChangeDefaultBlacklistSizeBrowserTest
    : public HintsFetcherBrowserTest {
 public:
  HintsFetcherChangeDefaultBlacklistSizeBrowserTest() = default;

  ~HintsFetcherChangeDefaultBlacklistSizeBrowserTest() override = default;

  void SetUp() override {
    base::FieldTrialParams optimization_hints_fetching_params;
    optimization_hints_fetching_params["top_host_blacklist_size_multiplier"] =
        "5";

      scoped_feature_list_.InitWithFeaturesAndParameters(
          {
              /* vector of enabled features along with params */
              {optimization_guide::features::kOptimizationHintsFetching,
               {optimization_hints_fetching_params}},
              {optimization_guide::features::kOptimizationHints, {}},
              {previews::features::kPreviews, {}},
              {previews::features::kNoScriptPreviews, {}},
              {previews::features::kResourceLoadingHints, {}},
              {data_reduction_proxy::features::
                   kDataReductionProxyEnabledWithNetworkService,
               {}},
          },
          {/* disabled_features */});

    // Call to inherited class to match same set up with feature flags added.
    HintsFetcherDisabledBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    // Due to race conditions, it's possible that blacklist data is not loaded
    // at the time of first navigation. That may prevent Preview from
    // triggering, and causing the test to flake.
    cmd->AppendSwitch(previews::switches::kIgnorePreviewsBlacklist);
    cmd->AppendSwitch("purge_hint_cache_store");

    // Set up OptimizationGuideServiceURL, this does not enable HintsFetching,
    // only provides the URL.
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceGetHintsURL,
        hints_server_->base_url().spec());

    cmd->AppendSwitch(previews::switches::kDoNotRequireLitePageRedirectInfoBar);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HintsFetcherChangeDefaultBlacklistSizeBrowserTest);
};

// Changes the default size of the top host blacklist using finch. Also, sets
// the count of hosts previously engaged to a large number and verifies that the
// top host blacklist is correctly populated.
IN_PROC_BROWSER_TEST_F(HintsFetcherChangeDefaultBlacklistSizeBrowserTest,
                       ChangeDefaultBlacklistSize) {
  AddHostsToSiteEngagementService(120u);
  const size_t engaged_hosts = GetCountHostsKnownToSiteEngagementService();
  EXPECT_EQ(120u, engaged_hosts);

  // Ensure everything within the site engagement service fits within the top
  // host blacklist size.
  ASSERT_LE(
      engaged_hosts,
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize());

    SetTopHostBlacklistState(
        optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
            kNotInitialized);

    OptimizationGuideKeyedService* keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide::TopHostProvider* top_host_provider =
        keyed_service->GetTopHostProvider();
    ASSERT_TRUE(top_host_provider);

    std::vector<std::string> top_hosts = top_host_provider->GetTopHosts();
    EXPECT_EQ(0u, top_hosts.size());

    top_hosts = top_host_provider->GetTopHosts();
    EXPECT_EQ(0u, top_hosts.size());

    // Everything HTTPS origin within the site engagement service should now be
    // in the blacklist.
    EXPECT_EQ(engaged_hosts, GetTopHostBlacklistSize());
}

class HintsFetcherSearchPageBrowserTest : public HintsFetcherBrowserTest {
  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableFetchingHintsAtNavigationStartForTesting);
    cmd->AppendSwitch("ignore-certificate-errors");
    HintsFetcherBrowserTest::SetUpCommandLine(cmd);
  }
};

IN_PROC_BROWSER_TEST_F(
    HintsFetcherSearchPageBrowserTest,
    DISABLE_ON_WIN_MAC_CHROMEOS(HintsFetcher_SRP_Slow_Connection)) {
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
  base::flat_set<std::string> expected_hosts;
  expected_hosts.insert(GURL("https://foo.com").host());
  expected_hosts.insert(GURL("https://example.com").host());
  expected_hosts.insert(GURL("https://example3.com").host());
  SetExpectedHintsRequestForHosts(expected_hosts);

  histogram_tester->ExpectTotalCount(
      optimization_guide::kLoadedHintLocalHistogramString, 0);

  // Navigate to a host not in the seeded site engagement service; it
  // should be recorded as not covered by the hints fetcher.
  ResetCountHintsRequestsReceived();
  ui_test_utils::NavigateToURL(browser(), search_results_page_url());
  WaitForPageLayout();

  RetryForHistogramUntilCountReached(
      histogram_tester, "AnchorElementMetrics.Visible.HighestNavigationScore",
      1);

  RetryForHistogramUntilCountReached(
      histogram_tester,
      "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", 1);

  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch", false, 1);

  WaitUntilHintsFetcherRequestReceived();
  EXPECT_EQ(1u, count_hints_requests_received());

  RetryForHistogramUntilCountReached(
      histogram_tester, optimization_guide::kLoadedHintLocalHistogramString, 2);
  // Only the SRP is navigated to and it should not be covered at navigation
  // finish.
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.NavigationHostCoveredByFetch."
      "AtCommit",
      false, 1);
}
