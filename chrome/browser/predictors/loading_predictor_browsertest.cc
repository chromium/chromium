// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_preconnect_client.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/predictors/predictors_enums.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/predictors_switches.h"
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/base/escape.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserThread;
using testing::Optional;
using testing::SizeIs;

namespace predictors {

const char kChromiumUrl[] = "http://chromium.org";

const char kHtmlSubresourcesPath[] = "/predictors/html_subresources.html";
// The embedded test server runs on test.com.
// kHtmlSubresourcesPath contains high priority resources from baz.com and
// foo.com. kHtmlSubresourcesPath also contains a low priority resource from
// bar.com.
const char* const kHtmlSubresourcesHosts[] = {"test.com", "baz.com", "foo.com"};

std::string GetPathWithPortReplacement(const std::string& path, uint16_t port) {
  std::string string_port = base::StringPrintf("%d", port);
  return net::test_server::GetFilePathWithReplacements(
      path, {{"REPLACE_WITH_PORT", string_port}});
}

GURL GetDataURLWithContent(const std::string& content) {
  std::string encoded_content;
  base::Base64Encode(content, &encoded_content);
  std::string data_uri_content = "data:text/html;base64," + encoded_content;
  return GURL(data_uri_content);
}

// Helper class to track and allow waiting for ResourcePrefetchPredictor
// initialization. WARNING: OnPredictorInitialized event will not be fired if
// ResourcePrefetchPredictor is initialized before the observer creation.
class PredictorInitializer : public TestObserver {
 public:
  explicit PredictorInitializer(ResourcePrefetchPredictor* predictor)
      : TestObserver(predictor), predictor_(predictor) {}

  void EnsurePredictorInitialized() {
    if (predictor_->initialization_state_ ==
        ResourcePrefetchPredictor::INITIALIZED) {
      return;
    }

    if (predictor_->initialization_state_ ==
        ResourcePrefetchPredictor::NOT_INITIALIZED) {
      predictor_->StartInitialization();
    }

    run_loop_.Run();
  }

  void OnPredictorInitialized() override { run_loop_.Quit(); }

 private:
  ResourcePrefetchPredictor* predictor_;
  base::RunLoop run_loop_;
  DISALLOW_COPY_AND_ASSIGN(PredictorInitializer);
};

// Keeps track of incoming connections being accepted or read from and exposes
// that info to the tests.
// A port being reused is currently considered an error.
// If a test needs to verify multiple connections are opened in sequence, that
// will need to be changed.
class ConnectionTracker {
 public:
  ConnectionTracker() {}

  void AcceptedSocketWithPort(uint16_t port) {
    EXPECT_FALSE(base::Contains(sockets_, port));
    sockets_[port] = SocketStatus::kAccepted;
    CheckAccepted();
    first_accept_loop_.Quit();
  }

  void ReadFromSocketWithPort(uint16_t port) {
    EXPECT_TRUE(base::Contains(sockets_, port));
    sockets_[port] = SocketStatus::kReadFrom;
    first_read_loop_.Quit();
  }

  // Returns the number of sockets that were accepted by the server.
  size_t GetAcceptedSocketCount() const { return sockets_.size(); }

  // Returns the number of sockets that were read from by the server.
  size_t GetReadSocketCount() const {
    size_t read_sockets = 0;
    for (const auto& socket : sockets_) {
      if (socket.second == SocketStatus::kReadFrom)
        ++read_sockets;
    }
    return read_sockets;
  }

  void WaitUntilFirstConnectionAccepted() { first_accept_loop_.Run(); }
  void WaitUntilFirstConnectionRead() { first_read_loop_.Run(); }

  // The UI thread will wait for exactly |num_connections| items in |sockets_|.
  // This method expects the server will not accept more than |num_connections|
  // connections. |num_connections| must be greater than 0.
  void WaitForAcceptedConnections(size_t num_connections) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!num_accepted_connections_loop_);
    DCHECK_GT(num_connections, 0u);
    base::RunLoop run_loop;
    EXPECT_GE(num_connections, sockets_.size());
    num_accepted_connections_loop_ = &run_loop;
    num_accepted_connections_needed_ = num_connections;
    CheckAccepted();
    // Note that the previous call to CheckAccepted can quit this run loop
    // before this call, which will make this call a no-op.
    run_loop.Run();
    EXPECT_EQ(num_connections, sockets_.size());
  }

  // Helper function to stop the waiting for sockets to be accepted for
  // WaitForAcceptedConnections. |num_accepted_connections_loop_| spins
  // until |num_accepted_connections_needed_| sockets are accepted by the test
  // server. The values will be null/0 if the loop is not running.
  void CheckAccepted() {
    // |num_accepted_connections_loop_| null implies
    // |num_accepted_connections_needed_| == 0.
    DCHECK(num_accepted_connections_loop_ ||
           num_accepted_connections_needed_ == 0);
    if (!num_accepted_connections_loop_ ||
        num_accepted_connections_needed_ != sockets_.size()) {
      return;
    }

    num_accepted_connections_loop_->Quit();
    num_accepted_connections_needed_ = 0;
    num_accepted_connections_loop_ = nullptr;
  }

  void ResetCounts() { sockets_.clear(); }

 private:
  enum class SocketStatus { kAccepted, kReadFrom };

  base::RunLoop first_accept_loop_;
  base::RunLoop first_read_loop_;

  // Port -> SocketStatus.
  using SocketContainer = std::map<uint16_t, SocketStatus>;
  SocketContainer sockets_;

  // If |num_accepted_connections_needed_| is non zero, then the object is
  // waiting for |num_accepted_connections_needed_| sockets to be accepted
  // before quitting the |num_accepted_connections_loop_|.
  size_t num_accepted_connections_needed_ = 0;
  base::RunLoop* num_accepted_connections_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ConnectionTracker);
};

// Gets notified by the EmbeddedTestServer on incoming connections being
// accepted or read from and transfers this information to ConnectionTracker.
class ConnectionListener
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  // This class should be constructed on the browser UI thread.
  explicit ConnectionListener(ConnectionTracker* tracker)
      : task_runner_(base::ThreadTaskRunnerHandle::Get()), tracker_(tracker) {}

  ~ConnectionListener() override {}

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was accepted.
  std::unique_ptr<net::StreamSocket> AcceptedSocket(
      std::unique_ptr<net::StreamSocket> connection) override {
    uint16_t port = GetPort(*connection);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConnectionTracker::AcceptedSocketWithPort,
                                  base::Unretained(tracker_), port));
    return connection;
  }

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was read from.
  void ReadFromSocket(const net::StreamSocket& connection, int rv) override {
    // Don't log a read if no data was transferred. This case often happens if
    // the sockets of the test server are being flushed and disconnected.
    if (rv <= 0)
      return;
    uint16_t port = GetPort(connection);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConnectionTracker::ReadFromSocketWithPort,
                                  base::Unretained(tracker_), port));
  }

 private:
  static uint16_t GetPort(const net::StreamSocket& connection) {
    // Get the remote port of the peer, since the local port will always be the
    // port the test server is listening on. This isn't strictly correct - it's
    // possible for multiple peers to connect with the same remote port but
    // different remote IPs - but the tests here assume that connections to the
    // test server (running on localhost) will always come from localhost, and
    // thus the peer port is all that's needed to distinguish two connections.
    // This also would be problematic if the OS reused ports, but that's not
    // something to worry about for these tests.
    net::IPEndPoint address;
    EXPECT_EQ(net::OK, connection.GetPeerAddress(&address));
    return address.port();
  }

  // Task runner associated with the browser UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This pointer should be only accessed on the browser UI thread.
  ConnectionTracker* tracker_;
};

class TestPreconnectManagerObserver : public PreconnectManager::Observer {
 public:
  explicit TestPreconnectManagerObserver(
      PreconnectManager* preconnect_manager) {
    preconnect_manager->SetObserverForTesting(this);
  }

  void OnPreconnectUrl(const GURL& url,
                       int num_sockets,
                       bool allow_credentials) override {
    preconnect_url_attempts_.insert(url.GetOrigin());
  }

  void OnPreresolveFinished(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      bool success) override {
    ResolveHostRequestInfo preconnect_info{url.host(), network_isolation_key};
    if (success)
      successful_dns_lookups_.insert(preconnect_info);
    else
      unsuccessful_dns_lookups_.insert(preconnect_info);
    CheckForWaitingLoop();
  }

  void OnProxyLookupFinished(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      bool success) override {
    ResolveProxyRequestInfo resolve_info{url::Origin::Create(url),
                                         network_isolation_key};
    if (success)
      successful_proxy_lookups_.insert(resolve_info);
    else
      unsuccessful_proxy_lookups_.insert(resolve_info);
    CheckForWaitingLoop();
  }

  void WaitUntilHostLookedUp(
      const std::string& host,
      const net::NetworkIsolationKey& network_isolation_key) {
    wait_event_ = WaitEvent::kDns;
    DCHECK(waiting_on_dns_.IsEmpty());
    waiting_on_dns_ = ResolveHostRequestInfo{host, network_isolation_key};
    Wait();
  }

  void WaitUntilProxyLookedUp(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key) {
    wait_event_ = WaitEvent::kProxy;
    DCHECK(waiting_on_proxy_.IsEmpty());
    waiting_on_proxy_ = ResolveProxyRequestInfo{url::Origin::Create(url),
                                                network_isolation_key};
    Wait();
  }

  bool HasOriginAttemptedToPreconnect(const GURL& origin) {
    DCHECK_EQ(origin, origin.GetOrigin());
    return base::Contains(preconnect_url_attempts_, origin);
  }

  bool HasHostBeenLookedUp(
      const std::string& host,
      const net::NetworkIsolationKey& network_isolation_key) {
    ResolveHostRequestInfo preconnect_info{host, network_isolation_key};
    return base::Contains(successful_dns_lookups_, preconnect_info) ||
           base::Contains(unsuccessful_dns_lookups_, preconnect_info);
  }

