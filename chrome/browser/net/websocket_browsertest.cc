// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_handler_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/site_for_cookies.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class WebSocketBrowserTest : public InProcessBrowserTest {
 public:
  WebSocketBrowserTest()
      : ws_server_(net::SpawnedTestServer::TYPE_WS,
                   net::GetWebSocketTestDataDirectory()),
        wss_server_(net::SpawnedTestServer::TYPE_WSS,
                    SSLOptions(SSLOptions::CERT_OK),
                    net::GetWebSocketTestDataDirectory()) {}

 protected:
  void NavigateToHTTP(const std::string& path) {
    // Visit a HTTP page for testing.
    GURL::Replacements replacements;
    replacements.SetSchemeStr("http");
    ui_test_utils::NavigateToURL(
        browser(), ws_server_.GetURL(path).ReplaceComponents(replacements));
  }

  void NavigateToHTTPS(const std::string& path) {
    // Visit a HTTPS page for testing.
    GURL::Replacements replacements;
    replacements.SetSchemeStr("https");
    ui_test_utils::NavigateToURL(
        browser(), wss_server_.GetURL(path).ReplaceComponents(replacements));
  }

  // Prepare the title watcher.
  void SetUpOnMainThread() override {
    watcher_ = std::make_unique<content::TitleWatcher>(
        browser()->tab_strip_model()->GetActiveWebContents(), u"PASS");
    watcher_->AlsoWaitForTitle(u"FAIL");
  }

  void TearDownOnMainThread() override { watcher_.reset(); }

  std::string WaitAndGetTitle() {
    return base::UTF16ToUTF8(watcher_->WaitAndGetTitle());
  }

  // Triggers a WebSocket connection to the given |url| in the context of the
  // main frame of the active WebContents.
  void MakeWebSocketConnection(
      const GURL& url,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client) {
    content::RenderFrameHost* const frame =
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    content::RenderProcessHost* const process = frame->GetProcess();

    const std::vector<std::string> requested_protocols;
    const net::SiteForCookies site_for_cookies;
    // The actual value of this doesn't actually matter, it just can't be empty,
    // to avoid a DCHECK.
    const net::IsolationInfo isolation_info =
        net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url));
    std::vector<network::mojom::HttpHeaderPtr> additional_headers;
    const url::Origin origin;

    process->GetStoragePartition()->GetNetworkContext()->CreateWebSocket(
        url, requested_protocols, site_for_cookies, isolation_info,
        std::move(additional_headers), process->GetID(), origin,
        network::mojom::kWebSocketOptionNone,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::move(handshake_client),
        process->GetStoragePartition()->CreateURLLoaderNetworkObserverForFrame(
            process->GetID(), frame->GetRoutingID()),
        /*auth_handler=*/mojo::NullRemote(),
        /*header_client=*/mojo::NullRemote());
  }

  net::SpawnedTestServer ws_server_;
  net::SpawnedTestServer wss_server_;

 private:
  typedef net::SpawnedTestServer::SSLOptions SSLOptions;
  std::unique_ptr<content::TitleWatcher> watcher_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketBrowserTest);
};

// Framework for tests using the connect_to.html page served by a separate HTTP
// server.
class WebSocketBrowserConnectToTest : public WebSocketBrowserTest {
 protected:
  WebSocketBrowserConnectToTest() {
    http_server_.ServeFilesFromSourceDirectory(
        net::GetWebSocketTestDataDirectory());
  }

  // The title watcher and HTTP server are set up automatically by the test
  // framework. Each test case still needs to configure and start the
  // WebSocket server(s) it needs.
  void SetUpOnMainThread() override {
    WebSocketBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(http_server_.Start());
  }

  // Supply a ws: or wss: URL to connect to.
  void ConnectTo(GURL url) {
    ASSERT_TRUE(http_server_.Started());
    std::string query("url=" + url.spec());
    GURL::Replacements replacements;
    replacements.SetQueryStr(query);
    ui_test_utils::NavigateToURL(browser(),
                                 http_server_.GetURL("/connect_to.html")
                                     .ReplaceComponents(replacements));
  }

 private:
  net::EmbeddedTestServer http_server_;
};

