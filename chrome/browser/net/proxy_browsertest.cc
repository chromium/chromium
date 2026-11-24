// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/linux/dbus/buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/install_default_websocket_handlers.h"
#include "net/test/embedded_test_server/register_basic_auth_handler.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/net/dhcp_wpad_url_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS))
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/proxy_resolver/public/mojom/system_proxy_resolver.mojom.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) &&
        // BUILDFLAG(USE_DBUS))

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
#include "components/dbus/xdg/portal.h"
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)

namespace {

// Verify kPACScript is installed as the PAC script.
void VerifyProxyScript(BrowserWindowInterface* browser) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser, GURL("https://google.com")));

  // Verify we get the ERR_PROXY_CONNECTION_FAILED screen.
  EXPECT_EQ(true, content::EvalJs(
                      browser->tab_strip_model()->GetActiveWebContents(),
                      "var textContent = document.body.textContent;"
                      "var hasError = "
                      "textContent.indexOf('ERR_PROXY_CONNECTION_FAILED') >= 0;"
                      "hasError;"));
}

class ProxyBrowserTest : public InProcessBrowserTest {
 public:
  ProxyBrowserTest() {
    net::test_server::RegisterProxyBasicAuthHandler(*embedded_test_server(),
                                                    "user", "pass");
    EXPECT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  ProxyBrowserTest(const ProxyBrowserTest&) = delete;
  ProxyBrowserTest& operator=(const ProxyBrowserTest&) = delete;

  ~ProxyBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kProxyServer,
        embedded_test_server()->host_port_pair().ToString());
  }
};

// Test that the browser can establish a WebSocket connection via a proxy
// that requires basic authentication. This test also checks the headers
// arrive at WebSocket server.
IN_PROC_BROWSER_TEST_F(ProxyBrowserTest, BasicAuthWSConnect) {
  net::test_server::EmbeddedTestServer ws_server{
      net::test_server::EmbeddedTestServer::Type::TYPE_HTTP};
  net::test_server::InstallDefaultWebSocketHandlers(&ws_server);
  EXPECT_TRUE(ws_server.Start());

  embedded_test_server()->EnableConnectProxy(
      {net::HostPortPair::FromURL(ws_server.GetURL("host.test", "/"))});
  embedded_test_server()->StartAcceptingConnections();

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TitleWatcher watcher(tab, u"PASS");
  watcher.AlsoWaitForTitle(u"FAIL");

  // Visit a page that tries to establish WebSocket connection. The title
  // of the page will be 'PASS' on success.
  //
  // Note that "http://host.test:?/websocket/proxied_request_check.html" is
  // requested from the "proxy" server using a GET, rather than a CONNECT
  // request, because of how proxying HTTP over HTTP/HTTPS proxies works. The
  // "proxy" EmbeddedTestServer uses the default file handler to serves
  // "proxied" GET responses directly rather than actually proxying them. The
  // WebSocket request, however, uses a CONNECT request when proxied, which the
  // EmbeddedTestServer does actually proxy to the destination. As a result, the
  // two files act like they're served from the same origin, even though one is
  // tunnelled to `ws_server`, and the other only looks to Chrome like it is.
  //
  // Use a hostname other than localhost to avoid behavior on some platforms
  // that prevents proxying requests to localhost by default.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      ws_server.GetURL("host.test", "/websocket/proxied_request_check.html")));

  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));
  LoginHandler::GetAllLoginHandlersForTest().front()->SetAuth(u"user", u"pass");

  const std::u16string result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::EqualsASCII(result, "PASS"));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS))
constexpr char kNativeProxyTestUrl[] = "http://does.not.resolve.test/echo";

class NativeSystemProxyBrowserClient
    : public ChromeContentBrowserClient,
      public proxy_resolver::mojom::SystemProxyResolver {
 public:
  NativeSystemProxyBrowserClient(const net::HostPortPair& proxy_server,
                                 bool force_system_config)
      : force_system_config_(force_system_config) {
    proxy_list_.Set(proxy_server.ToString());
  }

  bool resolved_test_url() const { return resolved_test_url_; }

  // ChromeContentBrowserClient:
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override {
    ChromeContentBrowserClient::ConfigureNetworkContextParams(
        context, in_memory, relative_partition_path, network_context_params,
        cert_verifier_creation_params);
    EXPECT_TRUE(network_context_params->system_proxy_resolver.is_valid());

    if (force_system_config_) {
      net::ProxyConfig config;
      config.set_from_system(true);
      network_context_params->initial_proxy_config =
          net::ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS);
      // Keep subsequent OS configuration updates from replacing the test
      // config.
      network_context_params->proxy_config_client_receiver.reset();
    }
    network_context_params->system_proxy_resolver.reset();
    receivers_.Add(this, network_context_params->system_proxy_resolver
                             .InitWithNewPipeAndPassReceiver());
  }

  // proxy_resolver::mojom::SystemProxyResolver:
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override {
    if (url == GURL(kNativeProxyTestUrl)) {
      resolved_test_url_ = true;
    }
    auto status = proxy_resolver::mojom::SystemProxyResolutionStatus::New();
    status->is_success = true;
    std::move(callback).Run(proxy_list_, std::move(status));
  }

 private:
  const bool force_system_config_;
  net::ProxyList proxy_list_;
  bool resolved_test_url_ = false;
  mojo::ReceiverSet<proxy_resolver::mojom::SystemProxyResolver> receivers_;
};