  bool HostFound(const std::string& host,
                 const net::NetworkIsolationKey& network_isolation_key) {
    return base::Contains(successful_dns_lookups_,
                          ResolveHostRequestInfo{host, network_isolation_key});
  }

  bool ProxyFound(const GURL& url,
                  const net::NetworkIsolationKey& network_isolation_key) {
    return base::Contains(successful_proxy_lookups_,
                          ResolveProxyRequestInfo{url::Origin::Create(url),
                                                  network_isolation_key});
  }

 private:
  enum class WaitEvent { kNone, kDns, kProxy };

  struct ResolveHostRequestInfo {
    bool operator<(const ResolveHostRequestInfo& other) const {
      return std::tie(hostname, network_isolation_key) <
             std::tie(other.hostname, other.network_isolation_key);
    }

    bool operator==(const ResolveHostRequestInfo& other) const {
      return std::tie(hostname, network_isolation_key) ==
             std::tie(other.hostname, other.network_isolation_key);
    }

    bool IsEmpty() const {
      return hostname.empty() && network_isolation_key.IsEmpty();
    }

    std::string hostname;
    net::NetworkIsolationKey network_isolation_key;
  };

  struct ResolveProxyRequestInfo {
    bool operator<(const ResolveProxyRequestInfo& other) const {
      return std::tie(origin, network_isolation_key) <
             std::tie(other.origin, other.network_isolation_key);
    }

    bool operator==(const ResolveProxyRequestInfo& other) const {
      return std::tie(origin, network_isolation_key) ==
             std::tie(other.origin, other.network_isolation_key);
    }

    bool IsEmpty() const {
      return origin.opaque() && network_isolation_key.IsEmpty();
    }

    url::Origin origin;
    net::NetworkIsolationKey network_isolation_key;
  };

  bool HasProxyBeenLookedUp(const ResolveProxyRequestInfo& resolve_proxy_info) {
    return base::Contains(successful_proxy_lookups_, resolve_proxy_info) ||
           base::Contains(unsuccessful_proxy_lookups_, resolve_proxy_info);
  }

  void Wait() {
    base::RunLoop run_loop;
    DCHECK(!run_loop_);
    run_loop_ = &run_loop;
    CheckForWaitingLoop();
    run_loop.Run();
  }

  void CheckForWaitingLoop() {
    switch (wait_event_) {
      case WaitEvent::kNone:
        return;
      case WaitEvent::kDns:
        if (!HasHostBeenLookedUp(waiting_on_dns_.hostname,
                                 waiting_on_dns_.network_isolation_key)) {
          return;
        }
        waiting_on_dns_ = ResolveHostRequestInfo();
        break;
      case WaitEvent::kProxy:
        if (!HasProxyBeenLookedUp(waiting_on_proxy_))
          return;
        waiting_on_proxy_ = ResolveProxyRequestInfo();
        break;
    }
    DCHECK(run_loop_);
    run_loop_->Quit();
    run_loop_ = nullptr;
    wait_event_ = WaitEvent::kNone;
  }

  WaitEvent wait_event_ = WaitEvent::kNone;
  base::RunLoop* run_loop_ = nullptr;

  ResolveHostRequestInfo waiting_on_dns_;
  std::set<ResolveHostRequestInfo> successful_dns_lookups_;
  std::set<ResolveHostRequestInfo> unsuccessful_dns_lookups_;

  ResolveProxyRequestInfo waiting_on_proxy_;
  std::set<ResolveProxyRequestInfo> successful_proxy_lookups_;
  std::set<ResolveProxyRequestInfo> unsuccessful_proxy_lookups_;

  std::set<GURL> preconnect_url_attempts_;
};

struct PrefetchResult {
  PrefetchResult(const GURL& prefetch_url,
                 const network::URLLoaderCompletionStatus& status)
      : prefetch_url(prefetch_url), status(status) {}

  GURL prefetch_url;
  network::URLLoaderCompletionStatus status;
};

class TestPrefetchManagerObserver : public PrefetchManager::Observer {
 public:
  explicit TestPrefetchManagerObserver(PrefetchManager& manager) {
    manager.set_observer_for_testing(this);
  }

  void OnPrefetchFinished(
      const GURL& url,
      const GURL& prefetch_url,
      const network::URLLoaderCompletionStatus& status) override {
    prefetches_.emplace_back(prefetch_url, status);
  }

  void OnAllPrefetchesFinished(const GURL& url) override {
    done_urls_.insert(url);
    if (waiting_url_ == url) {
      waiting_url_ = GURL();
      std::move(done_callback_).Run();
    }
  }

  void WaitForPrefetchesForNavigation(const GURL& url) {
    DCHECK(waiting_url_.is_empty());
    DCHECK(!url.is_empty());
    if (done_urls_.find(url) != done_urls_.end())
      return;
    waiting_url_ = url;
    base::RunLoop loop;
    done_callback_ = loop.QuitClosure();
    loop.Run();
  }

  const std::vector<PrefetchResult>& results() const { return prefetches_; }

 private:
  std::vector<PrefetchResult> prefetches_;
  std::set<GURL> done_urls_;
  GURL waiting_url_;
  base::OnceClosure done_callback_;
};

class LoadingPredictorBrowserTest : public InProcessBrowserTest {
 public:
  LoadingPredictorBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kLoadingOnlyLearnHighPriorityResources,
         features::kLoadingPreconnectToRedirectTarget,
         features::kNavigationPredictorPreconnectHoldback},
        {});
  }
  ~LoadingPredictorBrowserTest() override {}

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &LoadingPredictorBrowserTest::HandleFaviconRequest));
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &LoadingPredictorBrowserTest::HandleCacheRedirectRequest));

    ASSERT_TRUE(preconnecting_test_server_.InitializeAndListen());
    preconnecting_test_server_.RegisterRequestHandler(base::BindRepeating(
        &LoadingPredictorBrowserTest::HandleFaviconRequest));
    preconnecting_test_server_.AddDefaultHandlers(GetChromeTestDataDir());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    connection_tracker_ = std::make_unique<ConnectionTracker>();
    connection_listener_ =
        std::make_unique<ConnectionListener>(connection_tracker_.get());
    preconnecting_server_connection_tracker_ =
        std::make_unique<ConnectionTracker>();
    preconnecting_server_connection_listener_ =
        std::make_unique<ConnectionListener>(
            preconnecting_server_connection_tracker_.get());

    embedded_test_server()->SetConnectionListener(connection_listener_.get());
    embedded_test_server()->StartAcceptingConnections();

    preconnecting_test_server_.SetConnectionListener(
        preconnecting_server_connection_listener());
    EXPECT_TRUE(preconnecting_test_server_.Started());
    preconnecting_test_server_.StartAcceptingConnections();

    loading_predictor_ =
        LoadingPredictorFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(loading_predictor_);
    preconnect_manager_observer_ =
        std::make_unique<TestPreconnectManagerObserver>(
            loading_predictor_->preconnect_manager());
    if (loading_predictor_->prefetch_manager()) {
      prefetch_manager_observer_ =
          std::make_unique<TestPrefetchManagerObserver>(
              *loading_predictor_->prefetch_manager());
    }
    PredictorInitializer initializer(
        loading_predictor_->resource_prefetch_predictor());
    initializer.EnsurePredictorInitialized();
  }

  // Navigates to an URL without blocking until the navigation finishes.
  // Returns an observer that can be used to wait for the navigation
  // completion.
  // This function creates a new tab for each navigation that allows multiple
  // simultaneous navigations and avoids triggering the reload behavior.
  std::unique_ptr<content::TestNavigationManager> NavigateToURLAsync(
      const GURL& url) {
    chrome::NewTab(browser());
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    DCHECK(tab);
    auto observer = std::make_unique<content::TestNavigationManager>(tab, url);
    tab->GetController().LoadURL(url, content::Referrer(),
                                 ui::PAGE_TRANSITION_TYPED, std::string());
    return observer;
  }

  void ResetNetworkState() {
    auto* network_context = content::BrowserContext::GetDefaultStoragePartition(
                                browser()->profile())
                                ->GetNetworkContext();
    base::RunLoop clear_host_cache_loop;
    base::RunLoop close_all_connections_loop;
    network_context->ClearHostCache(nullptr,
                                    clear_host_cache_loop.QuitClosure());
    network_context->CloseAllConnections(
        close_all_connections_loop.QuitClosure());
    clear_host_cache_loop.Run();
    close_all_connections_loop.Run();

    connection_tracker()->ResetCounts();
    preconnecting_server_connection_tracker_->ResetCounts();
  }

  void ResetPredictorState() {
    loading_predictor_->resource_prefetch_predictor()->DeleteAllUrls();
  }

  std::unique_ptr<PreconnectPrediction> GetPreconnectPrediction(
      const GURL& url) {
    auto prediction = std::make_unique<PreconnectPrediction>();
    bool has_prediction = loading_predictor_->resource_prefetch_predictor()
                              ->PredictPreconnectOrigins(url, prediction.get());
    if (!has_prediction)
      return nullptr;
    return prediction;
  }

  LoadingPredictor* loading_predictor() { return loading_predictor_; }

  TestPreconnectManagerObserver* preconnect_manager_observer() {
    return preconnect_manager_observer_.get();
  }

  TestPrefetchManagerObserver* prefetch_manager_observer() {
    return prefetch_manager_observer_.get();
  }

  ConnectionTracker* connection_tracker() { return connection_tracker_.get(); }

  ConnectionTracker* preconnecting_server_connection_tracker() const {
    return preconnecting_server_connection_tracker_.get();
  }

  ConnectionListener* preconnecting_server_connection_listener() const {
    return preconnecting_server_connection_listener_.get();
  }

  static std::unique_ptr<net::test_server::HttpResponse> HandleFaviconRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/favicon.ico")
      return nullptr;

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->AddCustomHeader("Cache-Control", "max-age=6000");
    return http_response;
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  HandleCacheRedirectRequest(const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, "/cached-redirect?",
                          base::CompareCase::INSENSITIVE_ASCII)) {
      return nullptr;
    }

    GURL request_url = request.GetURL();
    std::string dest =
        net::UnescapeBinaryURLComponent(request_url.query_piece());

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", dest);
    http_response->set_content_type("text/html");
    http_response->set_content(base::StringPrintf(
        "<html><head></head><body>Redirecting to %s</body></html>",
        dest.c_str()));
    http_response->AddCustomHeader("Cache-Control", "max-age=6000");
    return http_response;
  }

 protected:
  // Test server that initiates preconnect. Separate server from the one being
  // preconnected to separate preconnected connection count.
  net::EmbeddedTestServer preconnecting_test_server_;

 private:
  LoadingPredictor* loading_predictor_ = nullptr;
  std::unique_ptr<ConnectionListener> connection_listener_;
  std::unique_ptr<ConnectionTracker> connection_tracker_;
  std::unique_ptr<ConnectionListener> preconnecting_server_connection_listener_;
  std::unique_ptr<ConnectionTracker> preconnecting_server_connection_tracker_;
  std::unique_ptr<TestPreconnectManagerObserver> preconnect_manager_observer_;
  std::unique_ptr<TestPrefetchManagerObserver> prefetch_manager_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(LoadingPredictorBrowserTest);
};