// Automatically fill in any login prompts that appear with the supplied
// credentials.
class AutoLogin : public content::NotificationObserver {
 public:
  AutoLogin(const std::string& username,
            const std::string& password,
            content::NavigationController* navigation_controller)
      : username_(base::UTF8ToUTF16(username)),
        password_(base::UTF8ToUTF16(password)),
        logged_in_(false) {
    registrar_.Add(
        this,
        chrome::NOTIFICATION_AUTH_NEEDED,
        content::Source<content::NavigationController>(navigation_controller));
  }

  // NotificationObserver implementation
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(chrome::NOTIFICATION_AUTH_NEEDED, type);
    LoginHandler* login_handler =
        content::Details<LoginNotificationDetails>(details)->handler();
    login_handler->SetAuth(username_, password_);
    logged_in_ = true;
  }

  bool logged_in() const { return logged_in_; }

 private:
  const std::u16string username_;
  const std::u16string password_;
  bool logged_in_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutoLogin);
};

// Test that the browser can handle a WebSocket frame split into multiple TCP
// segments.
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, WebSocketSplitSegments) {
  // Launch a WebSocket server.
  ASSERT_TRUE(ws_server_.Start());

  NavigateToHTTP("split_packet_check.html");

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

// TODO(crbug.com/1176880): Disabled on macOS because the WSS SpawnedTestServer
// does not support modern TLS on the macOS bots.
#if defined(OS_MAC)
#define MAYBE_SecureWebSocketSplitRecords DISABLED_SecureWebSocketSplitRecords
#else
#define MAYBE_SecureWebSocketSplitRecords SecureWebSocketSplitRecords
#endif
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest,
                       MAYBE_SecureWebSocketSplitRecords) {
  // Launch a secure WebSocket server.
  ASSERT_TRUE(wss_server_.Start());

  NavigateToHTTPS("split_packet_check.html");

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, SendCloseFrameWhenTabIsClosed) {
  // Launch a WebSocket server.
  ASSERT_TRUE(ws_server_.Start());

  {
    // Create a new tab, establish a WebSocket connection and close the tab.
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<content::WebContents> new_tab =
        content::WebContents::Create(
            content::WebContents::CreateParams(tab->GetBrowserContext()));
    content::WebContents* raw_new_tab = new_tab.get();
    browser()->tab_strip_model()->AppendWebContents(std::move(new_tab), true);
    ASSERT_EQ(raw_new_tab, browser()->tab_strip_model()->GetWebContentsAt(1));

    content::TitleWatcher connected_title_watcher(raw_new_tab, u"CONNECTED");
    connected_title_watcher.AlsoWaitForTitle(u"CLOSED");
    NavigateToHTTP("connect_and_be_observed.html");
    const std::u16string result = connected_title_watcher.WaitAndGetTitle();
    EXPECT_TRUE(base::EqualsASCII(result, "CONNECTED"));

    content::WebContentsDestroyedWatcher destroyed_watcher(raw_new_tab);
    browser()->tab_strip_model()->CloseWebContentsAt(1, 0);
    destroyed_watcher.Wait();
  }

  NavigateToHTTP("close_observer.html");
  EXPECT_EQ("PASS", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, WebSocketBasicAuthInHTTPURL) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  // Open connect_check.html via HTTP with credentials in the URL.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("http");
  ui_test_utils::NavigateToURL(
      browser(),
      ws_server_.GetURLWithUserAndPassword("connect_check.html", "test", "test")
          .ReplaceComponents(replacements));

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

// TODO(crbug.com/1176880): Disabled on macOS because the WSS SpawnedTestServer
// does not support modern TLS on the macOS bots.
#if defined(OS_MAC)
#define MAYBE_WebSocketBasicAuthInHTTPSURL DISABLED_WebSocketBasicAuthInHTTPSURL
#else
#define MAYBE_WebSocketBasicAuthInHTTPSURL WebSocketBasicAuthInHTTPSURL
#endif
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest,
                       MAYBE_WebSocketBasicAuthInHTTPSURL) {
  // Launch a basic-auth-protected secure WebSocket server.
  wss_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(wss_server_.Start());

  // Open connect_check.html via HTTPS with credentials in the URL.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ui_test_utils::NavigateToURL(
      browser(),
      wss_server_.GetURLWithUserAndPassword(
                      "connect_check.html", "test", "test")
          .ReplaceComponents(replacements));

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

