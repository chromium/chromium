// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/filename_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/reporting/reporting_policy.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/test/test_extension_dir.h"
#endif

namespace {

const char kCacheRandomPath[] = "/cacherandom";

// Path using a ControllableHttpResponse that's part of the test fixture.
const char kControllablePath[] = "/controllable";

enum class NetworkServiceState {
  kEnabled,
  // Similar to |kEnabled|, but will simulate a crash and run tests again the
  // restarted Network Service process.
  kRestarted,
};

enum class NetworkContextType {
  kSystem,
  kSafeBrowsing,
  kProfile,
  kIncognitoProfile,
  kOnDiskApp,
  kInMemoryApp,
  kOnDiskAppWithIncognitoProfile,
};

// This list should be kept in sync with the NetworkContextType enum.
const NetworkContextType kNetworkContextTypes[] = {
    NetworkContextType::kSystem,
    NetworkContextType::kSafeBrowsing,
    NetworkContextType::kProfile,
    NetworkContextType::kIncognitoProfile,
    NetworkContextType::kOnDiskApp,
    NetworkContextType::kInMemoryApp,
    NetworkContextType::kOnDiskAppWithIncognitoProfile,
};

struct TestCase {
  NetworkServiceState network_service_state;
  NetworkContextType network_context_type;
};

// Waits for the network connection type to be the specified value.
class ConnectionTypeWaiter
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  ConnectionTypeWaiter() : tracker_(content::GetNetworkConnectionTracker()) {
    tracker_->AddNetworkConnectionObserver(this);
  }

  ~ConnectionTypeWaiter() override {
    tracker_->RemoveNetworkConnectionObserver(this);
  }

  void Wait(network::mojom::ConnectionType expected_type) {
    auto current_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    network::NetworkConnectionTracker::ConnectionTypeCallback callback =
        base::BindOnce(&ConnectionTypeWaiter::OnConnectionChanged,
                       base::Unretained(this));
    while (!tracker_->GetConnectionType(&current_type, std::move(callback)) ||
           current_type != expected_type) {
      run_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
      run_loop_->Run();
    }
  }

 private:
  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    if (run_loop_)
      run_loop_->Quit();
  }

  network::NetworkConnectionTracker* tracker_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Tests the system, profile, and incognito profile NetworkContexts.
class NetworkContextConfigurationBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<TestCase> {
 public:
  // Backing storage types that can used for various things (HTTP cache,
  // cookies, etc).
  enum class StorageType {
    kNone,
    kMemory,
    kDisk,
  };

  enum class CookieType {
    kFirstParty,
    kThirdParty,
  };

  enum class CookiePersistenceType {
    kSession,
    kPersistent,
  };

  NetworkContextConfigurationBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Have to get a port before setting up the command line, but can only set
    // up the connection listener after there's a main thread, so can't start
    // the test server here.
    EXPECT_TRUE(embedded_test_server()->InitializeAndListen());
    EXPECT_TRUE(https_server()->InitializeAndListen());
  }

  // Returns a cacheable response (10 hours) that is some random text.
  static std::unique_ptr<net::test_server::HttpResponse> HandleCacheRandom(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != kCacheRandomPath)
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(base::GenerateGUID());
    response->set_content_type("text/plain");
    response->AddCustomHeader("Cache-Control", "max-age=60000");
    return std::move(response);
  }

  ~NetworkContextConfigurationBrowserTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    // Used in a bunch of proxy tests. Should not resolve.
    host_resolver()->AddSimulatedFailure("does.not.resolve.test");

    controllable_http_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), kControllablePath);
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &NetworkContextConfigurationBrowserTest::HandleCacheRandom));
    embedded_test_server()->StartAcceptingConnections();
    net::test_server::RegisterDefaultHandlers(https_server());
    https_server()->StartAcceptingConnections();

    if (is_incognito())
      incognito_ = CreateIncognitoBrowser();
    SimulateNetworkServiceCrashIfNecessary();