// Tests that a navigation triggers the LoadingPredictor.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, SimpleNavigation) {
  GURL url = embedded_test_server()->GetURL("/nocontent");
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  EXPECT_EQ(1u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  // Checking GetActiveHintsSizeForTesting() is racy since the active hint
  // is removed after the preconnect finishes. Instead check for total
  // hints activated.
  EXPECT_LE(1u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(2u, loading_predictor()->GetTotalHintsActivatedForTesting());
  observer->WaitForNavigationFinished();
  EXPECT_EQ(0u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  EXPECT_EQ(0u, loading_predictor()->GetActiveHintsSizeForTesting());
  EXPECT_LE(1u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(2u, loading_predictor()->GetTotalHintsActivatedForTesting());
}

// Tests that two concurrenct navigations are recorded correctly by the
// predictor.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, TwoConcurrentNavigations) {
  GURL url1 = embedded_test_server()->GetURL("/echo-raw?1");
  GURL url2 = embedded_test_server()->GetURL("/echo-raw?2");
  auto observer1 = NavigateToURLAsync(url1);
  auto observer2 = NavigateToURLAsync(url2);
  EXPECT_TRUE(observer1->WaitForRequestStart());
  EXPECT_TRUE(observer2->WaitForRequestStart());
  EXPECT_EQ(2u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  // Checking GetActiveHintsSizeForTesting() is racy since the active hint
  // is removed after the preconnect finishes. Instead check for total
  // hints activated.
  EXPECT_LE(2u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(4u, loading_predictor()->GetTotalHintsActivatedForTesting());
  observer1->WaitForNavigationFinished();
  observer2->WaitForNavigationFinished();
  EXPECT_EQ(0u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  EXPECT_EQ(0u, loading_predictor()->GetActiveHintsSizeForTesting());
  EXPECT_LE(2u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(4u, loading_predictor()->GetTotalHintsActivatedForTesting());
}

// Tests that two navigations to the same URL are deduplicated.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       TwoNavigationsToTheSameURL) {
  GURL url = embedded_test_server()->GetURL("/nocontent");
  auto observer1 = NavigateToURLAsync(url);
  auto observer2 = NavigateToURLAsync(url);
  EXPECT_TRUE(observer1->WaitForRequestStart());
  EXPECT_TRUE(observer2->WaitForRequestStart());
  EXPECT_EQ(2u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  // Checking GetActiveHintsSizeForTesting() is racy since the active hint
  // is removed after the preconnect finishes. Instead check for total
  // hints activated. The total hints activated may be only 1 if the second
  // navigation arrives before the first preconnect finishes. However, if the
  // second navigation arrives later, then two hints may get activated.
  EXPECT_LE(1u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(4u, loading_predictor()->GetTotalHintsActivatedForTesting());
  observer1->WaitForNavigationFinished();
  observer2->WaitForNavigationFinished();
  EXPECT_EQ(0u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  EXPECT_EQ(0u, loading_predictor()->GetActiveHintsSizeForTesting());
  EXPECT_LE(1u, loading_predictor()->GetTotalHintsActivatedForTesting());
  EXPECT_GE(4u, loading_predictor()->GetTotalHintsActivatedForTesting());
}

// Tests that the LoadingPredictor doesn't record non-http(s) navigations.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, NonHttpNavigation) {
  std::string content = "<body>Hello world!</body>";
  GURL url = GetDataURLWithContent(content);
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  EXPECT_EQ(0u, loading_predictor()->GetActiveNavigationsSizeForTesting());
  EXPECT_EQ(0u, loading_predictor()->GetActiveHintsSizeForTesting());
}

// Tests that the LoadingPredictor doesn't preconnect to non-http(s) urls.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       PrepareForPageLoadNonHttpScheme) {
  std::string content = "<body>Hello world!</body>";
  GURL url = GetDataURLWithContent(content);
  ui_test_utils::NavigateToURL(browser(), url);
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  // Ensure that no backgound task would make a host lookup or attempt to
  // preconnect.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(
      url.host(), network_isolation_key));
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(
      "", network_isolation_key));
  EXPECT_FALSE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
      url.GetOrigin()));
  EXPECT_FALSE(
      preconnect_manager_observer()->HasOriginAttemptedToPreconnect(GURL()));
}

namespace {
class TestPrerenderStopObserver
    : public prerender::NoStatePrefetchHandle::Observer {
 public:
  explicit TestPrerenderStopObserver(base::OnceClosure on_stop_closure)
      : on_stop_closure_(std::move(on_stop_closure)) {}
  ~TestPrerenderStopObserver() override = default;

  void OnPrefetchStop(prerender::NoStatePrefetchHandle* contents) override {
    if (on_stop_closure_) {
      std::move(on_stop_closure_).Run();
    }
  }

  void OnPrefetchNetworkBytesChanged(
      prerender::NoStatePrefetchHandle* handle) override {}

 private:
  base::OnceClosure on_stop_closure_;
};
}  // namespace

// Tests that the LoadingPredictor doesn't preconnect during a prerender.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       PrepareForPageLoadDuringPrerender) {
  GURL url("http://test.com");
  base::RunLoop prerender_run_loop;
  TestPrerenderStopObserver prerender_observer(
      prerender_run_loop.QuitClosure());

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          browser()->profile());

  std::unique_ptr<prerender::NoStatePrefetchHandle> handle =
      no_state_prefetch_manager->AddPrerenderFromNavigationPredictor(
          url,
          browser()
              ->tab_strip_model()
              ->GetActiveWebContents()
              ->GetController()
              .GetDefaultSessionStorageNamespace(),
          gfx::Size(640, 480));
  ASSERT_TRUE(handle);
  handle->SetObserver(&prerender_observer);
  prerender_run_loop.Run();

  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  // Ensure that the prerender does not make a host lookup or attempt to
  // preconnect.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(
      url.host(), network_isolation_key));
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(
      "", network_isolation_key));
  EXPECT_FALSE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
      url.GetOrigin()));
  EXPECT_FALSE(
      preconnect_manager_observer()->HasOriginAttemptedToPreconnect(GURL()));
}

// Tests that the LoadingPredictor preconnects to the main frame origin even if
// it doesn't have any prediction for this origin.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       PrepareForPageLoadWithoutPrediction) {
  // Navigate the first time to fill the HTTP cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();
  ResetPredictorState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  preconnect_manager_observer()->WaitUntilHostLookedUp(url.host(),
                                                       network_isolation_key);
  EXPECT_TRUE(preconnect_manager_observer()->HostFound(url.host(),
                                                       network_isolation_key));
  // We should preconnect only 2 sockets for the main frame host.
  const size_t expected_connections = 2;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

// Tests that the LoadingPredictor has a prediction for a host after navigating
// to it.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, LearnFromNavigation) {
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  url::Origin origin = url::Origin::Create(url);
  std::vector<PreconnectRequest> requests;
  for (auto* const host : kHtmlSubresourcesHosts) {
    requests.emplace_back(
        url::Origin::Create(embedded_test_server()->GetURL(host, "/")), 1,
        net::NetworkIsolationKey(origin, origin));
  }

  ui_test_utils::NavigateToURL(browser(), url);
  auto prediction = GetPreconnectPrediction(url);
  ASSERT_TRUE(prediction);
  EXPECT_EQ(prediction->is_redirected, false);
  EXPECT_EQ(prediction->host, url.host());
  EXPECT_THAT(prediction->requests,
              testing::UnorderedElementsAreArray(requests));
}

