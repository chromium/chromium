// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "url/gurl.h"

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
    watcher_.reset(new content::TitleWatcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16("PASS")));
    watcher_->AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));
  }

  void TearDownOnMainThread() override { watcher_.reset(); }

  std::string WaitAndGetTitle() {
    return base::UTF16ToUTF8(watcher_->WaitAndGetTitle());
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
  const base::string16 username_;
  const base::string16 password_;
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

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, SecureWebSocketSplitRecords) {
  // Launch a secure WebSocket server.
  ASSERT_TRUE(wss_server_.Start());

  NavigateToHTTPS("split_packet_check.html");

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

// Flaky failing on Win10 only.  http://crbug.com/616958
#if defined(OS_WIN)
#define MAYBE_SendCloseFrameWhenTabIsClosed \
    DISABLED_SendCloseFrameWhenTabIsClosed
#else
#define MAYBE_SendCloseFrameWhenTabIsClosed SendCloseFrameWhenTabIsClosed
#endif

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest,
                       MAYBE_SendCloseFrameWhenTabIsClosed) {
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

    content::TitleWatcher connected_title_watcher(
        raw_new_tab, base::ASCIIToUTF16("CONNECTED"));
    connected_title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("CLOSED"));
    NavigateToHTTP("connect_and_be_observed.html");
    const base::string16 result = connected_title_watcher.WaitAndGetTitle();
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

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, WebSocketBasicAuthInHTTPSURL) {
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
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, SSLConnectionLimit) {
  ASSERT_TRUE(wss_server_.Start());

  NavigateToHTTPS("multiple-connections.html");

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

// Regression test for crbug.com/903553005
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, WebSocketAppliesHSTS) {
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
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::ASCIIToUTF16("SET"));
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("/websocket/set-hsts.html"));
  const base::string16 result = title_watcher.WaitAndGetTitle();
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

}  // namespace