// This test verifies that login details entered by the user into the login
// prompt to authenticate the main page are re-used for WebSockets from the same
// origin.
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest,
                       ReuseMainPageBasicAuthCredentialsForWebSocket) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  content::NavigationController* navigation_controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  AutoLogin auto_login("test", "test", navigation_controller);

  WindowedAuthNeededObserver auth_needed_waiter(navigation_controller);
  NavigateToHTTP("connect_check.html");
  auth_needed_waiter.Wait();

  EXPECT_TRUE(auto_login.logged_in());
  EXPECT_EQ("PASS", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserConnectToTest,
                       WebSocketBasicAuthInWSURL) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  ConnectTo(ws_server_.GetURLWithUserAndPassword(
      "echo-with-no-extension", "test", "test"));

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserConnectToTest,
                       WebSocketBasicAuthInWSURLBadCreds) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  ConnectTo(ws_server_.GetURLWithUserAndPassword(
      "echo-with-no-extension", "wrong-user", "wrong-password"));

  EXPECT_EQ("FAIL", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserConnectToTest,
                       WebSocketBasicAuthNoCreds) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  ConnectTo(ws_server_.GetURL("echo-with-no-extension"));

  EXPECT_EQ("FAIL", WaitAndGetTitle());
}

// HTTPS connection limits should not be applied to wss:. This is only tested
// for secure connections here because the unencrypted case is tested in the
// Blink layout tests, and browser tests are expensive to run.
// TODO(crbug.com/1176880): Disabled on macOS because the WSS SpawnedTestServer
// does not support modern TLS on the macOS bots.
#if defined(OS_MAC)
#define MAYBE_SSLConnectionLimit DISABLED_SSLConnectionLimit
#else
#define MAYBE_SSLConnectionLimit SSLConnectionLimit
#endif
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, MAYBE_SSLConnectionLimit) {
  ASSERT_TRUE(wss_server_.Start());

  NavigateToHTTPS("multiple-connections.html");

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

// Regression test for crbug.com/903553005
// TODO(crbug.com/1176880): Disabled on macOS because the WSS SpawnedTestServer
// does not support modern TLS on the macOS bots.
#if defined(OS_MAC)
#define MAYBE_WebSocketAppliesHSTS DISABLED_WebSocketAppliesHSTS
#else
#define MAYBE_WebSocketAppliesHSTS WebSocketAppliesHSTS
#endif
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, MAYBE_WebSocketAppliesHSTS) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  net::SpawnedTestServer wss_server(
      net::SpawnedTestServer::TYPE_WSS,
      net::SpawnedTestServer::SSLOptions(
          net::SpawnedTestServer::SSLOptions::CERT_COMMON_NAME_IS_DOMAIN),
      net::GetWebSocketTestDataDirectory());
  // This test sets HSTS on localhost. To avoid being redirected to https, start
  // the http server on 127.0.0.1 instead.
  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  ASSERT_TRUE(http_server.Start());
  ASSERT_TRUE(wss_server.StartInBackground());

  // Set HSTS on localhost.
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), u"SET");
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("/websocket/set-hsts.html"));
  const std::u16string result = title_watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::EqualsASCII(result, "SET"));

  // Verify that it applies to WebSockets.
  ASSERT_TRUE(wss_server.BlockUntilStarted());
  GURL wss_url = wss_server.GetURL("echo-with-no-extension");
  std::string scheme("ws");
  GURL::Replacements scheme_replacement;
  scheme_replacement.SetSchemeStr(scheme);
  GURL ws_url = wss_url.ReplaceComponents(scheme_replacement);

  // An https: URL won't work here here because the mixed content policy
  // disallows connections to unencrypted WebSockets from encrypted pages.
  GURL http_url =
      http_server.GetURL("/websocket/check-hsts.html#" + ws_url.spec());

  ui_test_utils::NavigateToURL(browser(), http_url);

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