class NativeSystemProxyBrowserTest : public InProcessBrowserTest {
 public:
  NativeSystemProxyBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSystemProxyResolver);
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddSimulatedFailure("does.not.resolve.test");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

#if BUILDFLAG(IS_LINUX)
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kFailed);
  }

  void TearDownInProcessBrowserTestFixture() override {
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }
#endif  // BUILDFLAG(IS_LINUX)

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NativeSystemProxyBrowserTest, NativeProxyNavigation) {
  NativeSystemProxyBrowserClient browser_client(
      embedded_test_server()->host_port_pair(), /*force_system_config=*/true);
  content::ScopedContentBrowserClientSetting browser_client_setting(
      &browser_client);

  // A fresh profile creates its NetworkContext through the test browser client.
  BrowserWindowInterface* proxy_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(proxy_browser, GURL(kNativeProxyTestUrl)));
  EXPECT_TRUE(browser_client.resolved_test_url());
  EXPECT_EQ(
      "Echo",
      content::EvalJs(proxy_browser->GetTabStripModel()->GetActiveWebContents(),
                      "document.body.textContent"));
  CloseBrowserSynchronously(proxy_browser);
}

IN_PROC_BROWSER_TEST_F(NativeSystemProxyBrowserTest,
                       BrowserConfiguredProxyNavigation) {
  browser()->GetProfile()->GetPrefs()->SetDict(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers(
          embedded_test_server()->host_port_pair().ToString(), std::string()));

  NativeSystemProxyBrowserClient browser_client(
      net::HostPortPair(GURL(kNativeProxyTestUrl).host(), 80),
      /*force_system_config=*/false);
  content::ScopedContentBrowserClientSetting browser_client_setting(
      &browser_client);

  BrowserWindowInterface* proxy_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(proxy_browser, GURL(kNativeProxyTestUrl)));
  EXPECT_FALSE(browser_client.resolved_test_url());
  EXPECT_EQ(
      "Echo",
      content::EvalJs(proxy_browser->GetTabStripModel()->GetActiveWebContents(),
                      "document.body.textContent"));
  CloseBrowserSynchronously(proxy_browser);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) &&
        // BUILDFLAG(USE_DBUS))

// Fetches a PAC script via an http:// URL, and ensures that requests to
// https://www.google.com fail with ERR_PROXY_CONNECTION_FAILED (by virtue of
// PAC file having selected a non-existent PROXY server).
class BaseHttpProxyScriptBrowserTest : public InProcessBrowserTest {
 public:
  BaseHttpProxyScriptBrowserTest() {
    http_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  BaseHttpProxyScriptBrowserTest(const BaseHttpProxyScriptBrowserTest&) =
      delete;
  BaseHttpProxyScriptBrowserTest& operator=(
      const BaseHttpProxyScriptBrowserTest&) = delete;

  ~BaseHttpProxyScriptBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(http_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl,
        http_server_.GetURL("/" + GetPacFilename()).spec());
  }

 protected:
  virtual std::string GetPacFilename() = 0;
  net::EmbeddedTestServer http_server_;
};

// Tests the use of a PAC script that rejects requests to
// https://www.google.com/
class HttpProxyScriptBrowserTest : public BaseHttpProxyScriptBrowserTest {
 public:
  HttpProxyScriptBrowserTest() = default;

  HttpProxyScriptBrowserTest(const HttpProxyScriptBrowserTest&) = delete;
  HttpProxyScriptBrowserTest& operator=(const HttpProxyScriptBrowserTest&) =
      delete;

  ~HttpProxyScriptBrowserTest() override = default;

  std::string GetPacFilename() override {
    // PAC script that sends all requests to an invalid proxy server.
    return "bad_server.pac";
  }
};

IN_PROC_BROWSER_TEST_F(HttpProxyScriptBrowserTest, Verify) {
  VerifyProxyScript(browser());
}

