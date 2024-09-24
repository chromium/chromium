// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/prefetch_manager.h"

#include <map>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/predictors_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/origin.h"

namespace predictors {

namespace {

using ::testing::UnorderedElementsAreArray;

class FakePrefetchManagerDelegate : public PrefetchManager::Delegate {
 public:
  void PrefetchInitiated(const GURL& url, const GURL& prefetch_url) override {
    prefetched_urls_for_main_frame_url_[url].insert(prefetch_url);
  }

  void PrefetchFinished(std::unique_ptr<PrefetchStats> stats) override {
    finished_urls_.insert(stats->url);
    auto iter = done_callbacks_.find(stats->url);
    if (iter == done_callbacks_.end())
      return;
    auto callback = std::move(iter->second);
    done_callbacks_.erase(iter);
    std::move(callback).Run();
  }

  void WaitForPrefetchFinished(const GURL& url) {
    if (finished_urls_.find(url) != finished_urls_.end())
      return;
    base::RunLoop loop;
    DCHECK(done_callbacks_.find(url) == done_callbacks_.end());
    done_callbacks_[url] = loop.QuitClosure();
    loop.Run();
  }

  base::flat_set<GURL> GetPrefetchedURLsForURL(const GURL& url) const {
    auto it = prefetched_urls_for_main_frame_url_.find(url);
    if (it == prefetched_urls_for_main_frame_url_.end())
      return {};
    return it->second;
  }

  void ClearPrefetchedURLs() { prefetched_urls_for_main_frame_url_ = {}; }

  base::WeakPtr<FakePrefetchManagerDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::flat_map<GURL, base::flat_set<GURL>>
      prefetched_urls_for_main_frame_url_;
  base::flat_set<GURL> finished_urls_;
  base::flat_map<GURL, base::OnceClosure> done_callbacks_;
  base::WeakPtrFactory<FakePrefetchManagerDelegate> weak_ptr_factory_{this};
};

// Creates a NetworkAnonymizationKey for a main frame navigation to URL.
net::NetworkAnonymizationKey CreateNetworkIsolationKey(
    const GURL& main_frame_url) {
  net::SchemefulSite site = net::SchemefulSite(main_frame_url);
  return net::NetworkAnonymizationKey::CreateSameSite(site);
}

PrefetchRequest CreateScriptRequest(const GURL& url,
                                    const GURL& main_frame_url) {
  return PrefetchRequest(url, CreateNetworkIsolationKey(main_frame_url),
                         network::mojom::RequestDestination::kScript);
}

PrefetchRequest CreateFontRequest(const GURL& url, const GURL& main_frame_url) {
  return PrefetchRequest(url, CreateNetworkIsolationKey(main_frame_url),
                         network::mojom::RequestDestination::kFont);
}

}  // namespace

// A test fixture for the PrefetchManager.
class PrefetchManagerTest : public testing::TestWithParam<bool> {
 public:
  PrefetchManagerTest();
  ~PrefetchManagerTest() override = default;

  PrefetchManagerTest(const PrefetchManagerTest&) = delete;
  PrefetchManagerTest& operator=(const PrefetchManagerTest&) = delete;

 protected:
  size_t GetQueuedJobsCount() const {
    return prefetch_manager_->queued_jobs_.size();
  }

  void CheckHeaders(network::ResourceRequest& request) {
    EXPECT_THAT(request.headers.GetHeader("Purpose"),
                testing::Optional(std::string("prefetch")));
    EXPECT_THAT(request.headers.GetHeader("Sec-Purpose"),
                testing::Optional(std::string("prefetch")));
  }

  base::test::ScopedFeatureList features_;
  // IO_MAINLOOP is needed for the EmbeddedTestServer.
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FakePrefetchManagerDelegate> fake_delegate_;
  std::unique_ptr<PrefetchManager> prefetch_manager_;
};

PrefetchManagerTest::PrefetchManagerTest()
    : profile_(std::make_unique<TestingProfile>()),
      fake_delegate_(std::make_unique<FakePrefetchManagerDelegate>()),
      prefetch_manager_(
          std::make_unique<PrefetchManager>(fake_delegate_->AsWeakPtr(),
                                            profile_.get())) {
  if (GetParam()) {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {features::kLoadingPredictorPrefetch,
         features::kLoadingPredictorPrefetchUseReadAndDiscardBody},
        /*disabled_features=*/{});
  } else {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kLoadingPredictorPrefetch},
        /*disabled_features=*/{
            features::kLoadingPredictorPrefetchUseReadAndDiscardBody});
  }
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kLoadingPredictorAllowLocalRequestForTesting);
}