#if defined(OS_CHROMEOS)
    // On ChromeOS the connection type comes from a fake Shill service, which
    // is configured with a fake ethernet connection asynchronously. Wait for
    // the connection type to be available to avoid getting notified of the
    // connection change halfway through the test.
    ConnectionTypeWaiter().Wait(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
#endif
  }

  // Returns true if the NetworkContext being tested is associated with an
  // incognito profile.
  bool is_incognito() const {
    return GetParam().network_context_type ==
               NetworkContextType::kIncognitoProfile ||
           GetParam().network_context_type ==
               NetworkContextType::kOnDiskAppWithIncognitoProfile;
  }

  void TearDownOnMainThread() override {
    // Have to destroy this before the main message loop is torn down. Need to
    // leave the embedded test server up for tests that use
    // |live_during_shutdown_simple_loader_|. It's safe to destroy the
    // ControllableHttpResponse before the test server.
    controllable_http_response_.reset();
  }

  // Returns, as a string, a PAC script that will use the EmbeddedTestServer as
  // a proxy.
  std::string GetPacScript() const {
    return base::StringPrintf(
        "function FindProxyForURL(url, host){ return 'PROXY %s;' }",
        net::HostPortPair::FromURL(embedded_test_server()->base_url())
            .ToString()
            .c_str());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  content::StoragePartition* GetStoragePartition() {
    return GetStoragePartitionForContextType(GetParam().network_context_type);
  }

  content::StoragePartition* GetStoragePartitionForContextType(
      NetworkContextType network_context_type) {
    const GURL kOnDiskUrl("chrome-guest://foo/persist");
    const GURL kInMemoryUrl("chrome-guest://foo/");
    switch (network_context_type) {
      case NetworkContextType::kSystem:
      case NetworkContextType::kSafeBrowsing:
        NOTREACHED() << "Network context has no storage partition";
        return nullptr;
      case NetworkContextType::kProfile:
        return content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
      case NetworkContextType::kIncognitoProfile:
        DCHECK(incognito_);
        return content::BrowserContext::GetDefaultStoragePartition(
            incognito_->profile());
      case NetworkContextType::kOnDiskApp:
        return content::BrowserContext::GetStoragePartitionForSite(
            browser()->profile(), kOnDiskUrl);
      case NetworkContextType::kInMemoryApp:
        return content::BrowserContext::GetStoragePartitionForSite(
            browser()->profile(), kInMemoryUrl);
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        DCHECK(incognito_);
        return content::BrowserContext::GetStoragePartitionForSite(
            incognito_->profile(), kOnDiskUrl);
    }
    NOTREACHED();
    return nullptr;
  }

  network::mojom::URLLoaderFactory* loader_factory() {
    return GetLoaderFactoryForContextType(GetParam().network_context_type);
  }

  network::mojom::URLLoaderFactory* GetLoaderFactoryForContextType(
      NetworkContextType network_context_type) {
    switch (network_context_type) {
      case NetworkContextType::kSystem:
        return g_browser_process->system_network_context_manager()
            ->GetURLLoaderFactory();
      case NetworkContextType::kSafeBrowsing:
        return g_browser_process->safe_browsing_service()
            ->GetURLLoaderFactory()
            .get();
      case NetworkContextType::kProfile:
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kOnDiskApp:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        return GetStoragePartitionForContextType(network_context_type)
            ->GetURLLoaderFactoryForBrowserProcess()
            .get();
    }
    NOTREACHED();
    return nullptr;
  }

  network::mojom::NetworkContext* network_context() {
    return GetNetworkContextForContextType(GetParam().network_context_type);
  }

  network::mojom::NetworkContext* GetNetworkContextForContextType(
      NetworkContextType network_context_type) {
    switch (network_context_type) {
      case NetworkContextType::kSystem:
        return g_browser_process->system_network_context_manager()
            ->GetContext();
      case NetworkContextType::kSafeBrowsing:
        return g_browser_process->safe_browsing_service()->GetNetworkContext();
      case NetworkContextType::kProfile:
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kOnDiskApp:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        return GetStoragePartitionForContextType(network_context_type)
            ->GetNetworkContext();
    }
    NOTREACHED();
    return nullptr;
  }

  StorageType GetHttpCacheType() const {
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
      case NetworkContextType::kSafeBrowsing:
        return StorageType::kNone;
      case NetworkContextType::kProfile:
      case NetworkContextType::kOnDiskApp:
        return StorageType::kDisk;
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        return StorageType::kMemory;
    }
    NOTREACHED();
    return StorageType::kNone;
  }

  StorageType GetCookieStorageType() const {
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        return StorageType::kMemory;
      case NetworkContextType::kSafeBrowsing:
      case NetworkContextType::kProfile:
      case NetworkContextType::kOnDiskApp:
        return StorageType::kDisk;
    }
    NOTREACHED();
    return StorageType::kNone;
  }

  // Returns the pref service with most prefs related to the NetworkContext
  // being tested.
  PrefService* GetPrefService() {
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
      case NetworkContextType::kSafeBrowsing:
        return g_browser_process->local_state();
      case NetworkContextType::kProfile:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskApp:
        return browser()->profile()->GetPrefs();
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        // Incognito actually uses the non-incognito prefs, so this should end
        // up being the same pref store as in the KProfile case.
        return browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
    }
  }

  // Sets the proxy preference on a PrefService based on the NetworkContextType,
  // and waits for it to be applied.
  void SetProxyPref(const net::HostPortPair& host_port_pair) {
    GetPrefService()->Set(proxy_config::prefs::kProxy,
                          ProxyConfigDictionary::CreateFixedServers(
                              host_port_pair.ToString(), std::string()));

    // Wait for the new ProxyConfig to be passed over the pipe. Needed because
    // Mojo doesn't guarantee ordering of events on different Mojo pipes, and
    // requests are sent on a separate pipe from ProxyConfigs.
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
      case NetworkContextType::kSafeBrowsing:
        g_browser_process->system_network_context_manager()
            ->FlushProxyConfigMonitorForTesting();
        break;
      case NetworkContextType::kProfile:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskApp:
        ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
            ->FlushProxyConfigMonitorForTesting();
        break;
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        ProfileNetworkContextServiceFactory::GetForContext(
            browser()->profile()->GetOffTheRecordProfile())
            ->FlushProxyConfigMonitorForTesting();
        break;
    }
  }

  // Sends a request and expects it to be handled by embedded_test_server()
  // acting as a proxy;
  void TestProxyConfigured(bool expect_success) {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    // This URL should be directed to the test server because of the proxy.
    request->url = GURL("http://does.not.resolve.test:1872/echo");

    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);

    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory(), simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();

    if (expect_success) {
      EXPECT_EQ(net::OK, simple_loader->NetError());
      ASSERT_TRUE(simple_loader_helper.response_body());
      EXPECT_EQ(*simple_loader_helper.response_body(), "Echo");
    } else {
      EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, simple_loader->NetError());
      ASSERT_FALSE(simple_loader_helper.response_body());
    }
  }

  // Makes a request that hangs, and will live until browser shutdown.
  void MakeLongLivedRequestThatHangsUntilShutdown() {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = embedded_test_server()->GetURL(kControllablePath);
    live_during_shutdown_simple_loader_ = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
    live_during_shutdown_simple_loader_helper_ =
        std::make_unique<content::SimpleURLLoaderTestHelper>();

    live_during_shutdown_simple_loader_
        ->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
            loader_factory(),
            live_during_shutdown_simple_loader_helper_->GetCallback());

    // Don't actually care about controlling the response, just need to wait
    // until it sees the request, to make sure that a URLRequest has been
    // created to potentially leak. Since the |controllable_http_response_| is
    // not used to send a response to the request, the request just hangs until
    // the NetworkContext is destroyed (Or the test server is shut down, but the
    // NetworkContext should be destroyed before that happens, in this test).
    controllable_http_response_->WaitForRequest();
  }

  // Sends a request to the test server's echoheader URL and sets
  // |header_value_out| to the value of the specified response header. Returns
  // false if the request fails. If non-null, uses |request| to make the
  // request, after setting its |url| value.
  bool FetchHeaderEcho(const std::string& header_name,
                       std::string* header_value_out,
                       std::unique_ptr<network::ResourceRequest> request =
                           nullptr) WARN_UNUSED_RESULT {
    if (!request)
      request = std::make_unique<network::ResourceRequest>();
    request->url = embedded_test_server()->GetURL(
        base::StrCat({"/echoheader?", header_name}));
    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory(), simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();
    if (simple_loader_helper.response_body()) {
      *header_value_out = *simple_loader_helper.response_body();
      return true;
    }
    return false;
  }

  // |server| should be |TYPE_HTTPS| if the cookie is third-party, because
  // SameSite=None cookies must be Secure.
  void SetCookie(CookieType cookie_type,
                 CookiePersistenceType cookie_expiration_type,
                 net::EmbeddedTestServer* server) {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    std::string cookie_line = "cookie";
    if (cookie_expiration_type == CookiePersistenceType::kPersistent)
      cookie_line += ";max-age=3600";
    // Third party (i.e. SameSite=None) cookies must be secure.
    if (cookie_type == CookieType::kThirdParty)
      cookie_line += ";SameSite=None;Secure";
    request->url = server->GetURL("/set-cookie?" + cookie_line);
    if (cookie_type == CookieType::kThirdParty)
      request->site_for_cookies = GURL("http://example.com");
    else
      request->site_for_cookies = server->base_url();

    url::Origin origin = url::Origin::Create(request->site_for_cookies);
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->network_isolation_key =
        net::NetworkIsolationKey(origin, origin);

    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);

    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory(), simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();
    EXPECT_EQ(net::OK, simple_loader->NetError());
  }

  void FlushNetworkInterface() {
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
        g_browser_process->system_network_context_manager()
            ->FlushNetworkInterfaceForTesting();
        break;
      case NetworkContextType::kSafeBrowsing:
        g_browser_process->safe_browsing_service()
            ->FlushNetworkInterfaceForTesting();
        break;
      case NetworkContextType::kProfile:
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskApp:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        GetStoragePartition()->FlushNetworkInterfaceForTesting();
        break;
    }
  }

  Profile* GetProfile() {
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
      case NetworkContextType::kSafeBrowsing:
      case NetworkContextType::kProfile:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskApp:
        return browser()->profile();
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        DCHECK(incognito_);
        return incognito_->profile();
    }
  }

  // Gets all cookies for given URL, as a single string.
  std::string GetCookies(const GURL& url) {
    return GetCookiesForContextType(GetParam().network_context_type, url);
  }

  std::string GetCookiesForContextType(NetworkContextType network_context_type,
                                       const GURL& url) {
    std::string cookies;
    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    GetNetworkContextForContextType(network_context_type)
        ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
    cookie_manager->GetCookieList(
        url, net::CookieOptions::MakeAllInclusive(),
        base::BindOnce(
            [](std::string* cookies_out, base::RunLoop* run_loop,
               const net::CookieStatusList& cookies,
               const net::CookieStatusList& excluded_cookies) {
              *cookies_out = net::CanonicalCookie::BuildCookieLine(cookies);
              run_loop->Quit();
            },
            &cookies, &run_loop));
    run_loop.Run();
    return cookies;
  }

  void ForEachOtherContext(
      base::RepeatingCallback<void(NetworkContextType network_context_type)>
          callback) {
    // Create Incognito Profile if needed.
    if (!incognito_)
      incognito_ = CreateIncognitoBrowser();

    // True if the |network_context_type| corresponding to GetParam() has been
    // found. Used to verify that kNetworkContextTypes is kept up to date, and
    // contains no duplicates.
    bool found_matching_type = false;
    for (const auto network_context_type : kNetworkContextTypes) {
      if (network_context_type == GetParam().network_context_type) {
        EXPECT_FALSE(found_matching_type);
        found_matching_type = true;
        continue;
      }
      callback.Run(network_context_type);
    }
    EXPECT_TRUE(found_matching_type);
  }

  bool IsRestartStateWithInProcessNetworkService() {
    return GetParam().network_service_state ==
               NetworkServiceState::kRestarted &&
           content::IsInProcessNetworkService();
  }

  void UpdateChromePolicy(const policy::PolicyMap& policy_map) {
    provider_.UpdateChromePolicy(policy_map);
  }

  enum class WayToEnableSSLConfig { kViaPrefs, kViaPolicy };

  // This helper function enables the kSSLVersionMin pref and tests that this
  // pref is respected. kSSLVersionMin can be set in two ways: over prefs
  // directly or over a policy. |way_to_enable| is used to determine the way to
  // set the pref.
  void TestEnablingSSLVersionMin(WayToEnableSSLConfig way_to_enable) {
    // Start a TLS 1.0 server.
    net::EmbeddedTestServer ssl_server(net::EmbeddedTestServer::TYPE_HTTPS);
    net::SSLServerConfig ssl_config;
    ssl_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1;
    ssl_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1;
    ssl_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    ssl_server.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(ssl_server.Start());

    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = ssl_server.GetURL("/echo");
    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory(), simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();
    ASSERT_TRUE(simple_loader_helper.response_body());
    EXPECT_EQ(*simple_loader_helper.response_body(), "Echo");

    if (way_to_enable == WayToEnableSSLConfig::kViaPrefs) {
      // Disallow TLS 1.0 via prefs.
      g_browser_process->local_state()->SetString(prefs::kSSLVersionMin,
                                                  switches::kSSLVersionTLSv11);
    } else {
      // Disallow TLS 1.0 via policy.
      policy::PolicyMap values;
      values.Set(policy::key::kSSLVersionMin, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(switches::kSSLVersionTLSv11),
                 nullptr);
      base::RunLoop run_loop;
      PrefChangeRegistrar pref_change_registrar;
      pref_change_registrar.Init(g_browser_process->local_state());
      pref_change_registrar.Add(prefs::kSSLVersionMin, run_loop.QuitClosure());
      UpdateChromePolicy(values);
      run_loop.Run();
    }

    g_browser_process->system_network_context_manager()
        ->FlushSSLConfigManagerForTesting();

    // With the new prefs, requests to the server should be blocked.
    request = std::make_unique<network::ResourceRequest>();
    request->url = ssl_server.GetURL("/echo");
    content::SimpleURLLoaderTestHelper simple_loader_helper2;
    simple_loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory(), simple_loader_helper2.GetCallback());
    simple_loader_helper2.WaitForCallback();
    EXPECT_FALSE(simple_loader_helper2.response_body());
    EXPECT_EQ(net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH,
              simple_loader->NetError());
  }

 private:
  void SimulateNetworkServiceCrashIfNecessary() {
    if (GetParam().network_service_state != NetworkServiceState::kRestarted ||
        content::IsInProcessNetworkService()) {
      return;
    }

    // Make sure |network_context()| is working as expected. Use '/echoheader'
    // instead of '/echo' to avoid a disk_cache bug.
    // See https://crbug.com/792255.
    int net_error = content::LoadBasicRequest(
        network_context(), embedded_test_server()->GetURL("/echoheader"), 0, 0,
        net::LOAD_BYPASS_PROXY);
    EXPECT_THAT(net_error, net::test::IsOk());

    // Crash the NetworkService process. Existing interfaces should receive
    // error notifications at some point.
    SimulateNetworkServiceCrash();
    // Flush the interface to make sure the error notification was received.
    FlushNetworkInterface();
  }

  Browser* incognito_ = nullptr;
  base::test::ScopedFeatureList feature_list_;

  net::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_http_response_;

  policy::MockConfigurationPolicyProvider provider_;
  // Used in tests that need a live request during browser shutdown.
  std::unique_ptr<network::SimpleURLLoader> live_during_shutdown_simple_loader_;
  std::unique_ptr<content::SimpleURLLoaderTestHelper>
      live_during_shutdown_simple_loader_helper_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       SecureCookiesAllowedForChromeScheme) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // TODO(crbug.com/1007320): This does not work under SameSiteByDefaultCookies,
  // (and also does not work for SameSite cookies, in general). This is not
  // easily fixable, so disable the test for now.
  if (net::cookie_util::IsSameSiteByDefaultCookiesEnabled())
    return;
  // Cookies are only allowed for chrome:// schemes requesting a secure origin,
  // so create an HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::RegisterDefaultHandlers(&https_server);
  ASSERT_TRUE(https_server.Start());
  if (GetPrefService()->FindPreference(prefs::kBlockThirdPartyCookies))
    GetPrefService()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  SetCookie(CookieType::kFirstParty, CookiePersistenceType::kPersistent,
            embedded_test_server());

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = https_server.GetURL("/echoheader?Cookie");
  request->site_for_cookies = GURL(chrome::kChromeUIPrintURL);
  url::Origin origin = url::Origin::Create(request->site_for_cookies);
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->network_isolation_key =
      net::NetworkIsolationKey(origin, origin);
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  EXPECT_EQ(200, simple_loader->ResponseInfo()->headers->response_code());
  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_EQ("cookie", *simple_loader_helper.response_body());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::unique_ptr<net::test_server::HttpResponse> EchoCookieHeader(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  auto it = request.headers.find("Cookie");
  if (it != request.headers.end())
    response->set_content(it->second);
  response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  return response;
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       ThirdPartyCookiesAllowedForExtensions) {
  if (IsRestartStateWithInProcessNetworkService())
    return;

  // Loading an extension only makes sense for profile contexts.
  if (GetParam().network_context_type != NetworkContextType::kProfile &&
      GetParam().network_context_type !=
          NetworkContextType::kIncognitoProfile) {
    return;
  }
  net::EmbeddedTestServer test_server;
  net::test_server::RegisterDefaultHandlers(&test_server);
  test_server.RegisterRequestHandler(base::BindRepeating(&EchoCookieHeader));
  ASSERT_TRUE(test_server.Start());

  GetPrefService()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  SetCookie(CookieType::kFirstParty, CookiePersistenceType::kPersistent,
            embedded_test_server());

  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "Cookie Test",
    "manifest_version": 2,
    "version": "1.0",
    "background": {
      "scripts": ["background.js"]
    },
    "incognito": "split",
    "permissions": ["<all_urls>"]
   })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  extensions::ChromeTestExtensionLoader loader(GetProfile());
  loader.set_allow_incognito_access(true);
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // This request will show up as cross-site because the chrome-extension URL
  // won't match the test_server domain (127.0.0.1), but because we set
  // |attach_same_site_cookies| to true for extension-initiated requests, this
  // will actually be able to get the cookie.
  GURL url = test_server.GetURL("/echocookieheader");
  std::string script = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = () => domAutomationController.send(xhr.responseText);
    xhr.send();
  }))";
  std::string result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          GetProfile(), extension->id(), script + "('" + url.spec() + "')");
  EXPECT_EQ("cookie", result);
}
#endif

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, BasicRequest) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/echo");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  ASSERT_TRUE(simple_loader->ResponseInfo());
  ASSERT_TRUE(simple_loader->ResponseInfo()->headers);
  EXPECT_EQ(200, simple_loader->ResponseInfo()->headers->response_code());
  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_EQ("Echo", *simple_loader_helper.response_body());
}