class LoadingPredictorBrowserTestLearnAllResources
    : public LoadingPredictorBrowserTest {
 public:
  LoadingPredictorBrowserTestLearnAllResources() {
    feature_list_.InitAndDisableFeature(
        features::kLoadingOnlyLearnHighPriorityResources);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the LoadingPredictor has a prediction for a host after navigating
// to it. Disables kLoadingOnlyLearnHighPriorityResources.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTestLearnAllResources,
                       LearnFromNavigation) {
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  url::Origin origin = url::Origin::Create(url);
  std::vector<PreconnectRequest> requests;
  for (auto* const host : kHtmlSubresourcesHosts) {
    requests.emplace_back(
        url::Origin::Create(embedded_test_server()->GetURL(host, "/")), 1,
        net::NetworkIsolationKey(origin, origin));
  }

  // When kLoadingOnlyLearnHighPriorityResources is disabled, loading data
  // collector should learn the loading of low priority resources hosted on
  // bar.com as well.
  requests.emplace_back(
      url::Origin::Create(embedded_test_server()->GetURL("bar.com", "/")), 1,
      net::NetworkIsolationKey(origin, origin));

  ui_test_utils::NavigateToURL(browser(), url);
  auto prediction = GetPreconnectPrediction(url);
  ASSERT_TRUE(prediction);
  EXPECT_EQ(prediction->is_redirected, false);
  EXPECT_EQ(prediction->host, url.host());
  EXPECT_THAT(prediction->requests,
              testing::UnorderedElementsAreArray(requests));
}

// Tests that the LoadingPredictor correctly learns from navigations with
// redirect.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       LearnFromNavigationWithRedirect) {
  GURL redirect_url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  GURL original_url = embedded_test_server()->GetURL(
      "redirect.com",
      base::StringPrintf("/server-redirect?%s", redirect_url.spec().c_str()));
  url::Origin origin = url::Origin::Create(redirect_url);
  std::vector<PreconnectRequest> expected_requests;
  for (auto* const host : kHtmlSubresourcesHosts) {
    expected_requests.emplace_back(
        url::Origin::Create(embedded_test_server()->GetURL(host, "/")), 1,
        net::NetworkIsolationKey(origin, origin));
  }

  ui_test_utils::NavigateToURL(browser(), original_url);
  // The predictor can correctly predict hosts by |redirect_url|
  auto prediction = GetPreconnectPrediction(redirect_url);
  ASSERT_TRUE(prediction);
  EXPECT_EQ(prediction->is_redirected, false);
  EXPECT_EQ(prediction->host, redirect_url.host());
  EXPECT_THAT(prediction->requests,
              testing::UnorderedElementsAreArray(expected_requests));
  // The predictor needs minimum two redirect hits to be confident in the
  // redirect to generate the origins for subresources. However, after the
  // first redirect, the predictor should learn the redirect origin for
  // preconnect.
  prediction = GetPreconnectPrediction(original_url);
  ASSERT_TRUE(prediction);
  EXPECT_FALSE(prediction->is_redirected);
  EXPECT_EQ(prediction->host, original_url.host());
  std::vector<PreconnectRequest> expected_requests_1;
  url::Origin redirect_origin = url::Origin::Create(
      embedded_test_server()->GetURL(redirect_url.host(), "/"));
  expected_requests_1.emplace_back(redirect_origin, 1,
                                   net::NetworkIsolationKey(origin, origin));
  EXPECT_THAT(prediction->requests,
              testing::UnorderedElementsAreArray(expected_requests_1));

  // The predictor will start predict for origins of subresources (based on
  // redirect) after the second navigation.
  ui_test_utils::NavigateToURL(browser(), original_url);
  prediction = GetPreconnectPrediction(original_url);
  expected_requests.emplace_back(redirect_origin, 1,
                                 net::NetworkIsolationKey(origin, origin));
  ASSERT_TRUE(prediction);
  EXPECT_EQ(prediction->is_redirected, true);
  EXPECT_EQ(prediction->host, redirect_url.host());
  EXPECT_THAT(prediction->requests,
              testing::UnorderedElementsAreArray(expected_requests));
}

// Tests that the LoadingPredictor performs preresolving/preconnecting for a
// navigation which it has a prediction for.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest,
                       PrepareForPageLoadWithPrediction) {
  // Navigate the first time to fill the predictor's database and the HTTP
  // cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  for (auto* const host : kHtmlSubresourcesHosts) {
    GURL url(base::StringPrintf("http://%s", host));
    preconnect_manager_observer()->WaitUntilHostLookedUp(url.host(),
                                                         network_isolation_key);
    EXPECT_TRUE(preconnect_manager_observer()->HostFound(
        url.host(), network_isolation_key));
  }
  // 2 connections to the main frame host + 1 connection per host for others.
  const size_t expected_connections = base::size(kHtmlSubresourcesHosts) + 1;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

// Tests that a host requested by <link rel="dns-prefetch"> is looked up.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, DnsPrefetch) {
  GURL url = embedded_test_server()->GetURL("/predictor/dns_prefetch.html");
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  ui_test_utils::NavigateToURL(browser(), url);
  preconnect_manager_observer()->WaitUntilHostLookedUp(
      GURL(kChromiumUrl).host(), network_isolation_key);
  EXPECT_TRUE(preconnect_manager_observer()->HostFound(
      GURL(kChromiumUrl).host(), network_isolation_key));
}

// Tests that preconnect warms up a socket connection to a test server.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTest, PreconnectNonCors) {
  GURL preconnect_url = embedded_test_server()->base_url();
  std::string preconnect_content =
      "<link rel=\"preconnect\" href=\"" + preconnect_url.spec() + "\">";
  ui_test_utils::NavigateToURL(browser(),
                               GetDataURLWithContent(preconnect_content));
  connection_tracker()->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

enum class NetworkIsolationKeyMode {
  kDisabled,
  kEnabled,
};