// Tests prefetching a single URL.
TEST_P(PrefetchManagerTest, OneMainFrameUrlOnePrefetch) {
  GURL main_frame_url("https://abc.invalid");
  GURL subresource_url("https://xyz.invalid/script.js");
  PrefetchRequest request =
      CreateScriptRequest(subresource_url, main_frame_url);

  base::RunLoop loop;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        network::ResourceRequest& request = params->url_request;
        EXPECT_EQ(request.url, subresource_url);
        EXPECT_TRUE(request.load_flags & net::LOAD_PREFETCH);

        EXPECT_EQ(request.referrer_policy, net::ReferrerPolicy::NO_REFERRER);
        EXPECT_EQ(request.destination,
                  network::mojom::RequestDestination::kScript);
        EXPECT_EQ(
            static_cast<blink::mojom::ResourceType>(request.resource_type),
            blink::mojom::ResourceType::kScript);

        EXPECT_EQ(request.mode, network::mojom::RequestMode::kNoCors);

        CheckHeaders(request);

        loop.Quit();
        return false;
      }));
  prefetch_manager_->Start(main_frame_url, {request});
  loop.Run();

  EXPECT_THAT(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url),
              UnorderedElementsAreArray({subresource_url}));

  fake_delegate_->WaitForPrefetchFinished(main_frame_url);
}

// Tests prefetching multiple URLs.
TEST_P(PrefetchManagerTest, OneMainFrameUrlMultiplePrefetch) {
  net::test_server::EmbeddedTestServer test_server;
  std::vector<std::string> paths;
  std::vector<PrefetchRequest> requests;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;

  GURL main_frame_url("https://abc.invalid");

  // Set up prefetches one more than the inflight limit.

  // The ControllableHttpResponses must be made before the test server
  // is started.
  for (size_t i = 0; i < kMaxInflightPrefetches + 1; i++) {
    std::string path = base::StringPrintf("/script%" PRIuS ".js", i);
    paths.push_back(path);
    responses.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &test_server, path));
  }

  // Start the server.
  auto test_server_handle = test_server.StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // The request URLs can only be constructed after the server is started.
  for (size_t i = 0; i < responses.size(); i++) {
    GURL url = test_server.GetURL(paths[i]);
    requests.push_back(CreateScriptRequest(url, main_frame_url));
  }

  // Start the prefetching.
  prefetch_manager_->Start(main_frame_url, std::move(requests));

  // Wait for requests up to the inflight limit.
  std::vector<GURL> prefetched_urls;
  for (size_t i = 0; i < responses.size() - 1; i++) {
    prefetched_urls.push_back(test_server.GetURL(paths[i]));
    responses[i]->WaitForRequest();
  }

  EXPECT_THAT(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url),
              UnorderedElementsAreArray(prefetched_urls));

  // Verify there is a queued job. Pump the run loop just to give the manager a
  // chance to incorrectly start the queued job and fail the expectation.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetQueuedJobsCount(), 1u);

  fake_delegate_->ClearPrefetchedURLs();

  // Finish one request.
  responses.front()->Send("hi");
  responses.front()->Done();

  // Wait for the queued job to start.
  responses.back()->WaitForRequest();
  EXPECT_EQ(GetQueuedJobsCount(), 0u);

  EXPECT_THAT(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url),
              UnorderedElementsAreArray({test_server.GetURL(paths.back())}));

  // Finish all requests.
  for (size_t i = 1; i < responses.size(); i++) {
    responses[i]->Send("hi");
    responses[i]->Done();
  }
  fake_delegate_->WaitForPrefetchFinished(main_frame_url);
}

// Tests that metrics related to queueing of prefetch jobs are recorded.
TEST_P(PrefetchManagerTest, QueueingMetricsRecorded) {
  base::HistogramTester histogram_tester;
  net::test_server::EmbeddedTestServer test_server;
  std::vector<PrefetchRequest> requests;
  size_t num_prefetches = kMaxInflightPrefetches;

  GURL main_frame_url("https://abc.invalid");

  // Start the server.
  auto test_server_handle = test_server.StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // Set up prefetches one more than the inflight limit.
  // The request URLs can only be constructed after the server is started.
  for (size_t i = 0; i < num_prefetches + 1; i++) {
    std::string path = base::StringPrintf("/script%" PRIuS ".js", i);
    GURL url = test_server.GetURL(path);
    requests.push_back(CreateScriptRequest(url, main_frame_url));
  }

  // Start the prefetching.
  prefetch_manager_->Start(main_frame_url, std::move(requests));

  // The number of queued jobs should have been recorded.
  histogram_tester.ExpectUniqueSample(
      "Navigation.Prefetch.PrefetchJobQueueLength", num_prefetches + 1, 1);
  // Each job that was actually executed should have had its queueing time
  // recorded.
  histogram_tester.ExpectTotalCount(
      "Navigation.Prefetch.PrefetchJobQueueingTime", num_prefetches);
}