// An implementation of WebSocketClient that expects the mojo connection to be
// disconnected due to invalid UTF-8.
class ExpectInvalidUtf8Client : public network::mojom::WebSocketClient {
 public:
  ExpectInvalidUtf8Client(base::OnceClosure success_closure,
                          base::RepeatingClosure fail_closure)
      : success_closure_(std::move(success_closure)),
        fail_closure_(fail_closure) {}

  ~ExpectInvalidUtf8Client() override = default;

  ExpectInvalidUtf8Client(const ExpectInvalidUtf8Client&) = delete;
  ExpectInvalidUtf8Client& operator=(ExpectInvalidUtf8Client&) = delete;

  void Bind(mojo::PendingReceiver<network::mojom::WebSocketClient> receiver) {
    client_receiver_.Bind(std::move(receiver));
    // This use of base::Unretained is safe because the disconnect handler will
    // not be called after |client_receiver_| is destroyed.
    client_receiver_.set_disconnect_with_reason_handler(base::BindRepeating(
        &ExpectInvalidUtf8Client::OnDisconnect, base::Unretained(this)));
  }

  // Implementation of WebSocketClient
  void OnDataFrame(bool fin,
                   network::mojom::WebSocketMessageType,
                   uint64_t data_length) override {
    NOTREACHED();
  }

  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override {
    NOTREACHED();
  }

  void OnClosingHandshake() override { NOTREACHED(); }

 private:
  void OnDisconnect(uint32_t reason, const std::string& message) {
    if (message == "Browser sent a text frame containing invalid UTF-8") {
      std::move(success_closure_).Run();
    } else {
      ADD_FAILURE() << "Unexpected disconnect: reason=" << reason
                    << " message=\"" << message << '"';
      fail_closure_.Run();
    }
  }

  base::OnceClosure success_closure_;
  const base::RepeatingClosure fail_closure_;

  mojo::Receiver<network::mojom::WebSocketClient> client_receiver_{this};
};

// An implementation of WebSocketHandshakeClient that sends a text message
// containing invalid UTF-8 when the connection is established.
class InvalidUtf8HandshakeClient
    : public network::mojom::WebSocketHandshakeClient {
 public:
  InvalidUtf8HandshakeClient(std::unique_ptr<ExpectInvalidUtf8Client> client,
                             base::RepeatingClosure fail_closure)
      : client_(std::move(client)), fail_closure_(fail_closure) {}
  ~InvalidUtf8HandshakeClient() override = default;

  InvalidUtf8HandshakeClient(const InvalidUtf8HandshakeClient&) = delete;
  InvalidUtf8HandshakeClient& operator=(const InvalidUtf8HandshakeClient&) =
      delete;

  mojo::PendingRemote<network::mojom::WebSocketHandshakeClient> Bind() {
    auto pending_remote = handshake_client_receiver_.BindNewPipeAndPassRemote();
    // This use of base::Unretained is safe because the disconnect handler will
    // not be called after |handshake_client_receiver_| is destroyed.
    handshake_client_receiver_.set_disconnect_handler(
        base::BindOnce(&InvalidUtf8HandshakeClient::FailIfNotConnected,
                       base::Unretained(this)));
    return pending_remote;
  }

  // Implementation of WebSocketHandshakeClient
  void OnOpeningHandshakeStarted(
      network::mojom::WebSocketHandshakeRequestPtr) override {}

  void OnFailure(const std::string& message,
                 int net_error,
                 int response_code) override {}

  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::WebSocket> websocket,
      mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
      network::mojom::WebSocketHandshakeResponsePtr,
      mojo::ScopedDataPipeConsumerHandle readable,
      mojo::ScopedDataPipeProducerHandle writable) override {
    client_->Bind(std::move(client_receiver));
    websocket_.Bind(std::move(websocket));

    // Invalid UTF-8.
    static const uint32_t message[] = {0xff};
    uint32_t size = static_cast<uint32_t>(sizeof(message));

    websocket_->SendMessage(network::mojom::WebSocketMessageType::TEXT, size);

    EXPECT_EQ(writable->WriteData(message, &size, MOJO_WRITE_DATA_FLAG_NONE),
              MOJO_RESULT_OK);
    EXPECT_EQ(size, sizeof(message));

    connected_ = true;
  }

 private:
  void FailIfNotConnected() {
    if (!connected_) {
      fail_closure_.Run();
    }
  }

  const std::unique_ptr<ExpectInvalidUtf8Client> client_;
  const base::RepeatingClosure fail_closure_;
  bool connected_ = false;

  mojo::Receiver<network::mojom::WebSocketHandshakeClient>
      handshake_client_receiver_{this};
  mojo::Remote<network::mojom::WebSocket> websocket_;
};

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, SendBadUtf8) {
  ASSERT_TRUE(ws_server_.Start());

  base::RunLoop run_loop;
  bool failed = false;

  // This is a repeating closure for convenience so that we can use it in two
  // places.
  const base::RepeatingClosure fail_closure = base::BindLambdaForTesting([&]() {
    failed = true;
    run_loop.Quit();
  });

  auto client = std::make_unique<ExpectInvalidUtf8Client>(
      run_loop.QuitClosure(), fail_closure);
  auto handshake_client = std::make_unique<InvalidUtf8HandshakeClient>(
      std::move(client), fail_closure);

  MakeWebSocketConnection(ws_server_.GetURL("close"), handshake_client->Bind());

  run_loop.Run();

  EXPECT_FALSE(failed);
}