// Make sure a cache is used when expected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, Cache) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // Crashing the network service may corrupt the disk cache, so skip this test
  // in tests that crash the network service and use an on-disk cache.
  if (GetParam().network_service_state == NetworkServiceState::kRestarted &&
      GetHttpCacheType() == StorageType::kDisk) {
    return;
  }

  // Make a request whose response should be cached.
  GURL request_url = embedded_test_server()->GetURL("/cachetime");
  url::Origin request_origin =
      url::Origin::Create(embedded_test_server()->base_url());
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = request_url;
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->network_isolation_key =
      net::NetworkIsolationKey(request_origin, request_origin);

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_GT(simple_loader_helper.response_body()->size(), 0u);

  // Stop the server.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Make the request again, and make sure it's cached or not, according to
  // expectations. Reuse the content::ResourceRequest, but nothing else.
  std::unique_ptr<network::ResourceRequest> request2 =
      std::make_unique<network::ResourceRequest>();
  request2->url = request_url;
  request2->trusted_params = network::ResourceRequest::TrustedParams();
  request2->trusted_params->network_isolation_key =
      net::NetworkIsolationKey(request_origin, request_origin);

  content::SimpleURLLoaderTestHelper simple_loader_helper2;
  std::unique_ptr<network::SimpleURLLoader> simple_loader2 =
      network::SimpleURLLoader::Create(std::move(request2),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader2->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper2.GetCallback());
  simple_loader_helper2.WaitForCallback();
  if (GetHttpCacheType() == StorageType::kNone) {
    // If there's no cache, and no server running, the request should have
    // failed.
    EXPECT_FALSE(simple_loader_helper2.response_body());
    EXPECT_EQ(net::ERR_CONNECTION_REFUSED, simple_loader2->NetError());
  } else {
    // Otherwise, the request should have succeeded, and returned the same
    // result as before.
    ASSERT_TRUE(simple_loader_helper2.response_body());
    EXPECT_EQ(*simple_loader_helper.response_body(),
              *simple_loader_helper2.response_body());
  }
}