class LoadingPredictorNetworkIsolationKeyBrowserTest
    : public LoadingPredictorBrowserTest,
      public testing::WithParamInterface<NetworkIsolationKeyMode> {
 public:
  LoadingPredictorNetworkIsolationKeyBrowserTest() {
    switch (GetParam()) {
      case NetworkIsolationKeyMode::kDisabled:
        scoped_feature_list2_.InitWithFeatures(
            // enabled_features
            {features::kLoadingPreconnectToRedirectTarget},
            // disabled_features
            {net::features::kPartitionConnectionsByNetworkIsolationKey,
             net::features::kSplitCacheByNetworkIsolationKey});
        break;
      case NetworkIsolationKeyMode::kEnabled:
        scoped_feature_list2_.InitWithFeatures(
            // enabled_features
            {net::features::kPartitionConnectionsByNetworkIsolationKey,
             net::features::kSplitCacheByNetworkIsolationKey,
             features::kLoadingPreconnectToRedirectTarget},
            // disabled_features
            {});
        break;
    }
  }

  ~LoadingPredictorNetworkIsolationKeyBrowserTest() override {}

  // One server is used to initiate preconnects, and one is preconnected to.
  // This makes tracking preconnected sockets much easier, and removes all
  // worried about favicon fetches and other sources of preconnects.
  net::EmbeddedTestServer* preconnecting_test_server() {
    return &preconnecting_test_server_;
  }

  // Load the favicon into the cache, so that favicon requests won't create any
  // new connections. Can't just wait for it normally, because there's no easy
  // way to be sure that the favicon request associated with a page load has
  // completed, since it doesn't block navigation complete events.
  void CacheFavIcon() {
    CacheUrl(embedded_test_server()->GetURL("/favicon.ico"));
  }

  // Drives a request for the provided URL to completion, which will then be
  // stored in the HTTP cache, headers permitting.
  void CacheUrl(const GURL& url) {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = url;
    content::SimpleURLLoaderTestHelper simple_loader_helper;
    url::Origin origin = url::Origin::Create(url);
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(origin);
    request->site_for_cookies =
        request->trusted_params->isolation_info.site_for_cookies();
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        browser()->profile()->GetURLLoaderFactory().get(),
        simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();
    ASSERT_TRUE(simple_loader_helper.response_body());
    if (url.IntPort() == embedded_test_server()->port()) {
      EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
      EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
    } else {
      EXPECT_EQ(url.IntPort(), preconnecting_test_server_.port());
      EXPECT_EQ(
          1u,
          preconnecting_server_connection_tracker()->GetAcceptedSocketCount());
      EXPECT_EQ(
          1u, preconnecting_server_connection_tracker()->GetReadSocketCount());
    }
    ResetNetworkState();
  }

  void RunCorsTest(bool use_cors_for_preconnect,
                   bool use_cors_for_resource_request) {
    const char* kCrossOriginValue[]{
        "anonymous",
        "use-credentials",
    };

    ui_test_utils::NavigateToURL(
        browser(), preconnecting_test_server()->GetURL("/title1.html"));

    // Preconnect a socket.
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL preconnect_url = embedded_test_server()->base_url();
    std::string start_preconnect = base::StringPrintf(
        "var link = document.createElement('link');"
        "link.rel = 'preconnect';"
        "link.crossOrigin = '%s';"
        "link.href = '%s';"
        "document.head.appendChild(link);",
        kCrossOriginValue[use_cors_for_preconnect],
        preconnect_url.spec().c_str());
    ASSERT_TRUE(content::ExecJs(tab, start_preconnect));
    connection_tracker()->WaitUntilFirstConnectionAccepted();
    EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
    EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

    // Load an image.
    GURL image_url = embedded_test_server()->GetURL("/test.gif");
    std::string load_image = base::StringPrintf(
        "var image = document.createElement('img');"
        "image.crossOrigin = '%s';"
        "image.src = '%s';"
        "document.body.appendChild(image);",
        kCrossOriginValue[use_cors_for_resource_request],
        image_url.spec().c_str());
    ASSERT_TRUE(content::ExecJs(tab, load_image));
    connection_tracker()->WaitUntilFirstConnectionRead();

    // The preconnected socket should have been used by the image request if
    // the CORS behavior of the preconnect and the request were the same.
    if (use_cors_for_preconnect == use_cors_for_resource_request) {
      EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
      EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
    } else {
      EXPECT_EQ(2u, connection_tracker()->GetAcceptedSocketCount());
      EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list2_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LoadingPredictorNetworkIsolationKeyBrowserTest,
                         ::testing::Values(NetworkIsolationKeyMode::kDisabled,
                                           NetworkIsolationKeyMode::kEnabled));

// Make sure that the right NetworkIsolationKey is used by the LoadingPredictor,
// both when the predictor is populated and when it isn't.
IN_PROC_BROWSER_TEST_P(LoadingPredictorNetworkIsolationKeyBrowserTest,
                       LoadingPredictorNoRedirects) {
  // Cache resources needed by navigations, so the only sockets created
  // during navigations should be for the two preconnects.
  CacheFavIcon();
  GURL cacheable_url = embedded_test_server()->GetURL("/cachetime");
  CacheUrl(cacheable_url);

  // For the first loop iteration, the predictor has no state, and for the
  // second one it does.
  for (bool predictor_has_state : {false, true}) {
    SCOPED_TRACE(predictor_has_state);

    auto observer = NavigateToURLAsync(cacheable_url);
    observer->WaitForNavigationFinished();
    connection_tracker()->WaitForAcceptedConnections(2);
    EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

    // Have the page fetch a subresource, which should use one of the
    // preconnects triggered by the above navigation, due to the matching
    // NetworkIsolationKey. Do this instead of a navigation to a non-cached URL
    // to avoid triggering more preconnects.
    std::string fetch_resource = base::StringPrintf(
        "(async () => {"
        "  var resp = (await fetch('%s'));"
        "  return resp.status; })();",
        embedded_test_server()->GetURL("/echo").spec().c_str());
    EXPECT_EQ(200, EvalJs(browser()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetMainFrame(),
                          fetch_resource));

    EXPECT_EQ(2u, connection_tracker()->GetAcceptedSocketCount());
    EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());

    ResetNetworkState();
  }
}

// Make sure that the right NetworkIsolationKey is used by the LoadingPredictor,
// both when the predictor is populated and when it isn't.
IN_PROC_BROWSER_TEST_P(LoadingPredictorNetworkIsolationKeyBrowserTest,
                       LoadingPredictorWithRedirects) {
  // Cache resources needed by navigations, so the only connections to the
  // tracked server created during navigations should be for preconnects.
  CacheFavIcon();
  GURL cacheable_url = embedded_test_server()->GetURL("/cachetime");
  CacheUrl(cacheable_url);

  GURL redirecting_url = preconnecting_test_server()->GetURL(
      "/server-redirect?" + cacheable_url.spec());

  // Learn the redirects from initial navigation.
  ui_test_utils::NavigateToURL(browser(), redirecting_url);
  EXPECT_EQ(0u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

  // The next navigation should preconnect. It won't use the preconnected
  // socket, since the destination resource is still in the cache.
  auto observer = NavigateToURLAsync(redirecting_url);
  observer->WaitForNavigationFinished();
  connection_tracker()->WaitForAcceptedConnections(1);
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

  // Have the page fetch a subresource, which should use one of the
  // preconnects triggered by the above navigation, due to the matching
  // NetworkIsolationKey. Do this instead of a navigation to a non-cached URL
  // to avoid triggering more preconnects.
  std::string fetch_resource = base::StringPrintf(
      "(async () => {"
      "  var resp = (await fetch('%s'));"
      "  return resp.status; })();",
      embedded_test_server()->GetURL("/echo").spec().c_str());
  EXPECT_EQ(
      200,
      EvalJs(
          browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
          fetch_resource));

  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
}

// Checks the opposite of the above test - tests that even when a redirect is
// predicted, preconnects are still made to the original origin using the
// correct NetworkIsolationKey.
IN_PROC_BROWSER_TEST_P(LoadingPredictorNetworkIsolationKeyBrowserTest,
                       LoadingPredictorWithRedirects2) {
  // Cache the redirect, so the only connections to the tracked server created
  // during navigations should be for preconnects.
  GURL destination_url = preconnecting_test_server()->GetURL("/cachetime");
  GURL redirecting_url = embedded_test_server()->GetURL("/cached-redirect?" +
                                                        destination_url.spec());

  // Unlike other tests, the "preconnecting" server is actually the final
  // destination, so its favicon needs to be cached.
  CacheUrl(preconnecting_test_server()->GetURL("/favicon.ico"));

  CacheUrl(redirecting_url);

  // The first navigation learns to preconnect based on the redirect, and the
  // second actually preconnects to the untracked server. All navigations should
  // preconnect twice to the tracked server.
  for (int i = 0; i < 2; ++i) {
    if (i == 0) {
      // NavigateToURL waits long enough to ensure information from the
      // navigation is learned, while WaitForNavigationFinished() does not.
      ui_test_utils::NavigateToURL(browser(), redirecting_url);
    } else {
      auto observer = NavigateToURLAsync(redirecting_url);
      observer->WaitForNavigationFinished();
    }
    connection_tracker()->WaitForAcceptedConnections(2);
    EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

    // Verify that the redirect from |redirecting_url| to |destination_url| was
    // learned and preconnected to.
    if (i == 1)
      preconnecting_server_connection_tracker()->WaitForAcceptedConnections(1);
    EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

    // Verify that the preconnects to |embedded_test_server| were made using
    // the |redirecting_url|'s NetworkIsolationKey. To do this, make a request
    // using the tracked server's NetworkIsolationKey, and verify it used one
    // of the existing sockets.
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = embedded_test_server()->GetURL("/echo");
    content::SimpleURLLoaderTestHelper simple_loader_helper;
    url::Origin origin = url::Origin::Create(request->url);
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(origin);
    request->site_for_cookies =
        request->trusted_params->isolation_info.site_for_cookies();
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        browser()->profile()->GetURLLoaderFactory().get(),
        simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();
    ASSERT_TRUE(simple_loader_helper.response_body());
    EXPECT_EQ(2u, connection_tracker()->GetAcceptedSocketCount());
    EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());

    ResetNetworkState();
  }
}

IN_PROC_BROWSER_TEST_P(LoadingPredictorNetworkIsolationKeyBrowserTest,
                       LinkRelPreconnectMainFrame) {
  const char kHost1[] = "host1.test";
  const char kHost2[] = "host2.test";
  GURL preconnect_url = embedded_test_server()->GetURL("/echo");

  // Navigate two tabs, one to each host.

  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), preconnecting_test_server()->GetURL(kHost1, "/title1.html"));

  chrome::NewTab(browser());
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), preconnecting_test_server()->GetURL(kHost2, "/title1.html"));

  std::string start_preconnect = base::StringPrintf(
      "var link = document.createElement('link');"
      "link.rel = 'preconnect';"
      "link.crossOrigin = 'anonymous';"
      "link.href = '%s';"
      "document.head.appendChild(link);",
      preconnect_url.spec().c_str());
  ASSERT_TRUE(content::ExecJs(tab1->GetMainFrame(), start_preconnect));
  connection_tracker()->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

  std::string fetch_resource = base::StringPrintf(
      "(async () => {"
      "  var resp = (await fetch('%s',"
      "                          {credentials: 'omit',"
      "                           mode: 'no-cors'}));"
      "  return resp.status; })();",
      preconnect_url.spec().c_str());
  // Fetch a resource from the test server from tab 2, without CORS.
  EXPECT_EQ(0, EvalJs(tab2->GetMainFrame(), fetch_resource));
  if (GetParam() == NetworkIsolationKeyMode::kDisabled) {
    // When not using NetworkIsolationKeys, the preconnected socket from a tab
    // at one site is usable by a request from another site.
    EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
    EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
  } else {
    // When using NetworkIsolationKeys, the preconnected socket cannot be used.
    EXPECT_EQ(2u, connection_tracker()->GetAcceptedSocketCount());
    EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
  }

  // Now try fetching a resource from tab 1.
  EXPECT_EQ(0, EvalJs(tab1->GetMainFrame(), fetch_resource));
  // If the preconnected socket was not used before, it should now be used. If
  // it was used before, a new socket will be used.
  EXPECT_EQ(2u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(2u, connection_tracker()->GetReadSocketCount());
}

IN_PROC_BROWSER_TEST_P(LoadingPredictorNetworkIsolationKeyBrowserTest,
                       LinkRelPreconnectSubFrame) {
  const char kHost1[] = "host1.test";
  const char kHost2[] = "host2.test";
  GURL preconnect_url = embedded_test_server()->GetURL("/echo");

  // Tab 1 has two iframes, one at kHost1, one at kHost2.
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), preconnecting_test_server()->GetURL(
                     kHost1, GetPathWithPortReplacement(
                                 "/predictors/two_iframes.html",
                                 preconnecting_test_server()->port())));
  std::vector<content::RenderFrameHost*> frames = tab1->GetAllFrames();
  ASSERT_EQ(3u, frames.size());
  ASSERT_EQ(kHost1, frames[0]->GetLastCommittedOrigin().host());
  ASSERT_EQ(kHost1, frames[1]->GetLastCommittedOrigin().host());
  ASSERT_EQ(kHost2, frames[2]->GetLastCommittedOrigin().host());

  // Create another tab without an iframe, at kHost2.
  chrome::NewTab(browser());
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), preconnecting_test_server()->GetURL(kHost2, "/title1.html"));

  // Preconnect a socket in the cross-origin iframe.
  std::string start_preconnect = base::StringPrintf(
      "var link = document.createElement('link');"
      "link.rel = 'preconnect';"
      "link.crossOrigin = 'anonymous';"
      "link.href = '%s';"
      "document.head.appendChild(link);",
      preconnect_url.spec().c_str());
  ASSERT_TRUE(content::ExecJs(frames[2], start_preconnect));
  connection_tracker()->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());

  std::string fetch_resource = base::StringPrintf(
      "(async () => {"
      "  var resp = (await fetch('%s',"
      "                          {credentials: 'omit',"
      "                           mode: 'no-cors'}));"
      "  return resp.status; })();",
      preconnect_url.spec().c_str());

  // Fetch a resource from the test server from tab 2 iframe, without CORS.
  EXPECT_EQ(0, EvalJs(tab2->GetMainFrame(), fetch_resource));
  if (GetParam() == NetworkIsolationKeyMode::kDisabled) {
    // When not using NetworkIsolationKeys, the preconnected socket from the
    // iframe from the first tab can be used.
    EXPECT_EQ(1u, connection_tracker()->GetAcceptedSocketCount());
    EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
  } else {
    // Otherwise, the preconnected socket cannot be used.
    EXPECT_EQ(2u, connection_tracker()->GetAcceptedSocketCount());
    EXPECT_EQ(1u, connection_tracker()->GetReadSocketCount());
  }

  // Fetch a resource from the test server from the same-origin iframe, without
  // CORS.
  EXPECT_EQ(0, EvalJs(frames[1], fetch_resource));
  if (GetParam() == NetworkIsolationKeyMode::kDisabled) {
    // When not using NetworkIsolationKeys, a new socket is created and used.
    EXPECT_EQ(2u, connection_tracker()->GetAcceptedSocketCount());
    EXPECT_EQ(2u, connection_tracker()->GetReadSocketCount());
  } else {
    // Otherwise, the preconnected socket cannot be used.
    EXPECT_EQ(3u, connection_tracker()->GetAcceptedSocketCount());
    EXPECT_EQ(2u, connection_tracker()->GetReadSocketCount());
  }

  // Now try fetching a resource the cross-site iframe.
  EXPECT_EQ(0, EvalJs(frames[2], fetch_resource));
  // If the preconnected socket was not used before, it should now be used. If
  // it was used before, a new socket will be used.
  EXPECT_EQ(3u, connection_tracker()->GetAcceptedSocketCount());
  EXPECT_EQ(3u, connection_tracker()->GetReadSocketCount());
}