// Tests prefetching multiple URLs for multiple main frames.
TEST_P(PrefetchManagerTest, MultipleMainFrameUrlMultiplePrefetch) {
  net::test_server::EmbeddedTestServer test_server;
  std::vector<std::string> paths;
  std::vector<PrefetchRequest> requests;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;

  GURL main_frame_url("https://abc.invalid");
  GURL main_frame_url2("https://def.invalid");

  // Set up prefetches one more than the inflight limit.
  size_t count = kMaxInflightPrefetches;

  // The ControllableHttpResponses must be made before the test server
  // is started.
  for (size_t i = 0; i < count + 1; i++) {
    std::string path = base::StringPrintf("/script%" PRIuS ".js", i);
    paths.push_back(path);
    responses.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &test_server, path));
  }

  // Start the server.
  auto test_server_handle = test_server.StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // The request URLs can only be constructed after the server is started.
  std::vector<GURL> expected_prefetch_requests_for_main_frame_url;
  for (size_t i = 0; i < count - 1; i++) {
    GURL url = test_server.GetURL(paths[i]);
    requests.push_back(CreateScriptRequest(url, main_frame_url));
    expected_prefetch_requests_for_main_frame_url.push_back(url);
  }
  std::vector<GURL> expected_prefetch_requests_for_main_frame_url2;
  for (size_t i = count - 1; i < count + 1; i++) {
    GURL url = test_server.GetURL(paths[i]);
    requests.push_back(CreateScriptRequest(url, main_frame_url2));
    expected_prefetch_requests_for_main_frame_url2.push_back(url);
  }

  // Start the prefetching.
  prefetch_manager_->Start(main_frame_url,
                           std::vector<PrefetchRequest>(
                               requests.begin(), requests.begin() + count - 1));
  prefetch_manager_->Start(main_frame_url2,
                           std::vector<PrefetchRequest>(
                               requests.begin() + count - 1, requests.end()));

  // Wait for requests up to the inflight limit.
  for (size_t i = 0; i < responses.size() - 1; i++)
    responses[i]->WaitForRequest();

  // Verify there is a queued job. Pump the run loop just to give the manager a
  // chance to incorrectly start the queued job and fail the expectation.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetQueuedJobsCount(), 1u);

  EXPECT_THAT(
      fake_delegate_->GetPrefetchedURLsForURL(main_frame_url),
      UnorderedElementsAreArray(expected_prefetch_requests_for_main_frame_url));
  EXPECT_THAT(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url2),
              UnorderedElementsAreArray(
                  {expected_prefetch_requests_for_main_frame_url2.front()}));

  fake_delegate_->ClearPrefetchedURLs();

  // Finish one request.
  responses.front()->Send("hi");
  responses.front()->Done();

  // Wait for the queued job to start.
  responses.back()->WaitForRequest();
  EXPECT_EQ(GetQueuedJobsCount(), 0u);

  // We don't expect any more requests for |main_frame_url| to be initiated and
  // we expect the last request for |main_frame_url2| to go out.
  EXPECT_TRUE(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url).empty());
  EXPECT_THAT(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url2),
              UnorderedElementsAreArray(
                  {expected_prefetch_requests_for_main_frame_url2.back()}));

  // Finish all requests.
  for (size_t i = 1; i < responses.size(); i++) {
    responses[i]->Send("hi");
    responses[i]->Done();
  }
  fake_delegate_->WaitForPrefetchFinished(main_frame_url);
  fake_delegate_->WaitForPrefetchFinished(main_frame_url2);
}