// Make sure that NetworkContexts can't access each other's disk caches.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, CacheIsolation) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // Make a request whose response should be cached.
  GURL request_url = embedded_test_server()->GetURL("/cachetime");
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = request_url;
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_GT(simple_loader_helper.response_body()->size(), 0u);

  // Stop the server.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Make the request again, for each other network context, and make sure the
  // response is not cached.
  ForEachOtherContext(
      base::BindLambdaForTesting([&](NetworkContextType network_context_type) {
        std::unique_ptr<network::ResourceRequest> request2 =
            std::make_unique<network::ResourceRequest>();
        request2->url = request_url;
        content::SimpleURLLoaderTestHelper simple_loader_helper2;
        std::unique_ptr<network::SimpleURLLoader> simple_loader2 =
            network::SimpleURLLoader::Create(std::move(request2),
                                             TRAFFIC_ANNOTATION_FOR_TESTS);
        simple_loader2->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
            GetLoaderFactoryForContextType(network_context_type),
            simple_loader_helper2.GetCallback());
        simple_loader_helper2.WaitForCallback();
        EXPECT_FALSE(simple_loader_helper2.response_body());
        EXPECT_EQ(net::ERR_CONNECTION_REFUSED, simple_loader2->NetError());
      }));
}

// Make sure an on-disk cache is used when expected. PRE_DiskCache populates the
// cache. DiskCache then makes sure the cache entry is still there (Or not) as
// expected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, PRE_DiskCache) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // Save test URL to disk, so it can be used in the next test (Test server uses
  // a random port, so need to know the port to try and retrieve it from the
  // cache in the next test). The profile directory is preserved between the
  // PRE_DiskCache and DiskCache run, so can just keep a file there.
  GURL test_url = embedded_test_server()->GetURL(kCacheRandomPath);
  url::Origin test_origin = url::Origin::Create(test_url);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath save_url_file_path = browser()->profile()->GetPath().Append(
      FILE_PATH_LITERAL("url_for_test.txt"));

  // Make a request whose response should be cached.
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = test_url;
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->network_isolation_key =
      net::NetworkIsolationKey(test_origin, test_origin);

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  EXPECT_EQ(net::OK, simple_loader->NetError());
  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_FALSE(simple_loader_helper.response_body()->empty());

  // Write the URL and expected response to a file.
  std::string file_data =
      test_url.spec() + "\n" + *simple_loader_helper.response_body();
  ASSERT_EQ(
      static_cast<int>(file_data.length()),
      base::WriteFile(save_url_file_path, file_data.data(), file_data.size()));

  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

// Check if the URL loaded in PRE_DiskCache is still in the cache, across a
// browser restart.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, DiskCache) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // Crashing the network service may corrupt the disk cache, so skip this phase
  // in tests that crash the network service and use an on-disk cache.
  if (GetParam().network_service_state == NetworkServiceState::kRestarted &&
      GetHttpCacheType() == StorageType::kDisk) {
    return;
  }

  // Load URL from the above test body to disk.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath save_url_file_path = browser()->profile()->GetPath().Append(
      FILE_PATH_LITERAL("url_for_test.txt"));
  std::string file_data;
  ASSERT_TRUE(ReadFileToString(save_url_file_path, &file_data));

  size_t newline_pos = file_data.find('\n');
  ASSERT_NE(newline_pos, std::string::npos);

  GURL test_url = GURL(file_data.substr(0, newline_pos));
  ASSERT_TRUE(test_url.is_valid()) << test_url.possibly_invalid_spec();
  url::Origin test_origin = url::Origin::Create(test_url);

  std::string original_response = file_data.substr(newline_pos + 1);

  // Request the same test URL as may have been cached by PRE_DiskCache.
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = test_url;
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->network_isolation_key =
      net::NetworkIsolationKey(test_origin, test_origin);

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  request->load_flags = net::LOAD_ONLY_FROM_CACHE;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  // The request should only succeed if there is an on-disk cache.
  if (GetHttpCacheType() != StorageType::kDisk) {
    EXPECT_FALSE(simple_loader_helper.response_body());
  } else {
    DCHECK_NE(GetParam().network_service_state,
              NetworkServiceState::kRestarted);
    ASSERT_TRUE(simple_loader_helper.response_body());
    EXPECT_EQ(original_response, *simple_loader_helper.response_body());
  }
}

// Visits a URL with an HSTS header, and makes sure it is respected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, PRE_Hsts) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  net::test_server::EmbeddedTestServer ssl_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  ssl_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  ssl_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(ssl_server.Start());

  // Make a request whose response has an STS header.
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = ssl_server.GetURL(
      "/set-header?Strict-Transport-Security: max-age%3D600000");

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  EXPECT_EQ(net::OK, simple_loader->NetError());
  ASSERT_TRUE(simple_loader->ResponseInfo()->headers);
  EXPECT_TRUE(simple_loader->ResponseInfo()->headers->HasHeaderValue(
      "Strict-Transport-Security", "max-age=600000"));

  // Make a cache-only request to make sure the HSTS entry is respected. Using a
  // cache-only request both prevents the result from being cached, and makes
  // the check identical to the one in the next test, which is run after the
  // test server has been shut down.
  GURL exected_ssl_url = ssl_server.base_url();
  GURL::Replacements replacements;
  replacements.SetSchemeStr("http");
  GURL start_url = exected_ssl_url.ReplaceComponents(replacements);

  request = std::make_unique<network::ResourceRequest>();
  request->url = start_url;
  request->load_flags = net::LOAD_ONLY_FROM_CACHE;

  content::SimpleURLLoaderTestHelper simple_loader_helper2;
  simple_loader = network::SimpleURLLoader::Create(
      std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper2.GetCallback());
  simple_loader_helper2.WaitForCallback();
  EXPECT_FALSE(simple_loader_helper2.response_body());
  EXPECT_EQ(exected_ssl_url, simple_loader->GetFinalURL());

  // Write the URL with HSTS information to a file, so it can be loaded in the
  // next test. Have to use a file for this, since the server's port is random.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath save_url_file_path = browser()->profile()->GetPath().Append(
      FILE_PATH_LITERAL("url_for_test.txt"));
  std::string file_data = start_url.spec();
  ASSERT_EQ(
      static_cast<int>(file_data.length()),
      base::WriteFile(save_url_file_path, file_data.data(), file_data.size()));
}

// Checks if the HSTS information from the last test is still available after a
// restart.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, Hsts) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // The network service must be cleanly shut down to guarantee HSTS information
  // is flushed to disk, but that currently generally doesn't happen. See
  // https://crbug.com/820996.
  if (GetHttpCacheType() == StorageType::kDisk) {
    return;
  }
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath save_url_file_path = browser()->profile()->GetPath().Append(
      FILE_PATH_LITERAL("url_for_test.txt"));
  std::string file_data;
  ASSERT_TRUE(ReadFileToString(save_url_file_path, &file_data));
  GURL start_url = GURL(file_data);

  // Unfortunately, loading HSTS information is loaded asynchronously from
  // disk, so there's no way to guarantee it has loaded by the time a
  // request is made. As a result, may have to try multiple times to verify that
  // cached HSTS information has been loaded, when it's stored on disk.
  while (true) {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = start_url;
    request->load_flags = net::LOAD_ONLY_FROM_CACHE;

    content::SimpleURLLoaderTestHelper simple_loader_helper;
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory(), simple_loader_helper.GetCallback());
    simple_loader_helper.WaitForCallback();
    EXPECT_FALSE(simple_loader_helper.response_body());

    // HSTS information is currently stored on disk if-and-only-if there's an
    // on-disk HTTP cache.
    if (GetHttpCacheType() == StorageType::kDisk) {
      GURL::Replacements replacements;
      replacements.SetSchemeStr("https");
      GURL expected_https_url = start_url.ReplaceComponents(replacements);
      // The file may not have been loaded yet, so if the cached HSTS
      // information was not respected, try again.
      if (expected_https_url != simple_loader->GetFinalURL())
        continue;
      EXPECT_EQ(expected_https_url, simple_loader->GetFinalURL());
      break;
    } else {
      EXPECT_EQ(start_url, simple_loader->GetFinalURL());
      break;
    }
  }
}