// Tests that preconnect warms up a non-CORS connection to a test
// server, and that socket is used when fetching a non-CORS resource.
IN_PROC_BROWSER_TEST_P(LoadingPredictorNetworkIsolationKeyBrowserTest,
                       PreconnectNonCorsAndFetchNonCors) {
  RunCorsTest(false /* use_cors_for_preconnect */,
              false /* use_cors_for_resource_request */);
}

// Tests that preconnect warms up a non-CORS connection to a test
// server, but that socket is not used when fetching a CORS resource.
IN_PROC_BROWSER_TEST_P(LoadingPredictorNetworkIsolationKeyBrowserTest,
                       PreconnectNonCorsAndFetchCors) {
  RunCorsTest(false /* use_cors_for_preconnect */,
              true /* use_cors_for_resource_request */);
}

// Tests that preconnect warms up a CORS connection to a test server,
// but that socket is not used when fetching a non-CORS resource.
IN_PROC_BROWSER_TEST_P(LoadingPredictorNetworkIsolationKeyBrowserTest,
                       PreconnectCorsAndFetchNonCors) {
  RunCorsTest(true /* use_cors_for_preconnect */,
              false /* use_cors_for_resource_request */);
}

// Tests that preconnect warms up a CORS connection to a test server,
// that socket is used when fetching a CORS resource.
IN_PROC_BROWSER_TEST_P(LoadingPredictorNetworkIsolationKeyBrowserTest,
                       PreconnectCorsAndFetchCors) {
  RunCorsTest(true /* use_cors_for_preconnect */,
              true /* use_cors_for_resource_request */);
}

class LoadingPredictorBrowserTestWithProxy
    : public LoadingPredictorBrowserTest {
 public:
  void SetUp() override {
    pac_script_server_ = std::make_unique<net::EmbeddedTestServer>();
    pac_script_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(pac_script_server_->InitializeAndListen());
    LoadingPredictorBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    LoadingPredictorBrowserTest::SetUpOnMainThread();
    // This will make all dns requests fail.
    host_resolver()->ClearRules();

    pac_script_server_->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GURL pac_url = pac_script_server_->GetURL(GetPathWithPortReplacement(
        "/predictors/proxy.pac", embedded_test_server()->port()));
    command_line->AppendSwitchASCII(switches::kProxyPacUrl, pac_url.spec());
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> pac_script_server_;
};

IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTestWithProxy,
                       PrepareForPageLoadWithoutPrediction) {
  // Navigate the first time to fill the HTTP cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();
  ResetPredictorState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  preconnect_manager_observer()->WaitUntilProxyLookedUp(url,
                                                        network_isolation_key);
  EXPECT_TRUE(
      preconnect_manager_observer()->ProxyFound(url, network_isolation_key));
  // We should preconnect only 2 sockets for the main frame host.
  const size_t expected_connections = 2;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTestWithProxy,
                       PrepareForPageLoadWithPrediction) {
  // Navigate the first time to fill the predictor's database and the HTTP
  // cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  for (auto* const host : kHtmlSubresourcesHosts) {
    GURL url = embedded_test_server()->GetURL(host, "/");
    preconnect_manager_observer()->WaitUntilProxyLookedUp(
        url, network_isolation_key);
    EXPECT_TRUE(
        preconnect_manager_observer()->ProxyFound(url, network_isolation_key));
  }
  // 2 connections to the main frame host + 1 connection per host for others.
  const size_t expected_connections = base::size(kHtmlSubresourcesHosts) + 1;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

class LoadingPredictorBrowserTestWithOptimizationGuide
    : public ::testing::WithParamInterface<
          std::tuple<bool, bool, bool, std::string>>,
      public LoadingPredictorBrowserTest {
 public:
  LoadingPredictorBrowserTestWithOptimizationGuide() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kLoadingPredictorUseOptimizationGuide,
          {{"use_predictions",
            ShouldUseOptimizationGuidePredictions() ? "true" : "false"},
           {"always_retrieve_predictions", "true"}}},
         {optimization_guide::features::kOptimizationHints, {}}},
        {});

    if (IsLocalPredictionEnabled()) {
      local_predictions_feature_list_.InitAndEnableFeature(
          features::kLoadingPredictorUseLocalPredictions);
    } else {
      local_predictions_feature_list_.InitAndDisableFeature(
          features::kLoadingPredictorUseLocalPredictions);
    }

    if (IsPrefetchEnabled()) {
      prefetch_feature_list_.InitAndEnableFeatureWithParameters(
          features::kLoadingPredictorPrefetch,
          {{"subresource_type", GetSubresourceTypeParam()}});
    } else {
      prefetch_feature_list_.InitAndDisableFeature(
          features::kLoadingPredictorPrefetch);
    }
  }

  bool IsLocalPredictionEnabled() const { return std::get<0>(GetParam()); }

  bool ShouldUseOptimizationGuidePredictions() const {
    return std::get<1>(GetParam());
  }

  bool IsPrefetchEnabled() const { return std::get<2>(GetParam()); }

  std::string GetSubresourceTypeParam() const {
    return std::string(std::get<3>(GetParam()));
  }

  bool ShouldRetrieveOptimizationGuidePredictions() {
    return !IsLocalPredictionEnabled() ||
           features::ShouldAlwaysRetrieveOptimizationGuidePredictions();
  }

  // A predicted subresource.
  struct Subresource {
    explicit Subresource(std::string url)
        : url(url),
          type(optimization_guide::proto::RESOURCE_TYPE_UNKNOWN),
          preconnect_only(false) {}
    Subresource(std::string url, optimization_guide::proto::ResourceType type)
        : url(url), type(type), preconnect_only(false) {}
    Subresource(std::string url,
                optimization_guide::proto::ResourceType type,
                bool preconnect_only)
        : url(url), type(type), preconnect_only(preconnect_only) {}

    std::string url;
    optimization_guide::proto::ResourceType type;
    bool preconnect_only;
  };

  void SetUpOptimizationHint(
      const GURL& url,
      const std::vector<Subresource>& predicted_subresources) {
    auto* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide::proto::LoadingPredictorMetadata
        loading_predictor_metadata;
    for (const auto& subresource : predicted_subresources) {
      auto* added = loading_predictor_metadata.add_subresources();
      added->set_url(subresource.url);
      added->set_resource_type(subresource.type);
      added->set_preconnect_only(subresource.preconnect_only);
    }

    optimization_guide::OptimizationMetadata optimization_metadata;
    optimization_metadata.set_loading_predictor_metadata(
        loading_predictor_metadata);
    optimization_guide_keyed_service->AddHintForTesting(
        url, optimization_guide::proto::LOADING_PREDICTOR,
        optimization_metadata);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList local_predictions_feature_list_;
  base::test::ScopedFeatureList prefetch_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LoadingPredictorBrowserTestWithOptimizationGuide,
    testing::Combine(
        /*IsLocalPredictionEnabled()=*/testing::Bool(),
        /*ShouldUseOptimizationGuidePredictions()=*/testing::Bool(),
        /*IsPrefetchEnabled()=*/testing::Values(false),
        /*GetSubresourceType()=*/testing::Values("")));

IN_PROC_BROWSER_TEST_P(LoadingPredictorBrowserTestWithOptimizationGuide,
                       NavigationHasLocalPredictionNoOptimizationHint) {
  // Navigate the first time to fill the predictor's database and the HTTP
  // cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  for (auto* const host : kHtmlSubresourcesHosts) {
    if (!IsLocalPredictionEnabled() && host != url.host()) {
      // We don't expect local predictions to be preconnected to.
      continue;
    }

    preconnect_manager_observer()->WaitUntilHostLookedUp(host,
                                                         network_isolation_key);
    EXPECT_TRUE(
        preconnect_manager_observer()->HostFound(host, network_isolation_key));
  }
  size_t expected_connections;
  if (IsLocalPredictionEnabled()) {
    // 2 connections to the main frame host  + 1 connection per host for others.
    expected_connections = base::size(kHtmlSubresourcesHosts) + 1;
  } else {
    // There should always be 2 connections to the main frame host.
    expected_connections = 2;
  }
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

IN_PROC_BROWSER_TEST_P(LoadingPredictorBrowserTestWithOptimizationGuide,
                       NavigationWithBothLocalPredictionAndOptimizationHint) {
  // Navigate the first time to fill the predictor's database and the HTTP
  // cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  SetUpOptimizationHint(url, {Subresource("http://subresource.com/1"),
                              Subresource("http://subresource.com/2"),
                              Subresource("http://otherresource.com/2"),
                              Subresource("skipsoverinvalidurl/////")});

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());

  // The initial URL should be preconnected to.
  preconnect_manager_observer()->WaitUntilHostLookedUp(url.host(),
                                                       network_isolation_key);
  EXPECT_TRUE(preconnect_manager_observer()->HostFound(url.host(),
                                                       network_isolation_key));
  EXPECT_TRUE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
      origin.GetURL()));

  // Both subresource hosts should be preconnected to.
  std::vector<std::string> expected_subresource_hosts;
  if (IsLocalPredictionEnabled()) {
    // Should use subresources that were learned.
    expected_subresource_hosts = {"baz.com", "foo.com"};
  } else if (ShouldUseOptimizationGuidePredictions()) {
    // Should use subresources from optimization hint.
    expected_subresource_hosts = {"subresource.com", "otherresource.com"};
  }
  for (const auto& host : expected_subresource_hosts) {
    preconnect_manager_observer()->WaitUntilHostLookedUp(host,
                                                         network_isolation_key);
    EXPECT_TRUE(
        preconnect_manager_observer()->HostFound(host, network_isolation_key));

    GURL expected_origin;
    if (IsLocalPredictionEnabled()) {
      // The locally learned origins are expected to have a port.
      expected_origin = embedded_test_server()->GetURL(host, "/");
    } else {
      // The optimization hints learned origins do not have a port.
      expected_origin = GURL(base::StringPrintf("http://%s", host.c_str()));
    }
    EXPECT_TRUE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
        expected_origin));
  }
}