TEST_P(PrefetchManagerTest, Stop) {
  net::test_server::EmbeddedTestServer test_server;

  // Set up prefetches (limit + 1 for URL1, and 1 for URL2)
  size_t limit = kMaxInflightPrefetches;

  GURL main_frame_url("https://abc.invalid");
  std::vector<std::string> paths;
  std::vector<PrefetchRequest> requests;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;

  GURL main_frame_url2("https://def.invalid");
  std::string path2;
  std::unique_ptr<net::test_server::ControllableHttpResponse> response2;

  // The ControllableHttpResponses must be made before the test server
  // is started.
  for (size_t i = 0; i < limit; i++) {
    std::string path = base::StringPrintf("/script%" PRIuS ".js", i);
    paths.push_back(path);
    responses.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &test_server, path));
  }

  path2 = base::StringPrintf("/script%" PRIuS ".js", limit);
  response2 = std::make_unique<net::test_server::ControllableHttpResponse>(
      &test_server, path2);

  // Verify we don't see a request after Stop().
  test_server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        EXPECT_NE(request.relative_url, "/should_be_cancelled");
      }));

  // Start the server.
  auto test_server_handle = test_server.StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // The request URLs can only be constructed after the server is started.
  std::vector<GURL> expected_prefetch_requests;
  for (size_t i = 0; i < limit; i++) {
    GURL url = test_server.GetURL(paths[i]);
    requests.push_back(CreateScriptRequest(url, main_frame_url));
    expected_prefetch_requests.push_back(url);
  }
  // This request should never be seen.
  requests.push_back(CreateScriptRequest(
      test_server.GetURL("/should_be_cancelled"), main_frame_url));

  // The request from the second navigation.
  PrefetchRequest request2 =
      CreateScriptRequest(test_server.GetURL(path2), main_frame_url2);

  // Start URL1, URL2.
  prefetch_manager_->Start(main_frame_url, requests);
  prefetch_manager_->Start(main_frame_url2, {request2});

  // Wait for |limit| requests from URL1.
  for (auto& response : responses)
    response->WaitForRequest();

  // Call stop on URL1.
  prefetch_manager_->Stop(main_frame_url);

  // Let URL1 requests finish. This finishes URL1 without
  // the limit + 1 request being sent.
  for (auto& response : responses) {
    response->Send("hi");
    response->Done();
  }
  fake_delegate_->WaitForPrefetchFinished(main_frame_url);

  EXPECT_THAT(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url),
              UnorderedElementsAreArray(expected_prefetch_requests));

  // The request for URL2 should be requested.
  response2->WaitForRequest();
  response2->Send("hi");
  response2->Done();

  fake_delegate_->WaitForPrefetchFinished(main_frame_url2);

  EXPECT_THAT(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url2),
              UnorderedElementsAreArray({test_server.GetURL(path2)}));
}

// Flaky on Mac/Linux/CrOS/Android/Windows. http://crbug.com/1239235
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_StopAndStart DISABLED_StopAndStart
#else
#define MAYBE_StopAndStart StopAndStart
#endif
TEST_P(PrefetchManagerTest, MAYBE_StopAndStart) {
  net::test_server::EmbeddedTestServer test_server;

  // Set up prefetches (limit + 1).
  size_t limit = kMaxInflightPrefetches;

  GURL main_frame_url("https://abc.invalid");
  std::vector<std::string> paths;
  std::vector<PrefetchRequest> requests;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      responses2;

  // The ControllableHttpResponses must be made before the test server
  // is started.
  for (size_t i = 0; i < limit; i++) {
    std::string path = base::StringPrintf("/script%" PRIuS ".js", i);
    paths.push_back(path);
    responses.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &test_server, path));
    responses2.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &test_server, path));
  }

  // Verify we don't see a request after Stop().
  test_server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        EXPECT_NE(request.relative_url, "/should_be_cancelled");
      }));

  // Start the server.
  auto test_server_handle = test_server.StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  // The request URLs can only be constructed after the server is started.
  std::vector<GURL> expected_prefetch_requests;
  for (size_t i = 0; i < limit; i++) {
    GURL url = test_server.GetURL(paths[i]);
    requests.push_back(CreateScriptRequest(url, main_frame_url));
    expected_prefetch_requests.push_back(url);
  }
  // This request should never be seen.
  requests.push_back(CreateScriptRequest(
      test_server.GetURL("/should_be_cancelled"), main_frame_url));

  // Start.
  prefetch_manager_->Start(main_frame_url, requests);

  // Wait for |limit| requests from URL1.
  for (auto& response : responses) {
    response->WaitForRequest();
  }
  EXPECT_THAT(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url),
              UnorderedElementsAreArray(expected_prefetch_requests));

  // Call stop.
  prefetch_manager_->Stop(main_frame_url);

  fake_delegate_->ClearPrefetchedURLs();

  // Call start again. These requests will be coalesced
  // with the stopped info, and will just be dropped.
  prefetch_manager_->Start(main_frame_url, requests);

  // Let the inflight requests finish. This finishes the
  // info without the limit + 1 request or requests
  // added after Stop() being sent.
  for (auto& response : responses) {
    response->Send("hi");
    response->Done();
  }
  fake_delegate_->WaitForPrefetchFinished(main_frame_url);

  // We don't expect any additional requests to be started.
  EXPECT_TRUE(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url).empty());

  // Restart requests. These requests will work as normal.
  prefetch_manager_->Start(main_frame_url, requests);
  for (auto& response : responses2) {
    response->WaitForRequest();
    response->Send("hi");
    response->Done();
  }

  fake_delegate_->WaitForPrefetchFinished(main_frame_url);

  // Prefetches should have been initiated with the second start.
  EXPECT_FALSE(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url).empty());
}