// Check that the SSLConfig is hooked up. PRE_SSLConfig checks that changing
// local_state() after start modifies the SSLConfig, SSLConfig makes sure the
// (now modified) initial value of local_state() is respected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, PRE_SSLConfig) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  TestEnablingSSLVersionMin(WayToEnableSSLConfig::kViaPrefs);
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, SSLConfig) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // Start a TLS 1.0 server.
  net::EmbeddedTestServer ssl_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1;
  ssl_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1;
  ssl_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  ASSERT_TRUE(ssl_server.Start());

  // Making a request should fail, since PRE_SSLConfig saved a pref to disallow
  // TLS 1.0.
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = ssl_server.GetURL("/echo");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  EXPECT_FALSE(simple_loader_helper.response_body());
  EXPECT_EQ(net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH, simple_loader->NetError());
}

// This test does the same as
// 'NetworkContextConfigurationBrowserTest.PRE_SSLConfig' but with the
// difference that the SSLVersionMin is set via a policy (not via the prefs).
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       SSLVersionMinSetViaPolicy) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  TestEnablingSSLVersionMin(WayToEnableSSLConfig::kViaPolicy);
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, ProxyConfig) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  SetProxyPref(embedded_test_server()->host_port_pair());
  TestProxyConfigured(/*expect_success=*/true);
}

// This test should not end in an AssertNoURLLRequests CHECK.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       ShutdownWithLiveRequest) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  MakeLongLivedRequestThatHangsUntilShutdown();
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       UserAgentAndLanguagePrefs) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // The system and SafeBrowsing network contexts aren't associated with any
  // profile, so changing the language settings for the profile's main network
  // context won't affect what they send.
  bool system =
      (GetParam().network_context_type == NetworkContextType::kSystem ||
       GetParam().network_context_type == NetworkContextType::kSafeBrowsing);
  // echoheader returns "None" when the header isn't there in the first place.
  const char kNoAcceptLanguage[] = "None";

  std::string accept_language, user_agent;
  // Check default.
  ASSERT_TRUE(FetchHeaderEcho("accept-language", &accept_language));
  EXPECT_EQ(system ? kNoAcceptLanguage : "en-US,en;q=0.9", accept_language);
  ASSERT_TRUE(FetchHeaderEcho("user-agent", &user_agent));
  EXPECT_EQ(::GetUserAgent(), user_agent);

  // Now change the profile a different language, and see if the headers
  // get updated.
  browser()->profile()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                              "uk");
  FlushNetworkInterface();
  std::string accept_language2, user_agent2;
  ASSERT_TRUE(FetchHeaderEcho("accept-language", &accept_language2));
  EXPECT_EQ(system ? kNoAcceptLanguage : "uk", accept_language2);
  ASSERT_TRUE(FetchHeaderEcho("user-agent", &user_agent2));
  EXPECT_EQ(::GetUserAgent(), user_agent2);

  // Try a more complicated one, with multiple languages.
  browser()->profile()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                              "uk, en_US");
  FlushNetworkInterface();
  std::string accept_language3, user_agent3;
  ASSERT_TRUE(FetchHeaderEcho("accept-language", &accept_language3));
  EXPECT_EQ(system ? kNoAcceptLanguage : "uk,en_US;q=0.9", accept_language3);
  ASSERT_TRUE(FetchHeaderEcho("user-agent", &user_agent3));
  EXPECT_EQ(::GetUserAgent(), user_agent3);
}

// First part of testing enable referrers. Check that referrers are enabled by
// default at browser start, and referrers are indeed sent. Then disable
// referrers, and make sure that they aren't set.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       PRE_EnableReferrers) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  const GURL kReferrer("http://referrer/");

  // Referrers should be enabled by default.
  EXPECT_TRUE(GetPrefService()->GetBoolean(prefs::kEnableReferrers));

  // Send a request, make sure the referrer is sent.
  std::string referrer;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->referrer = kReferrer;
  ASSERT_TRUE(FetchHeaderEcho("referer", &referrer, std::move(request)));

  // SafeBrowsing never sends the referrer since it doesn't need to.
  if (GetParam().network_context_type == NetworkContextType::kSafeBrowsing) {
    EXPECT_EQ("None", referrer);
  } else {
    EXPECT_EQ(kReferrer.spec(), referrer);
  }

  // Disable referrers, and flush the NetworkContext mojo interface it's set on,
  // to avoid any races with the URLLoaderFactory pipe.
  GetPrefService()->SetBoolean(prefs::kEnableReferrers, false);
  FlushNetworkInterface();

  // Send a request and make sure its referer is not sent.
  std::string referrer2;
  std::unique_ptr<network::ResourceRequest> request2 =
      std::make_unique<network::ResourceRequest>();
  request2->referrer = kReferrer;
  ASSERT_TRUE(FetchHeaderEcho("referer", &referrer2, std::move(request2)));
  EXPECT_EQ("None", referrer2);
}

// Second part of enable referrer test. Referrer should still be disabled. Make
// sure that disable referrers option is respected after startup, as to just
// after changing it.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       EnableReferrers) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  const GURL kReferrer("http://referrer/");

  // The preference is expected to be reset in incognito mode.
  if (is_incognito()) {
    EXPECT_TRUE(GetPrefService()->GetBoolean(prefs::kEnableReferrers));
    return;
  }

  // Referrers should still be disabled.
  EXPECT_FALSE(GetPrefService()->GetBoolean(prefs::kEnableReferrers));

  // Send a request and make sure its referer is not sent.
  std::string referrer;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->referrer = kReferrer;
  ASSERT_TRUE(FetchHeaderEcho("referer", &referrer, std::move(request)));
  EXPECT_EQ("None", referrer);
}

// Make sure that sending referrers that violate the referrer policy results in
// errors.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       PolicyViolatingReferrers) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/echoheader?Referer");
  request->referrer = GURL("http://referrer/");
  request->referrer_policy = net::URLRequest::NO_REFERRER;
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  if (GetParam().network_context_type == NetworkContextType::kSafeBrowsing) {
    // Safebrowsing ignores referrers so the requests succeed.
    EXPECT_EQ(net::OK, simple_loader->NetError());
    ASSERT_TRUE(simple_loader_helper.response_body());
    EXPECT_EQ("None", *simple_loader_helper.response_body());
  } else {
    // In all other cases, the invalid referrer causes the request to fail.
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, simple_loader->NetError());
  }
}

// Makes sure cookies are enabled by default, and saved to disk / not saved to
// disk as expected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       PRE_CookiesEnabled) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  EXPECT_TRUE(GetCookies(embedded_test_server()->base_url()).empty());

  SetCookie(CookieType::kFirstParty, CookiePersistenceType::kPersistent,
            embedded_test_server());
  EXPECT_FALSE(GetCookies(embedded_test_server()->base_url()).empty());
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, CookiesEnabled) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
#if defined(OS_MACOSX)
  // TODO(https://crbug.com/880496): Fix and reenable test.
  if (base::mac::IsOS10_11())
    return;
