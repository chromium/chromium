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
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
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
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserThread;

namespace predictors {

const char kChromiumUrl[] = "http://chromium.org";
const char kInvalidLongUrl[] =
    "http://"
    "illegally-long-hostname-over-255-characters-should-not-send-an-ipc-"
    "message-to-the-browser-"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000.org";

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
  void AcceptedSocket(const net::StreamSocket& connection) override {
    uint16_t port = GetPort(connection);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConnectionTracker::AcceptedSocketWithPort,
                                  base::Unretained(tracker_), port));
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
      PreconnectManager* preconnect_manager_) {
    preconnect_manager_->SetObserverForTesting(this);
  }

  void OnPreconnectUrl(const GURL& url,
                       int num_sockets,
                       bool allow_credentials) override {
    preconnect_url_attempts_.insert(url.GetOrigin());
  }

  void OnPreresolveFinished(const GURL& url, bool success) override {
    if (success)
      successful_dns_lookups_.insert(url.host());
    else
      unsuccessful_dns_lookups_.insert(url.host());
    CheckForWaitingLoop();
  }

  void OnProxyLookupFinished(const GURL& url, bool success) override {
    GURL origin = url.GetOrigin();
    if (success)
      successful_proxy_lookups_.insert(origin);
    else
      unsuccessful_proxy_lookups_.insert(origin);
    CheckForWaitingLoop();
  }

  void WaitUntilHostLookedUp(const std::string& host) {
    wait_event_ = WaitEvent::kDns;
    DCHECK(waiting_on_dns_.empty());
    waiting_on_dns_ = host;
    Wait();
  }

  void WaitUntilProxyLookedUp(const GURL& url) {
    wait_event_ = WaitEvent::kProxy;
    DCHECK(waiting_on_proxy_.is_empty());
    waiting_on_proxy_ = url;
    Wait();
  }

  bool HasOriginAttemptedToPreconnect(const GURL& origin) {
    DCHECK_EQ(origin, origin.GetOrigin());
    return base::Contains(preconnect_url_attempts_, origin);
  }

  bool HasHostBeenLookedUp(const std::string& host) {
    return base::Contains(successful_dns_lookups_, host) ||
           base::Contains(unsuccessful_dns_lookups_, host);
  }

  bool HostFound(const std::string& host) {
    return base::Contains(successful_dns_lookups_, host);
  }

  bool HasProxyBeenLookedUp(const GURL& url) {
    return base::Contains(successful_proxy_lookups_, url.GetOrigin()) ||
           base::Contains(unsuccessful_proxy_lookups_, url.GetOrigin());
  }

  bool ProxyFound(const GURL& url) {
    return base::Contains(successful_proxy_lookups_, url.GetOrigin());
  }

 private:
  enum class WaitEvent { kNone, kDns, kProxy };

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
        if (!HasHostBeenLookedUp(waiting_on_dns_))
          return;
        waiting_on_dns_ = std::string();
        break;
      case WaitEvent::kProxy:
        if (!HasProxyBeenLookedUp(waiting_on_proxy_))
          return;
        waiting_on_proxy_ = GURL();
        break;
    }
    DCHECK(run_loop_);
    run_loop_->Quit();
    run_loop_ = nullptr;
    wait_event_ = WaitEvent::kNone;
  }

  WaitEvent wait_event_ = WaitEvent::kNone;
  base::RunLoop* run_loop_ = nullptr;

  std::string waiting_on_dns_;
  std::set<std::string> successful_dns_lookups_;
  std::set<std::string> unsuccessful_dns_lookups_;

  GURL waiting_on_proxy_;
  std::set<GURL> successful_proxy_lookups_;
  std::set<GURL> unsuccessful_proxy_lookups_;

  std::set<GURL> preconnect_url_attempts_;
};

class LoadingPredictorBrowserTest : public InProcessBrowserTest {
 public:
  LoadingPredictorBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kLoadingOnlyLearnHighPriorityResources,
         features::kLoadingPreconnectToRedirectTarget},
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
      return std::unique_ptr<net::test_server::HttpResponse>();

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
      return std::unique_ptr<net::test_server::HttpResponse>();
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
  // Ensure that no backgound task would make a host lookup or attempt to
  // preconnect.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(url.host()));
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(""));
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
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();
  ResetPredictorState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  preconnect_manager_observer()->WaitUntilHostLookedUp(url.host());
  EXPECT_TRUE(preconnect_manager_observer()->HostFound(url.host()));
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
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  for (auto* const host : kHtmlSubresourcesHosts) {
    GURL url(base::StringPrintf("http://%s", host));
    preconnect_manager_observer()->WaitUntilHostLookedUp(url.host());
    EXPECT_TRUE(preconnect_manager_observer()->HostFound(url.host()));
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
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                              "/predictor/dns_prefetch.html"));
  preconnect_manager_observer()->WaitUntilHostLookedUp(
      GURL(kChromiumUrl).host());
  EXPECT_FALSE(preconnect_manager_observer()->HasHostBeenLookedUp(
      GURL(kInvalidLongUrl).host()));
  EXPECT_TRUE(
      preconnect_manager_observer()->HostFound(GURL(kChromiumUrl).host()));
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
  kNone,
  kTopFrameOrigin,
  kTopFrameAndFrameOrigins,
};