class HeaderInjectingThrottle : public blink::URLLoaderThrottle {
 public:
  HeaderInjectingThrottle() = default;
  ~HeaderInjectingThrottle() override = default;

  HeaderInjectingThrottle(const HeaderInjectingThrottle&) = delete;
  HeaderInjectingThrottle& operator=(const HeaderInjectingThrottle&) = delete;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    request->headers.SetHeader("x-injected", "injected value");
  }
};

class ThrottlingContentBrowserClient : public content::ContentBrowserClient {
 public:
  ThrottlingContentBrowserClient() = default;
  ~ThrottlingContentBrowserClient() override = default;

  ThrottlingContentBrowserClient(const ThrottlingContentBrowserClient&) =
      delete;
  ThrottlingContentBrowserClient& operator=(
      const ThrottlingContentBrowserClient&) = delete;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      content::FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id) override {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    throttles.emplace_back(std::make_unique<HeaderInjectingThrottle>());
    return throttles;
  }
};

// Test that prefetches go through URLLoaderThrottles.
TEST_P(PrefetchManagerTest, Throttles) {
  // Add a throttle which injects a header.
  ThrottlingContentBrowserClient content_browser_client;
  auto* old_content_browser_client =
      content::SetBrowserClientForTesting(&content_browser_client);

  net::test_server::EmbeddedTestServer test_server;
  net::test_server::ControllableHttpResponse response(&test_server,
                                                      "/prefetch");

  // Start the server.
  auto test_server_handle = test_server.StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle);

  GURL main_frame_url("https://abc.invalid");
  GURL prefetch_url = test_server.GetURL("/prefetch");
  PrefetchRequest request = CreateScriptRequest(prefetch_url, main_frame_url);

  prefetch_manager_->Start(main_frame_url, {request});

  response.WaitForRequest();
  const net::test_server::HttpRequest* actual_request = response.http_request();
  auto iter = actual_request->headers.find("x-injected");
  ASSERT_TRUE(iter != actual_request->headers.end());
  EXPECT_EQ(iter->second, "injected value");

  content::SetBrowserClientForTesting(old_content_browser_client);
}

// Tests prefetching a font URL.
TEST_P(PrefetchManagerTest, Font) {
  GURL main_frame_url("https://abc.invalid");
  GURL subresource_url("https://xyz.invalid/font.woff");
  PrefetchRequest request = CreateFontRequest(subresource_url, main_frame_url);

  base::RunLoop loop;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        network::ResourceRequest& request = params->url_request;
        EXPECT_EQ(request.url, subresource_url);
        EXPECT_TRUE(request.load_flags & net::LOAD_PREFETCH);

        EXPECT_EQ(request.referrer_policy, net::ReferrerPolicy::NO_REFERRER);
        EXPECT_EQ(request.destination,
                  network::mojom::RequestDestination::kFont);
        EXPECT_EQ(
            static_cast<blink::mojom::ResourceType>(request.resource_type),
            blink::mojom::ResourceType::kFontResource);

        EXPECT_EQ(request.mode, network::mojom::RequestMode::kNoCors);

        CheckHeaders(request);

        loop.Quit();
        return false;
      }));
  prefetch_manager_->Start(main_frame_url, {request});
  loop.Run();

  EXPECT_THAT(fake_delegate_->GetPrefetchedURLsForURL(main_frame_url),
              UnorderedElementsAreArray({subresource_url}));

  fake_delegate_->WaitForPrefetchFinished(main_frame_url);
}

INSTANTIATE_TEST_SUITE_P(PrefetchManagerTest,
                         PrefetchManagerTest,
                         ::testing::Bool(),
                         ::testing::PrintToStringParamName());

}  // namespace predictors