IN_PROC_BROWSER_TEST_P(LoadingPredictorBrowserTestWithOptimizationGuide,
                       NavigationWithNoLocalPredictionsButHasOptimizationHint) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL("m.hints.com", "/simple.html");
  SetUpOptimizationHint(url, {Subresource("http://subresource.com/1"),
                              Subresource("http://subresource.com/2"),
                              Subresource("http://otherresource.com/2"),
                              Subresource("skipsoverinvalidurl/////")});
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());

  // The initial URL should be preconnected to.
  preconnect_manager_observer()->WaitUntilHostLookedUp(url.host(),
                                                       network_isolation_key);
  EXPECT_TRUE(preconnect_manager_observer()->HostFound(url.host(),
                                                       network_isolation_key));
  EXPECT_TRUE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
      origin.GetURL()));

  for (auto* const host : {"subresource.com", "otherresource.com"}) {
    if (ShouldUseOptimizationGuidePredictions()) {
      // Both subresource hosts should be preconnected to.
      preconnect_manager_observer()->WaitUntilHostLookedUp(
          host, network_isolation_key);
    }
    EXPECT_EQ(
        preconnect_manager_observer()->HostFound(host, network_isolation_key),
        ShouldUseOptimizationGuidePredictions());

    EXPECT_EQ(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
                  GURL(base::StringPrintf("http://%s/", host))),
              ShouldUseOptimizationGuidePredictions());
  }

  EXPECT_TRUE(observer->WaitForResponse());
  observer->ResumeNavigation();
  content::AwaitDocumentOnLoadCompleted(observer->web_contents());
  observer->WaitForNavigationFinished();

  // Navigate to another URL - make sure optimization guide prediction is
  // cleared.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("nohints.com", "/"));

  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningRecall.OptimizationGuide", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningPrecision.OptimizationGuide", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningCount.OptimizationGuide", 2, 1);
}

IN_PROC_BROWSER_TEST_P(
    LoadingPredictorBrowserTestWithOptimizationGuide,
    OptimizationGuidePredictionsNotAppliedForAlreadyCommittedNavigation) {
  GURL url = embedded_test_server()->GetURL("hints.com", "/simple.html");
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  // Navigate to URL with hints but only seed hints after navigation has
  // committed.
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForResponse());
  observer->ResumeNavigation();
  SetUpOptimizationHint(url, {Subresource("http://subresource.com/1"),
                              Subresource("http://subresource.com/2"),
                              Subresource("http://otherresource.com/2"),
                              Subresource("skipsoverinvalidurl/////")});

  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(
      "subresource.com", network_isolation_key));
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(
      "otheresource.com", network_isolation_key));
}

IN_PROC_BROWSER_TEST_P(LoadingPredictorBrowserTestWithOptimizationGuide,
                       OptimizationGuidePredictionsNotAppliedForRedirect) {
  GURL destination_url =
      embedded_test_server()->GetURL("otherhost.com", "/cachetime");
  GURL redirecting_url = embedded_test_server()->GetURL(
      "sometimesredirects.com", "/cached-redirect?" + destination_url.spec());
  SetUpOptimizationHint(destination_url,
                        {Subresource("http://subresource.com/1"),
                         Subresource("http://subresource.com/2"),
                         Subresource("http://otherresource.com/2"),
                         Subresource("skipsoverinvalidurl/////")});

  // Navigate the first time to something on redirecting origin to fill the
  // predictor's database and the HTTP cache.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "sometimesredirects.com",
          GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                     embedded_test_server()->port())));
  ResetNetworkState();

  url::Origin origin = url::Origin::Create(destination_url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  // Navigate to URL with hints but is redirected, hints should not be
  // applied.
  auto observer = NavigateToURLAsync(redirecting_url);
  EXPECT_TRUE(observer->WaitForResponse());
  SetUpOptimizationHint(redirecting_url,
                        {Subresource("http://subresourceredirect.com/1"),
                         Subresource("http://subresourceredirect.com/2"),
                         Subresource("http://otherresourceredirect.com/2"),
                         Subresource("skipsoverinvalidurl/////")});
  observer->ResumeNavigation();

  std::vector<std::string> expected_opt_guide_subresource_hosts = {
      "subresource.com", "otherresource.com"};
  if (ShouldRetrieveOptimizationGuidePredictions() &&
      ShouldUseOptimizationGuidePredictions()) {
    // Should use subresources from optimization hint.
    for (const auto& host : expected_opt_guide_subresource_hosts) {
      preconnect_manager_observer()->WaitUntilHostLookedUp(
          host, network_isolation_key);
      EXPECT_TRUE(preconnect_manager_observer()->HostFound(
          host, network_isolation_key));

      // The origins from optimization hints do not have a port.
      GURL expected_origin =
          GURL(base::StringPrintf("http://%s", host.c_str()));
      EXPECT_TRUE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
          expected_origin));
    }
  } else {
    for (const auto& host : expected_opt_guide_subresource_hosts) {
      EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(
          host, network_isolation_key));
    }
  }
}

class LoadingPredictorBrowserTestWithNoLocalPredictions
    : public LoadingPredictorBrowserTest {
 public:
  LoadingPredictorBrowserTestWithNoLocalPredictions() {
    feature_list_.InitAndDisableFeature(
        features::kLoadingPredictorUseLocalPredictions);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LoadingPredictorBrowserTestWithNoLocalPredictions,
                       ShouldNotActOnLocalPrediction) {
  // Navigate the first time to fill the predictor's database and the HTTP
  // cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  // The initial URL should be preconnected to.
  preconnect_manager_observer()->WaitUntilHostLookedUp(url.host(),
                                                       network_isolation_key);
  EXPECT_TRUE(preconnect_manager_observer()->HostFound(url.host(),
                                                       network_isolation_key));
  EXPECT_TRUE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
      origin.GetURL()));
  // 2 connections to the main frame host.
  const size_t expected_connections = 2;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

// A fixture for testing prefetching with optimization guide hints.
class LoadingPredictorPrefetchBrowserTest
    : public LoadingPredictorBrowserTestWithOptimizationGuide {
 public:
  void SetUp() override {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &LoadingPredictorPrefetchBrowserTest::MonitorRequest,
        base::Unretained(this)));

    LoadingPredictorBrowserTestWithOptimizationGuide::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoadingPredictorBrowserTestWithOptimizationGuide::SetUpCommandLine(
        command_line);
    command_line->AppendSwitch(
        switches::kLoadingPredictorAllowLocalRequestForTesting);
  }

 protected:
  // Sets the requests to expect in WaitForRequests().
  void SetExpectedRequests(base::flat_set<GURL> requests) {
    expected_requests_ = std::move(requests);
  }

  // Returns once all expected requests have been received.
  void WaitForRequests() {
    if (expected_requests_.empty())
      return;
    base::RunLoop loop;
    quit_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    // Monitor only prefetches.
    if (request.headers.find("Purpose") == request.headers.end() ||
        (request.headers.at("Purpose") != "prefetch")) {
      return;
    }

    // |request.GetURL()| gives us the URL after it's already resolved to
    // 127.0.0.1, so reconstruct the requested host via the Host header
    // (which includes host+port).
    GURL url = request.GetURL();
    auto host_iter = request.headers.find("Host");
    if (host_iter != request.headers.end())
      url = GURL("http://" + host_iter->second + request.relative_url);

    // Remove the expected request.
    auto it = expected_requests_.find(url);
    ASSERT_TRUE(it != expected_requests_.end())
        << "Got unexpected request: " << url;
    expected_requests_.erase(it);

    // Finish if done.
    if (expected_requests_.empty() && quit_)
      std::move(quit_).Run();
  }

  base::flat_set<GURL> expected_requests_;
  base::OnceClosure quit_;
};

// Tests that the LoadingPredictor performs prefetching
// for a navigation which it has a prediction for and there isn't a local
// prediction available.
IN_PROC_BROWSER_TEST_P(LoadingPredictorPrefetchBrowserTest,
                       DISABLED_PrepareForPageLoadWithPredictionForPrefetchNoLocalHint) {
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));

  // Set up optimization hints.
  std::vector<Subresource> hints = {
      {"skipsoverinvalidurl/////",
       optimization_guide::proto::RESOURCE_TYPE_CSS},
      {embedded_test_server()->GetURL("subresource.com", "/css").spec(),
       optimization_guide::proto::RESOURCE_TYPE_CSS},
      {embedded_test_server()->GetURL("subresource.com", "/image").spec(),
       optimization_guide::proto::RESOURCE_TYPE_UNKNOWN},
      {embedded_test_server()->GetURL("otherresource.com", "/js").spec(),
       optimization_guide::proto::RESOURCE_TYPE_SCRIPT},
      {embedded_test_server()->GetURL("preconnect.com", "/other").spec(),
       optimization_guide::proto::RESOURCE_TYPE_UNKNOWN, true},
  };
  SetUpOptimizationHint(url, hints);

  // Expect these prefetches.
  std::vector<GURL> requests;
  if (GetSubresourceTypeParam() == "all") {
    requests = {embedded_test_server()->GetURL("subresource.com", "/css"),
                embedded_test_server()->GetURL("subresource.com", "/image"),
                embedded_test_server()->GetURL("otherresource.com", "/js")};
  } else if (GetSubresourceTypeParam() == "css") {
    requests = {embedded_test_server()->GetURL("subresource.com", "/css")};
  } else if (GetSubresourceTypeParam() == "js_css") {
    requests = {embedded_test_server()->GetURL("subresource.com", "/css"),
                embedded_test_server()->GetURL("otherresource.com", "/js")};
  }
  SetExpectedRequests(std::move(requests));

  // Start a navigation and observe these prefetches.
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  WaitForRequests();

  // preconnect.com should be preconnected to.
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  preconnect_manager_observer()->WaitUntilHostLookedUp("preconnect.com",
                                                       network_isolation_key);
  EXPECT_TRUE(preconnect_manager_observer()->HostFound("preconnect.com",
                                                       network_isolation_key));
  EXPECT_TRUE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
      embedded_test_server()->GetURL("preconnect.com", "/")));
}