#endif
  // Check that the cookie from the first stage of the test was / was not
  // preserved between browser restarts, as expected.
  bool has_cookies = !GetCookies(embedded_test_server()->base_url()).empty();
  EXPECT_EQ(GetCookieStorageType() == StorageType::kDisk, has_cookies);
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       CookieIsolation) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  SetCookie(CookieType::kFirstParty, CookiePersistenceType::kPersistent,
            embedded_test_server());
  EXPECT_FALSE(GetCookies(embedded_test_server()->base_url()).empty());

  ForEachOtherContext(
      base::BindLambdaForTesting([&](NetworkContextType network_context_type) {
        EXPECT_TRUE(GetCookiesForContextType(network_context_type,
                                             embedded_test_server()->base_url())
                        .empty());
      }));
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       PRE_ThirdPartyCookiesBlocked) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // The system and SafeBrowsing network contexts don't support the third party
  // cookie blocking options, since they have no notion of third parties.
  bool system =
      GetParam().network_context_type == NetworkContextType::kSystem ||
      GetParam().network_context_type == NetworkContextType::kSafeBrowsing;
  if (system)
    return;

  GetPrefService()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  SetCookie(CookieType::kThirdParty, CookiePersistenceType::kSession,
            https_server());

  EXPECT_TRUE(GetCookies(https_server()->base_url()).empty());
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       ThirdPartyCookiesBlocked) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // The system and SafeBrowsing network contexts don't support the third party
  // cookie blocking options, since they have no notion of third parties.
  bool system =
      GetParam().network_context_type == NetworkContextType::kSystem ||
      GetParam().network_context_type == NetworkContextType::kSafeBrowsing;
  if (system)
    return;

  // The preference is expected to be reset in incognito mode.
  if (is_incognito()) {
    EXPECT_FALSE(GetPrefService()->GetBoolean(prefs::kBlockThirdPartyCookies));
    return;
  }

  // The kBlockThirdPartyCookies pref should carry over to the next session.
  EXPECT_TRUE(GetPrefService()->GetBoolean(prefs::kBlockThirdPartyCookies));
  SetCookie(CookieType::kThirdParty, CookiePersistenceType::kSession,
            https_server());

  EXPECT_TRUE(GetCookies(https_server()->base_url()).empty());

  // Set pref to false, third party cookies should be allowed now.
  GetPrefService()->SetBoolean(prefs::kBlockThirdPartyCookies, false);
  // Set a third-party cookie. It should actually get set this time.
  SetCookie(CookieType::kThirdParty, CookiePersistenceType::kSession,
            https_server());

  EXPECT_FALSE(GetCookies(https_server()->base_url()).empty());
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest,
                       PRE_CookieSettings) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // The system and SafeBrowsing network contexts don't respect cookie blocking
  // options, which are per-profile.
  bool system =
      GetParam().network_context_type == NetworkContextType::kSystem ||
      GetParam().network_context_type == NetworkContextType::kSafeBrowsing;
  if (system)
    return;

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  FlushNetworkInterface();
  SetCookie(CookieType::kFirstParty, CookiePersistenceType::kSession,
            embedded_test_server());

  EXPECT_TRUE(GetCookies(embedded_test_server()->base_url()).empty());
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, CookieSettings) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  // The system and SafeBrowsing network contexts don't respect cookie blocking
  // options, which are per-profile.
  bool system =
      GetParam().network_context_type == NetworkContextType::kSystem ||
      GetParam().network_context_type == NetworkContextType::kSafeBrowsing;
  if (system)
    return;

  // The content settings should carry over to the next session.
  SetCookie(CookieType::kFirstParty, CookiePersistenceType::kSession,
            embedded_test_server());

  EXPECT_TRUE(GetCookies(embedded_test_server()->base_url()).empty());

  // Set default setting to allow, cookies should be set now.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  FlushNetworkInterface();
  SetCookie(CookieType::kFirstParty, CookiePersistenceType::kSession,
            embedded_test_server());

  EXPECT_FALSE(GetCookies(embedded_test_server()->base_url()).empty());
}

// Make sure file uploads work.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationBrowserTest, UploadFile) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->method = "POST";
  request->url = embedded_test_server()->GetURL("/echo");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  base::FilePath dir_test_data;
  base::PathService::Get(chrome::DIR_TEST_DATA, &dir_test_data);
  base::FilePath path =
      dir_test_data.Append(base::FilePath(FILE_PATH_LITERAL("simple.html")));
  simple_loader->AttachFileForUpload(path, "text/html");

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  ASSERT_TRUE(simple_loader->ResponseInfo());
  ASSERT_TRUE(simple_loader->ResponseInfo()->headers);
  EXPECT_EQ(200, simple_loader->ResponseInfo()->headers->response_code());
  ASSERT_TRUE(simple_loader_helper.response_body());
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string expected_response;
  base::ReadFileToString(path, &expected_response);
  EXPECT_EQ(expected_response.c_str(), *simple_loader_helper.response_body());
}

class NetworkContextConfigurationFixedPortBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationFixedPortBrowserTest() {}
  ~NetworkContextConfigurationFixedPortBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kTestingFixedHttpPort,
        base::StringPrintf("%u", embedded_test_server()->port()));
    LOG(WARNING) << base::StringPrintf("%u", embedded_test_server()->port());
  }
};

// Test that the command line switch makes it to the network service and is
// respected.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationFixedPortBrowserTest,
                       TestingFixedPort) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  // This URL does not use the port the embedded test server is using. The
  // command line switch should make it result in the request being directed to
  // the test server anyways.
  request->url = GURL("http://127.0.0.1/echo");
  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  EXPECT_EQ(net::OK, simple_loader->NetError());
  ASSERT_TRUE(simple_loader_helper.response_body());
  EXPECT_EQ(*simple_loader_helper.response_body(), "Echo");
}

class NetworkContextConfigurationProxyOnStartBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationProxyOnStartBrowserTest() {}
  ~NetworkContextConfigurationProxyOnStartBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kProxyServer,
        embedded_test_server()->host_port_pair().ToString());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationProxyOnStartBrowserTest);
};

// Test that when there's a proxy configuration at startup, the initial requests
// use that configuration.
IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationProxyOnStartBrowserTest,
                       TestInitialProxyConfig) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  TestProxyConfigured(/*expect_success=*/true);
}

// Make sure the system URLRequestContext can handle fetching PAC scripts from
// http URLs.
class NetworkContextConfigurationHttpPacBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationHttpPacBrowserTest()
      : pac_test_server_(net::test_server::EmbeddedTestServer::TYPE_HTTP) {}

  ~NetworkContextConfigurationHttpPacBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    pac_test_server_.RegisterRequestHandler(base::Bind(
        &NetworkContextConfigurationHttpPacBrowserTest::HandlePacRequest,
        GetPacScript()));
    EXPECT_TRUE(pac_test_server_.Start());

    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
                                    pac_test_server_.base_url().spec().c_str());
  }

  static std::unique_ptr<net::test_server::HttpResponse> HandlePacRequest(
      const std::string& pac_script,
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content(pac_script);
    return response;
  }

 private:
  net::test_server::EmbeddedTestServer pac_test_server_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationHttpPacBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationHttpPacBrowserTest, HttpPac) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  TestProxyConfigured(/*expect_success=*/true);
}

// Make sure the system URLRequestContext can handle fetching PAC scripts from
// file URLs.
class NetworkContextConfigurationFilePacBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationFilePacBrowserTest() {}

  ~NetworkContextConfigurationFilePacBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const char kPacFileName[] = "foo.pac";

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath pac_file_path =
        temp_dir_.GetPath().AppendASCII(kPacFileName);

    std::string pac_script = GetPacScript();
    ASSERT_EQ(
        static_cast<int>(pac_script.size()),
        base::WriteFile(pac_file_path, pac_script.c_str(), pac_script.size()));

    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl, net::FilePathToFileURL(pac_file_path).spec());
  }

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationFilePacBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationFilePacBrowserTest, FilePac) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  TestProxyConfigured(false);
}

// Make sure the system URLRequestContext can handle fetching PAC scripts from
// data URLs.
class NetworkContextConfigurationDataPacBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationDataPacBrowserTest() {}
  ~NetworkContextConfigurationDataPacBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::string contents;
    // Read in kPACScript contents.
    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
                                    "data:," + GetPacScript());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationDataPacBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationDataPacBrowserTest, DataPac) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  TestProxyConfigured(/*expect_success=*/true);
}

// Make sure the system URLRequestContext can handle fetching PAC scripts from
// ftp URLs. Unlike the other PAC tests, this test uses a PAC script that
// results in an error, since the spawned test server is designed so that it can
// run remotely (So can't just write a script to a local file and have the
// server serve it).
class NetworkContextConfigurationFtpPacBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationFtpPacBrowserTest()
      : ftp_server_(net::SpawnedTestServer::TYPE_FTP, GetChromeTestDataDir()) {
    EXPECT_TRUE(ftp_server_.Start());
  }
  ~NetworkContextConfigurationFtpPacBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl,
        ftp_server_.GetURL("bad_server.pac").spec().c_str());
  }

 private:
  net::SpawnedTestServer ftp_server_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationFtpPacBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationFtpPacBrowserTest, FtpPac) {
  if (IsRestartStateWithInProcessNetworkService())
    return;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  // This URL should be directed to the test server because of the proxy.
  request->url = GURL("http://does.not.resolve.test:1872/echo");

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  EXPECT_EQ(net::ERR_PROXY_CONNECTION_FAILED, simple_loader->NetError());
}

