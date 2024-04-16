// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/proxy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/load_flags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/net/dhcp_wpad_url_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// Verify kPACScript is installed as the PAC script.
void VerifyProxyScript(Browser* browser) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, GURL("http://google.com")));

  // Verify we get the ERR_PROXY_CONNECTION_FAILED screen.
  EXPECT_EQ(true, content::EvalJs(
                      browser->tab_strip_model()->GetActiveWebContents(),
                      "var textContent = document.body.textContent;"
                      "var hasError = "
                      "textContent.indexOf('ERR_PROXY_CONNECTION_FAILED') >= 0;"
                      "hasError;"));
}

// Test that the browser can establish a WebSocket connection via a proxy
// that requires basic authentication. This test also checks the headers
// arrive at WebSocket server.
IN_PROC_BROWSER_TEST_F(ProxyBrowserTest, BasicAuthWSConnect) {
  // Launch WebSocket server.
  net::SpawnedTestServer ws_server(net::SpawnedTestServer::TYPE_WS,
                                   net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TitleWatcher watcher(tab, u"PASS");
  watcher.AlsoWaitForTitle(u"FAIL");

  // Visit a page that tries to establish WebSocket connection. The title
  // of the page will be 'PASS' on success.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("http");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), ws_server.GetURL("proxied_request_check.html")
                     .ReplaceComponents(replacements)));

  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));
  LoginHandler::GetAllLoginHandlersForTest().front()->SetAuth(u"foo", u"bar");

  const std::u16string result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::EqualsASCII(result, "PASS"));
}

// Fetches a PAC script via an http:// URL, and ensures that requests to
// http://www.google.com fail with ERR_PROXY_CONNECTION_FAILED (by virtue of
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

  ~BaseHttpProxyScriptBrowserTest() override {}

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

// Tests the use of a PAC script that rejects requests to http://www.google.com/
class HttpProxyScriptBrowserTest : public BaseHttpProxyScriptBrowserTest {
 public:
  HttpProxyScriptBrowserTest() = default;

  HttpProxyScriptBrowserTest(const HttpProxyScriptBrowserTest&) = delete;
  HttpProxyScriptBrowserTest& operator=(const HttpProxyScriptBrowserTest&) =
      delete;

  ~HttpProxyScriptBrowserTest() override {}

  std::string GetPacFilename() override {
    // PAC script that sends all requests to an invalid proxy server.
    return "bad_server.pac";
  }
};

IN_PROC_BROWSER_TEST_F(HttpProxyScriptBrowserTest, Verify) {
  VerifyProxyScript(browser());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  ~WPADHttpProxyScriptBrowserTest() override {}

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests the use of a PAC script that rejects requests to http://www.google.com/
// when myIpAddress() and myIpAddressEx() appear to be working.
class MyIpAddressProxyScriptBrowserTest
    : public BaseHttpProxyScriptBrowserTest {
 public:
  MyIpAddressProxyScriptBrowserTest() = default;

  MyIpAddressProxyScriptBrowserTest(const MyIpAddressProxyScriptBrowserTest&) =
      delete;
  MyIpAddressProxyScriptBrowserTest& operator=(
      const MyIpAddressProxyScriptBrowserTest&) = delete;

  ~MyIpAddressProxyScriptBrowserTest() override {}

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
  HangingPacRequestProxyScriptBrowserTest() {}

  HangingPacRequestProxyScriptBrowserTest(
      const HangingPacRequestProxyScriptBrowserTest&) = delete;
  HangingPacRequestProxyScriptBrowserTest& operator=(
      const HangingPacRequestProxyScriptBrowserTest&) = delete;

  ~HangingPacRequestProxyScriptBrowserTest() override {}

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
  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);

  auto* storage_partition = browser()->profile()->GetDefaultStoragePartition();
  auto url_loader_factory =
      storage_partition->GetURLLoaderFactoryForBrowserProcess();
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce([](std::unique_ptr<std::string> body) {
        ADD_FAILURE() << "This request should never complete.";
      }));

  connection_listener_->WaitForConnections();
}

}  // namespace