#if BUILDFLAG(IS_CHROMEOS)
// Tests the use of a PAC script set via Web Proxy Autodiscovery Protocol.
// TODO(crbug.com/41475031): Add a test case for when DhcpWpadUrlClient
// returns an empty PAC URL.
class WPADHttpProxyScriptBrowserTest : public HttpProxyScriptBrowserTest {
 public:
  WPADHttpProxyScriptBrowserTest() = default;

  WPADHttpProxyScriptBrowserTest(const WPADHttpProxyScriptBrowserTest&) =
      delete;
  WPADHttpProxyScriptBrowserTest& operator=(
      const WPADHttpProxyScriptBrowserTest&) = delete;

  ~WPADHttpProxyScriptBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(http_server_.Start());
    pac_url_ = http_server_.GetURL("/" + GetPacFilename());
    ash::DhcpWpadUrlClient::SetPacUrlForTesting(pac_url_);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    ash::DhcpWpadUrlClient::ClearPacUrlForTesting();
    InProcessBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kProxyAutoDetect);
  }

 private:
  GURL pac_url_;
};

IN_PROC_BROWSER_TEST_F(WPADHttpProxyScriptBrowserTest, Verify) {
  VerifyProxyScript(browser());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Tests the use of a PAC script that rejects requests to
// https://www.google.com/ when myIpAddress() and myIpAddressEx() appear to be
// working.
class MyIpAddressProxyScriptBrowserTest
    : public BaseHttpProxyScriptBrowserTest {
 public:
  MyIpAddressProxyScriptBrowserTest() = default;

  MyIpAddressProxyScriptBrowserTest(const MyIpAddressProxyScriptBrowserTest&) =
      delete;
  MyIpAddressProxyScriptBrowserTest& operator=(
      const MyIpAddressProxyScriptBrowserTest&) = delete;

  ~MyIpAddressProxyScriptBrowserTest() override = default;

  std::string GetPacFilename() override {
    // PAC script that sends all requests to an invalid proxy server provided
    // myIpAddress() and myIpAddressEx() are not loopback addresses.
    return "my_ip_address.pac";
  }
};

IN_PROC_BROWSER_TEST_F(MyIpAddressProxyScriptBrowserTest, Verify) {
  VerifyProxyScript(browser());
}

// Fetch PAC script via a hanging http:// URL.
class HangingPacRequestProxyScriptBrowserTest : public InProcessBrowserTest {
 public:
  HangingPacRequestProxyScriptBrowserTest() = default;

  HangingPacRequestProxyScriptBrowserTest(
      const HangingPacRequestProxyScriptBrowserTest&) = delete;
  HangingPacRequestProxyScriptBrowserTest& operator=(
      const HangingPacRequestProxyScriptBrowserTest&) = delete;

  ~HangingPacRequestProxyScriptBrowserTest() override = default;

  void SetUp() override {
    // Must start listening (And get a port for the proxy) before calling
    // SetUp().
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    // Need to stop this before |connection_listener_| is destroyed.
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDown();
  }

  void SetUpOnMainThread() override {
    // This must be created after the main message loop has been set up.
    // Waits for one connection.  Additional connections are fine.
    connection_listener_ =
        std::make_unique<net::test_server::SimpleConnectionListener>(
            1, net::test_server::SimpleConnectionListener::
                   ALLOW_ADDITIONAL_CONNECTIONS);
    embedded_test_server()->SetConnectionListener(connection_listener_.get());
    embedded_test_server()->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl, embedded_test_server()->GetURL("/hung").spec());
  }

 protected:
  std::unique_ptr<net::test_server::SimpleConnectionListener>
      connection_listener_;
};

// Check that the URLRequest for a PAC that is still alive during shutdown is
// safely cleaned up.  This test relies on AssertNoURLRequests being called on
// the main URLRequestContext.
IN_PROC_BROWSER_TEST_F(HangingPacRequestProxyScriptBrowserTest, Shutdown) {
  // Request that should hang while trying to request the PAC script.
  // Enough requests are created on startup that this probably isn't needed, but
  // best to be safe.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://blah/");
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);

  auto* storage_partition =
      browser()->GetProfile()->GetDefaultStoragePartition();
  auto url_loader_factory =
      storage_partition->GetURLLoaderFactoryForBrowserProcess();
  simple_loader->DownloadHeadersOnly(
      url_loader_factory.get(),
      base::BindOnce([](scoped_refptr<net::HttpResponseHeaders> headers) {
        ADD_FAILURE() << "This request should never complete.";
      }));

  connection_listener_->WaitForConnections();
}

}  // namespace