// Used to test that PAC HTTPS URL stripping works. A different test server is
// used as the "proxy" based on whether the PAC script sees the full path or
// not. The servers aren't correctly set up to mimic HTTP proxies that tunnel
// to an HTTPS test server, so the test fixture just watches for any incoming
// connection.
class NetworkContextConfigurationHttpsStrippingPacBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  NetworkContextConfigurationHttpsStrippingPacBrowserTest() {}

  ~NetworkContextConfigurationHttpsStrippingPacBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Test server HostPortPair, as a string.
    std::string test_server_host_port_pair =
        net::HostPortPair::FromURL(embedded_test_server()->base_url())
            .ToString();
    // Set up a PAC file that directs to different servers based on the URL it
    // sees.
    std::string pac_script = base::StringPrintf(
        "function FindProxyForURL(url, host) {"
        // With the test URL stripped of the path, try to use the embedded test
        // server to establish a an SSL tunnel over an HTTP proxy. The request
        // will fail with ERR_TUNNEL_CONNECTION_FAILED.
        "  if (url == 'https://does.not.resolve.test:1872/')"
        "    return 'PROXY %s';"
        // With the full test URL, try to use a domain that does not resolve as
        // a proxy. Errors connecting to the proxy result in
        // ERR_PROXY_CONNECTION_FAILED.
        "  if (url == 'https://does.not.resolve.test:1872/foo')"
        "    return 'PROXY does.not.resolve.test';"
        // Otherwise, use direct. If a connection to "does.not.resolve.test"
        // tries to use a direction connection, it will fail with
        // ERR_NAME_NOT_RESOLVED. This path will also
        // be used by the initial request in NetworkServiceState::kRestarted
        // tests to make sure the network service process is fully started
        // before it's crashed and restarted. Using direct in this case avoids
        // that request failing with an unexpeced error when being directed to a
        // bogus proxy.
        "  return 'DIRECT';"
        "}",
        test_server_host_port_pair.c_str());

    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
                                    "data:," + pac_script);
  }
};

class NetworkContextConfigurationProxySettingsBrowserTest
    : public NetworkContextConfigurationHttpPacBrowserTest {
 public:
  const size_t kDefaultMaxConnectionsPerProxy = 32;

  NetworkContextConfigurationProxySettingsBrowserTest() = default;
  ~NetworkContextConfigurationProxySettingsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &NetworkContextConfigurationProxySettingsBrowserTest::TrackConnections,
        base::Unretained(this)));

    expected_connections_loop_ptr_.store(nullptr);

    NetworkContextConfigurationHttpPacBrowserTest::SetUpOnMainThread();
  }

  virtual size_t GetExpectedMaxConnectionsPerProxy() const {
    return kDefaultMaxConnectionsPerProxy;
  }

  std::unique_ptr<net::test_server::HttpResponse> TrackConnections(
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, "/hung",
                          base::CompareCase::INSENSITIVE_ASCII))
      return nullptr;

    // Record the number of connections we're seeing.
    CHECK(observed_request_urls_.find(request.GetURL().spec()) ==
          observed_request_urls_.end());
    observed_request_urls_.emplace(request.GetURL().spec());
    CHECK_GE(GetExpectedMaxConnectionsPerProxy(),
             observed_request_urls_.size());

    // Once we've seen at least as many connections as we expect, we can quit
    // the loop on the main test thread. The test may choose to wait for
    // longer to see if there are any additional unexpected connections.
    if (GetExpectedMaxConnectionsPerProxy() == observed_request_urls_.size() &&
        expected_connections_loop_ptr_.load() != nullptr) {
      expected_connections_loop_ptr_.load()->Quit();
    }

    // To test the number of connections per proxy, we'll hang all responses.
    return std::make_unique<net::test_server::HungResponse>();
  }

  void RunMaxConnectionsPerProxyTest() {
    // At this point in the test, we've set up a proxy that points to our
    // embedded test server. We've also set things up to hang all incoming
    // requests and record how many concurrent connections we have running. To
    // detect the maximum number of requests per proxy, we now just have to
    // attempt to make as many requests as possible to ensure we don't see an
    // incorrect number of concurrent requests.

    // First of all, we're going to want to wait for at least as many
    // connections as we expect.
    base::RunLoop expected_connections_run_loop;
    expected_connections_loop_ptr_.store(&expected_connections_run_loop);

    std::vector<std::unique_ptr<network::SimpleURLLoader>> loaders(
        GetExpectedMaxConnectionsPerProxy());
    for (unsigned int i = 0; i < GetExpectedMaxConnectionsPerProxy() + 1; ++i) {
      std::unique_ptr<network::ResourceRequest> request =
          std::make_unique<network::ResourceRequest>();
      request->url =
          embedded_test_server()->GetURL(base::StringPrintf("foo%u.test", i),
                                         base::StringPrintf("/hung_%u", i));

      content::SimpleURLLoaderTestHelper simple_loader_helper;
      std::unique_ptr<network::SimpleURLLoader> simple_loader =
          network::SimpleURLLoader::Create(std::move(request),
                                           TRAFFIC_ANNOTATION_FOR_TESTS);

      simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
          loader_factory(), simple_loader_helper.GetCallback());
      loaders.emplace_back(std::move(simple_loader));
    }
    expected_connections_run_loop.Run();

    // Then wait for any remaining connections that we should NOT get.
    base::RunLoop unexpected_connections_run_loop;
    base::RunLoop::ScopedRunTimeoutForTest run_timeout(
        base::TimeDelta::FromMilliseconds(100),
        base::BindLambdaForTesting(
            [&]() { unexpected_connections_run_loop.Quit(); }));
    unexpected_connections_run_loop.Run();

    // Stop the server.
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

 private:
  std::atomic<base::RunLoop*> expected_connections_loop_ptr_{nullptr};

  // In RunMaxConnectionsPerProxyTest(), we'll make several network requests
  // that hang. These hung requests are assumed to last for the duration of the
  // test. This member, which is only accessed from the server's IO thread,
  // records each observed request to ensure we see only as many connections as
  // we expect.
  std::unordered_set<std::string> observed_request_urls_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextConfigurationProxySettingsBrowserTest);
};

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationProxySettingsBrowserTest,
                       MaxConnectionsPerProxy) {
  RunMaxConnectionsPerProxyTest();
}

class NetworkContextConfigurationManagedProxySettingsBrowserTest
    : public NetworkContextConfigurationProxySettingsBrowserTest {
 public:
  const size_t kTestMaxConnectionsPerProxy = 37;

  NetworkContextConfigurationManagedProxySettingsBrowserTest() = default;
  ~NetworkContextConfigurationManagedProxySettingsBrowserTest() override =
      default;

  void SetUpInProcessBrowserTestFixture() override {
    NetworkContextConfigurationProxySettingsBrowserTest::
        SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    policies.Set(policy::key::kMaxConnectionsPerProxy,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(
                     static_cast<int>(kTestMaxConnectionsPerProxy)),
                 /*external_data_fetcher=*/nullptr);
    UpdateChromePolicy(policies);
  }

  size_t GetExpectedMaxConnectionsPerProxy() const override {
    return kTestMaxConnectionsPerProxy;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      NetworkContextConfigurationManagedProxySettingsBrowserTest);
};

IN_PROC_BROWSER_TEST_P(
    NetworkContextConfigurationManagedProxySettingsBrowserTest,
    MaxConnectionsPerProxy) {
  RunMaxConnectionsPerProxyTest();
}