class FailureMonitoringHandshakeClient
    : public network::mojom::WebSocketHandshakeClient {
 public:
  struct Result {
    bool failure_reported = false;
    int net_error = -1;
    int response_code = -1;
  };

  explicit FailureMonitoringHandshakeClient(base::OnceClosure quit)
      : quit_(std::move(quit)) {}

  FailureMonitoringHandshakeClient(const FailureMonitoringHandshakeClient&) =
      delete;
  FailureMonitoringHandshakeClient& operator=(
      const FailureMonitoringHandshakeClient&) = delete;

  mojo::PendingRemote<network::mojom::WebSocketHandshakeClient> Bind() {
    CHECK(quit_);
    auto pending_remote = handshake_client_receiver_.BindNewPipeAndPassRemote();
    handshake_client_receiver_.set_disconnect_handler(std::move(quit_));
    return pending_remote;
  }

  const Result& result() const { return result_; }

  // Implementation of WebSocketHandshakeClient
  void OnOpeningHandshakeStarted(
      network::mojom::WebSocketHandshakeRequestPtr) override {}

  void OnFailure(const std::string& message,
                 int net_error,
                 int response_code) override {
    result_.failure_reported = true;
    result_.net_error = net_error;
    result_.response_code = response_code;
  }

  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::WebSocket> websocket,
      mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
      network::mojom::WebSocketHandshakeResponsePtr,
      mojo::ScopedDataPipeConsumerHandle readable,
      mojo::ScopedDataPipeProducerHandle writable) override {}

 private:
  Result result_;
  base::OnceClosure quit_;
  mojo::Receiver<network::mojom::WebSocketHandshakeClient>
      handshake_client_receiver_{this};
};

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, FailuresReported) {
  ASSERT_TRUE(ws_server_.Start());

  {
    // The OnFailure method should not be called for a successful connection.
    base::RunLoop run_loop;
    auto handshake_client = std::make_unique<FailureMonitoringHandshakeClient>(
        run_loop.QuitClosure());
    MakeWebSocketConnection(ws_server_.GetURL("echo-with-no-extension"),
                            handshake_client->Bind());
    run_loop.Run();
    EXPECT_FALSE(handshake_client->result().failure_reported);
  }

  {
    // If the server returns a 404 status, that should be surfaced via
    // OnFailure.
    base::RunLoop run_loop;
    auto handshake_client = std::make_unique<FailureMonitoringHandshakeClient>(
        run_loop.QuitClosure());
    MakeWebSocketConnection(ws_server_.GetURL("nonsensedoesntexist"),
                            handshake_client->Bind());
    run_loop.Run();
    EXPECT_TRUE(handshake_client->result().failure_reported);
    EXPECT_EQ(404, handshake_client->result().response_code);
  }
}

}  // namespace