// Tests that the LoadingPredictor performs prefetching
// for a navigation which it has a prediction for and there is a local
// prediction available.
IN_PROC_BROWSER_TEST_P(
    LoadingPredictorPrefetchBrowserTest,
    PrepareForPageLoadWithPredictionForPrefetchHasLocalHint) {
  // Navigate the first time to fill the predictor's database and the HTTP
  // cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  // Set up optimization hints.
  std::vector<Subresource> hints = {
      {"skipsoverinvalidurl/////",
       optimization_guide::proto::RESOURCE_TYPE_CSS},
      {embedded_test_server()->GetURL("subresource.com", "/css").spec(),
       optimization_guide::proto::RESOURCE_TYPE_CSS},
      {embedded_test_server()->GetURL("subresource.com", "/image").spec(),
       optimization_guide::proto::RESOURCE_TYPE_UNKNOWN},
      {embedded_test_server()->GetURL("otherresource.com", "/js").spec(),
       optimization_guide::proto::RESOURCE_TYPE_SCRIPT},
      {embedded_test_server()->GetURL("preconnect.com", "/other").spec(),
       optimization_guide::proto::RESOURCE_TYPE_UNKNOWN, true},
  };
  SetUpOptimizationHint(url, hints);

  // Expect these prefetches.
  std::vector<GURL> requests;
  if (GetSubresourceTypeParam() == "all") {
    requests = {embedded_test_server()->GetURL("subresource.com", "/css"),
                embedded_test_server()->GetURL("subresource.com", "/image"),
                embedded_test_server()->GetURL("otherresource.com", "/js")};
  } else if (GetSubresourceTypeParam() == "css") {
    requests = {embedded_test_server()->GetURL("subresource.com", "/css")};
  } else if (GetSubresourceTypeParam() == "js_css") {
    requests = {embedded_test_server()->GetURL("subresource.com", "/css"),
                embedded_test_server()->GetURL("otherresource.com", "/js")};
  }
  SetExpectedRequests(std::move(requests));

  // Start a navigation and observe these prefetches.
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  WaitForRequests();

  std::vector<std::string> expected_subresource_hosts;
  if (IsLocalPredictionEnabled()) {
    // Should use subresources that were learned.
    expected_subresource_hosts = {"baz.com", "foo.com"};
  } else {
    // Should use subresources from optimization hint.
    expected_subresource_hosts = {"preconnect.com"};
  }
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  for (const auto& host : expected_subresource_hosts) {
    preconnect_manager_observer()->WaitUntilHostLookedUp(host,
                                                         network_isolation_key);
    EXPECT_TRUE(
        preconnect_manager_observer()->HostFound(host, network_isolation_key));
    EXPECT_TRUE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
        embedded_test_server()->GetURL(host, "/")));
  }
}

// A fixture for testing prefetching with the local resource check not bypassed.
// The normal fixture bypasses the check so that the embedded test server can be
// used.
class LoadingPredictorPrefetchBrowserTestWithBlockedLocalRequest
    : public LoadingPredictorPrefetchBrowserTest {
 public:
  LoadingPredictorPrefetchBrowserTestWithBlockedLocalRequest() = default;

  // Override to prevent adding kLoadingPredictorAllowLocalRequestForTesting
  // here.
  void SetUpCommandLine(base::CommandLine* command_line) override {}
};

// Test that prefetches to local resources are blocked.
// Disabled for being flaky. crbug.com/1116599
IN_PROC_BROWSER_TEST_P(
    LoadingPredictorPrefetchBrowserTestWithBlockedLocalRequest,
    DISABLED_PrepareForPageLoadWithPredictionForPrefetch) {
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));

  GURL hint_url = embedded_test_server()->GetURL("subresource.com", "/css");

  // Set up one optimization hint.
  std::vector<Subresource> hints = {
      {hint_url.spec(), optimization_guide::proto::RESOURCE_TYPE_CSS},
  };
  SetUpOptimizationHint(url, hints);

  // Start a navigation which triggers prefetch.
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());

  // The prefetch should have failed.
  prefetch_manager_observer()->WaitForPrefetchesForNavigation(url);
  auto results = prefetch_manager_observer()->results();
  ASSERT_THAT(results, SizeIs(1));

  const auto& status = results[0].status;
  EXPECT_EQ(status.error_code, net::ERR_FAILED);
  EXPECT_THAT(status.cors_error_status,
              Optional(network::CorsErrorStatus(
                  network::mojom::IPAddressSpace::kLocal)));
}

// This fixture is for disabling prefetching via test suite instantiation to
// test the counterfactual arm (|always_retrieve_predictions| is
// true but using the predictions is disabled).
class LoadingPredictorPrefetchCounterfactualBrowserTest
    : public LoadingPredictorPrefetchBrowserTest {};

IN_PROC_BROWSER_TEST_P(
    LoadingPredictorPrefetchCounterfactualBrowserTest,
    PrepareForPageLoadWithPredictionForPrefetchHasLocalHint) {
  // Assert that this tests the counterfactual arm.
  ASSERT_TRUE(features::ShouldAlwaysRetrieveOptimizationGuidePredictions());
  ASSERT_FALSE(features::ShouldUseOptimizationGuidePredictions());

  // Navigate the first time to fill the predictor's database and the HTTP
  // cache.
  GURL url = embedded_test_server()->GetURL(
      "test.com", GetPathWithPortReplacement(kHtmlSubresourcesPath,
                                             embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  // Set up optimization hints.
  std::vector<Subresource> hints = {
      {"skipsoverinvalidurl/////",
       optimization_guide::proto::RESOURCE_TYPE_CSS},
      {embedded_test_server()->GetURL("subresource.com", "/css").spec(),
       optimization_guide::proto::RESOURCE_TYPE_CSS},
      {embedded_test_server()->GetURL("subresource.com", "/image").spec(),
       optimization_guide::proto::RESOURCE_TYPE_UNKNOWN},
      {embedded_test_server()->GetURL("otherresource.com", "/js").spec(),
       optimization_guide::proto::RESOURCE_TYPE_SCRIPT},
      {embedded_test_server()->GetURL("preconnect.com", "/other").spec(),
       optimization_guide::proto::RESOURCE_TYPE_UNKNOWN, true},
  };
  SetUpOptimizationHint(url, hints);

  // Expect no prefetches. The test will fail if any prefetch requests are
  // issued.
  SetExpectedRequests({});

  // Start a navigation.
  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());

  std::vector<std::string> expected_subresource_hosts;
  if (IsLocalPredictionEnabled()) {
    // Should use subresources that were learned.
    expected_subresource_hosts = {"baz.com", "foo.com"};
  } else {
    // Should not use subresources from optimization hint since
    // use_predictions is disabled.
  }
  url::Origin origin = url::Origin::Create(url);
  net::NetworkIsolationKey network_isolation_key(origin, origin);
  for (const auto& host : expected_subresource_hosts) {
    preconnect_manager_observer()->WaitUntilHostLookedUp(host,
                                                         network_isolation_key);
    EXPECT_TRUE(
        preconnect_manager_observer()->HostFound(host, network_isolation_key));
    EXPECT_TRUE(preconnect_manager_observer()->HasOriginAttemptedToPreconnect(
        embedded_test_server()->GetURL(host, "/")));
  }

  // Run the run loop to give the test a chance to fail by issuing a prefetch.
  // We don't have an explicit signal for the prefetch manager *not* starting.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(prefetch_manager_observer()->results().empty());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    LoadingPredictorPrefetchBrowserTest,
    testing::Combine(
        /*IsLocalPredictionEnabled()=*/testing::Values(true, false),
        /*ShouldUseOptimizationGuidePredictions()=*/
        testing::Values(true),
        /*IsPrefetchEnabled()=*/testing::Values(true),
        /*GetSubresourceType()=*/testing::Values("all", "css", "js_css")));

// For the "BlockedLocalRequest" test, the params largely don't matter. We just
// need to enable prefetching and test one configuration, since the test passes
// if the prefetch is blocked.
INSTANTIATE_TEST_SUITE_P(
    ,
    LoadingPredictorPrefetchBrowserTestWithBlockedLocalRequest,
    testing::Combine(
        /*IsLocalPredictionEnabled()=*/testing::Values(false),
        /*ShouldUseOptimizationGuidePredictions()=*/
        testing::Values(true),
        /*IsPrefetchEnabled()=*/testing::Values(true),
        /*GetSubresourceType()=*/testing::Values("all")));

// For the "prefetch counterfactual" test, we want to retrieve the optimization
// guide hints but not use them, so set ShouldUseOptimizationGuidePredictions()
// to false. It doesn't matter if IsPrefetchEnabled() is true or not, since
// prefetching only uses optimization guide predictions.
INSTANTIATE_TEST_SUITE_P(
    ,
    LoadingPredictorPrefetchCounterfactualBrowserTest,
    testing::Combine(
        /*IsLocalPredictionEnabled()=*/testing::Values(true, false),
        /*ShouldUseOptimizationGuidePredictions()=*/
        testing::Values(false),
        /*IsPrefetchEnabled()=*/testing::Values(true),
        /*GetSubresourceType()=*/testing::Values("all")));

}  // namespace predictors