class LoadingPredictorNetworkIsolationKeyBrowserTest
    : public LoadingPredictorBrowserTest,
      public testing::WithParamInterface<NetworkIsolationKeyMode> {
 public:
  LoadingPredictorNetworkIsolationKeyBrowserTest() {
    switch (GetParam()) {
      case NetworkIsolationKeyMode::kNone:
        scoped_feature_list2_.InitWithFeatures(
            // enabled_features
            {features::kLoadingPreconnectToRedirectTarget},
            // disabled_features
            {net::features::kPartitionConnectionsByNetworkIsolationKey,
             net::features::kSplitCacheByNetworkIsolationKey,
             net::features::kAppendFrameOriginToNetworkIsolationKey});
        break;
      case NetworkIsolationKeyMode::kTopFrameOrigin:
        scoped_feature_list2_.InitWithFeatures(
            // enabled_features
            {net::features::kPartitionConnectionsByNetworkIsolationKey,
             // While these tests are focusing on partitioning the socket pools,
             // some depend on cache behavior, and it would be
             // unfortunate if splitting the cache by the key as well broke
             // them.
             net::features::kSplitCacheByNetworkIsolationKey,
             features::kLoadingPreconnectToRedirectTarget},
            // disabled_features
            {net::features::kAppendFrameOriginToNetworkIsolationKey});
        break;
      case NetworkIsolationKeyMode::kTopFrameAndFrameOrigins:
        scoped_feature_list2_.InitWithFeatures(
            // enabled_features
            {net::features::kPartitionConnectionsByNetworkIsolationKey,
             net::features::kSplitCacheByNetworkIsolationKey,
             net::features::kAppendFrameOriginToNetworkIsolationKey,
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
    request->trusted_params->network_isolation_key =
        net::NetworkIsolationKey(origin, origin);
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

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    LoadingPredictorNetworkIsolationKeyBrowserTest,
    ::testing::Values(NetworkIsolationKeyMode::kNone,
                      NetworkIsolationKeyMode::kTopFrameOrigin,
                      NetworkIsolationKeyMode::kTopFrameAndFrameOrigins));

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
    request->trusted_params->network_isolation_key =
        net::NetworkIsolationKey(origin, origin);
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
  if (GetParam() == NetworkIsolationKeyMode::kNone) {
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
  if (GetParam() == NetworkIsolationKeyMode::kNone) {
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
  if (GetParam() != NetworkIsolationKeyMode::kTopFrameAndFrameOrigins) {
    // When not using NetworkIsolationKeys, a new socket is created and used.
    //
    // When using the origin of the main frame, the preconnected socket from the
    // cross-origin iframe can be used, since only the top frame origin matters.
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
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();
  ResetPredictorState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  preconnect_manager_observer()->WaitUntilProxyLookedUp(url);
  EXPECT_TRUE(preconnect_manager_observer()->ProxyFound(url));
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
  ui_test_utils::NavigateToURL(browser(), url);
  ResetNetworkState();

  auto observer = NavigateToURLAsync(url);
  EXPECT_TRUE(observer->WaitForRequestStart());
  for (auto* const host : kHtmlSubresourcesHosts) {
    GURL url = embedded_test_server()->GetURL(host, "/");
    preconnect_manager_observer()->WaitUntilProxyLookedUp(url);
    EXPECT_TRUE(preconnect_manager_observer()->ProxyFound(url));
  }
  // 2 connections to the main frame host + 1 connection per host for others.
  const size_t expected_connections = base::size(kHtmlSubresourcesHosts) + 1;
  connection_tracker()->WaitForAcceptedConnections(expected_connections);
  EXPECT_EQ(expected_connections,
            connection_tracker()->GetAcceptedSocketCount());
  // No reads since all resources should be cached.
  EXPECT_EQ(0u, connection_tracker()->GetReadSocketCount());
}

}  // namespace predictors