// Used to test that we persist Reporting clients and NEL policies to disk, but
// only when appropriate.
class NetworkContextConfigurationReportingAndNelBrowserTest
    : public NetworkContextConfigurationBrowserTest {
 public:
  struct RequestState {
    content::SimpleURLLoaderTestHelper helper;
    std::unique_ptr<network::SimpleURLLoader> loader;
  };

  NetworkContextConfigurationReportingAndNelBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    // Set up an SSL certificate so we can make requests to "https://localhost"
    https_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    // Only start the server if we actually need it in the test (i.e., we'll
    // call StartAcceptingConnections somewhere in the test).
    if (!IsRestartStateWithInProcessNetworkService() &&
        AreReportingAndNelEnabled()) {
      EXPECT_TRUE(https_server_.InitializeAndListen());
    }

    // Make report delivery happen instantly.
    net::ReportingPolicy policy;
    policy.delivery_interval = base::TimeDelta::FromSeconds(0);
    net::ReportingPolicy::UsePolicyForTesting(policy);
  }

  std::unique_ptr<RequestState> NewSimpleRequest(const GURL& url) {
    auto request_state = std::make_unique<RequestState>();
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = url;
    request_state->loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
    request_state->loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory(), request_state->helper.GetCallback());
    return request_state;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kShortReportingDelay);
    // This switch will cause traffic to *any* port to go to https_server_,
    // regardless of which arbitrary port https_server_ decides to run on.
    // NEL and Reporting policies are only valid for a single origin.
    if (!IsRestartStateWithInProcessNetworkService() &&
        AreReportingAndNelEnabled()) {
      command_line->AppendSwitchASCII(switches::kTestingFixedHttpsPort,
                                      base::StringPrintf("%u", port()));
    }
  }

  net::EmbeddedTestServer* http_server() { return &https_server_; }
  int port() const { return https_server_.port(); }

  static GURL GetCollectorURL() { return GURL("https://localhost/upload"); }

  static std::string GetReportToHeader() {
    return "Report-To: {\"endpoints\":[{\"url\":\"" + GetCollectorURL().spec() +
           "\"}],\"max_age\":86400}\r\n";
  }

  static std::string GetNELHeader() {
    return "NEL: "
           "{\"report_to\":\"default\",\"max_age\":86400,\"success_fraction\":"
           "1.0}\r\n";
  }

  void RespondWithHeaders(net::test_server::ControllableHttpResponse* resp) {
    resp->WaitForRequest();
    resp->Send("HTTP/1.1 200 OK\r\n");
    resp->Send(GetReportToHeader());
    resp->Send(GetNELHeader());
    resp->Send("\r\n");
    resp->Done();
  }

  void RespondWithoutHeaders(net::test_server::ControllableHttpResponse* resp) {
    resp->WaitForRequest();
    resp->Send("HTTP/1.1 200 OK\r\n");
    resp->Send("\r\n");
    resp->Done();
  }

  bool AreReportingAndNelEnabled() const {
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
      case NetworkContextType::kSafeBrowsing:
        return false;
      case NetworkContextType::kProfile:
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kOnDiskApp:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        return true;
    }
  }

  bool ExpectPersistenceEnabled() const {
    switch (GetParam().network_context_type) {
      case NetworkContextType::kSystem:
      case NetworkContextType::kSafeBrowsing:
      case NetworkContextType::kIncognitoProfile:
      case NetworkContextType::kInMemoryApp:
      case NetworkContextType::kOnDiskAppWithIncognitoProfile:
        return false;
      case NetworkContextType::kProfile:
      case NetworkContextType::kOnDiskApp:
        return true;
    }
  }

 private:
  net::EmbeddedTestServer https_server_;
};

namespace {

constexpr char kReportingEnabledURL[] = "https://localhost/page";
constexpr char kCrossOriginReportingEnabledURL[] = "https://localhost:444/page";

}  // namespace

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationReportingAndNelBrowserTest,
                       PRE_PersistReportingAndNel) {
  if (IsRestartStateWithInProcessNetworkService() ||
      !AreReportingAndNelEnabled()) {
    return;
  }

  net::test_server::ControllableHttpResponse original_response(http_server(),
                                                               "/page");
  net::test_server::ControllableHttpResponse upload_response(http_server(),
                                                             "/upload");
  http_server()->StartAcceptingConnections();

  // Make a request to /page and respond with Report-To and NEL headers, which
  // will turn on NEL for the localhost:443 origin.
  std::unique_ptr<RequestState> request =
      NewSimpleRequest(GURL(kReportingEnabledURL));
  RespondWithHeaders(&original_response);
  request->helper.WaitForCallback();

  // Wait for a request to /upload, which should happen because the previous
  // request precipitated a NEL report.
  upload_response.WaitForRequest();
  EXPECT_EQ(net::test_server::METHOD_POST,
            upload_response.http_request()->method);
}

IN_PROC_BROWSER_TEST_P(NetworkContextConfigurationReportingAndNelBrowserTest,
                       PersistReportingAndNel) {
  if (IsRestartStateWithInProcessNetworkService() ||
      !AreReportingAndNelEnabled()) {
    return;
  }

  net::test_server::ControllableHttpResponse original_response(http_server(),
                                                               "/page");
  net::test_server::ControllableHttpResponse cross_origin_response(
      http_server(), "/page");
  net::test_server::ControllableHttpResponse upload_response(http_server(),
                                                             "/upload");
  http_server()->StartAcceptingConnections();

  // Make a request that will only precipitate a NEL report if we loaded a NEL
  // policy from the store.
  std::unique_ptr<RequestState> request =
      NewSimpleRequest(GURL(kReportingEnabledURL));
  RespondWithoutHeaders(&original_response);
  request->helper.WaitForCallback();

  // Make another request that will generate another upload to the same
  // collector. If the previous request didn't generate a NEL report (e.g.,
  // because persistence is disabled) then this'll be first upload to the NEL
  // collector. Note that because this upload is to a different origin than the
  // request, Reporting will make a CORS preflight request, which is what we
  // look for.
  std::unique_ptr<RequestState> cross_origin_request =
      NewSimpleRequest(GURL(kCrossOriginReportingEnabledURL));
  RespondWithHeaders(&cross_origin_response);
  cross_origin_request->helper.WaitForCallback();

  // Wait for a request to /upload, which should happen because the previous
  // requests precipitated NEL reports.
  //
  // We assume that Reporting/NEL will send reports in the order we made the
  // requests above, which seems to be the case today but may not necessarily
  // true in general.
  upload_response.WaitForRequest();
  if (ExpectPersistenceEnabled()) {
    // A NEL upload about https://localhost
    EXPECT_EQ(net::test_server::METHOD_POST,
              upload_response.http_request()->method);
  } else {
    // A CORS preflight request before a cross-origin NEL upload about
    // https://localhost:444
    EXPECT_EQ(net::test_server::METHOD_OPTIONS,
              upload_response.http_request()->method);
  }
}

// Instantiates tests with a prefix indicating which NetworkContext is being
// tested, and a suffix of "/0" if the network service is disabled, "/1" if it's
// enabled, and "/2" if it's enabled and restarted.
#define TEST_CASES(network_context_type)                           \
  TestCase({NetworkServiceState::kEnabled, network_context_type}), \
      TestCase({NetworkServiceState::kRestarted, network_context_type})

#if BUILDFLAG(ENABLE_EXTENSIONS)
#define INSTANTIATE_EXTENSION_TESTS(TestFixture)                        \
  INSTANTIATE_TEST_SUITE_P(                                             \
      OnDiskApp, TestFixture,                                           \
      ::testing::Values(TEST_CASES(NetworkContextType::kOnDiskApp)));   \
                                                                        \
  INSTANTIATE_TEST_SUITE_P(                                             \
      InMemoryApp, TestFixture,                                         \
      ::testing::Values(TEST_CASES(NetworkContextType::kInMemoryApp))); \
                                                                        \
  INSTANTIATE_TEST_SUITE_P(                                             \
      OnDiskAppWithIncognitoProfile, TestFixture,                       \
      ::testing::Values(                                                \
          TEST_CASES(NetworkContextType::kOnDiskAppWithIncognitoProfile)));
#else  // !BUILDFLAG(ENABLE_EXTENSIONS)
#define INSTANTIATE_EXTENSION_TESTS(TestFixture)
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)

#define INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(TestFixture)             \
  INSTANTIATE_EXTENSION_TESTS(TestFixture)                               \
  INSTANTIATE_TEST_SUITE_P(                                              \
      SystemNetworkContext, TestFixture,                                 \
      ::testing::Values(TEST_CASES(NetworkContextType::kSystem)));       \
                                                                         \
  INSTANTIATE_TEST_SUITE_P(                                              \
      SafeBrowsingNetworkContext, TestFixture,                           \
      ::testing::Values(TEST_CASES(NetworkContextType::kSafeBrowsing))); \
                                                                         \
  INSTANTIATE_TEST_SUITE_P(                                              \
      ProfileMainNetworkContext, TestFixture,                            \
      ::testing::Values(TEST_CASES(NetworkContextType::kProfile)));      \
                                                                         \
  INSTANTIATE_TEST_SUITE_P(                                              \
      IncognitoProfileMainNetworkContext, TestFixture,                   \
      ::testing::Values(TEST_CASES(NetworkContextType::kIncognitoProfile)))

INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(NetworkContextConfigurationBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationFixedPortBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationProxyOnStartBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationHttpPacBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationFilePacBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationDataPacBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationFtpPacBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationHttpsStrippingPacBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationProxySettingsBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationManagedProxySettingsBrowserTest);
INSTANTIATE_TEST_CASES_FOR_TEST_FIXTURE(
    NetworkContextConfigurationReportingAndNelBrowserTest);

}  // namespace
