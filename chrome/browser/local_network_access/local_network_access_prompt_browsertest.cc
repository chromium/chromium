// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/mock_host_resolver.h"
#include "net/socket/tcp_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Local Network Access browsertests testing showing the prompt when connections
// aren't always established.

namespace local_network_access {

namespace {
// Path to a response that passes Local Network Access checks.
constexpr char kLnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *";

constexpr char kHostA[] = "a.test";
constexpr char kHostB[] = "b.test";
constexpr char kHostC[] = "c.test";
constexpr char kHostLocal[] = "resolve.local";

}  // namespace

class LocalNetworkAccessPromptBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 public:
  LocalNetworkAccessPromptBrowserTest()
      : LocalNetworkAccessBrowserTestBase(false) {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule(kHostA, "127.0.0.1");
    host_resolver()->AddRule(kHostB, "127.0.0.1");
    host_resolver()->AddRule(kHostLocal, "127.0.0.1");
    LocalNetworkAccessBrowserTestBase::SetUpOnMainThread();

    ASSERT_TRUE(content::NavigateToURL(
        web_contents(),
        https_server().GetURL(
            kHostA,
            "/local_network_access/no-favicon-treat-as-public-address.html")));

    // Enable auto-accept of LNA permission request; this ensures that if the
    // connection is made the fetch() will succeed. In most of the tests below,
    // we want to ensure the connection isn't made but the permission prompt is
    // still shown; having the permission be accepted makes it so that we do not
    // have to distinguish in the test between connection failure and connection
    // succeeded but LNA permission was denied.
    bubble_factory()->set_response_type(
        permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

    // Get a socket bound to a port, but don't listen to it. This lets us have a
    // port that will deterministically not accept any connections.
    // TCPSocket is used directly instead of TCPServerSocket because the latter
    // only has a Listen function; binding without listening has connection
    // attempts rejected faster.
    ASSERT_EQ(net::OK, socket_->Open(net::AddressFamily::ADDRESS_FAMILY_IPV4));
    ASSERT_EQ(net::OK, socket_->Bind(net::IPEndPoint(
                           net::IPAddress::IPv4Localhost(), 0)));
    net::IPEndPoint address;
    ASSERT_EQ(net::OK, socket_->GetLocalAddress(&address));
    port_ = base::NumberToString(address.port());
  }

  // Get a URL that will not have anything listening on it.
  GURL GetUnconnectedURL(std::string_view host, std::string_view relative_url) {
    GURL::Replacements replace_port;
    replace_port.SetPortStr(port_);
    return https_server()
        .GetURL(host, relative_url)
        .ReplaceComponents(replace_port);
  }

  std::string FetchUrlJs(const GURL& gurl) {
    return content::JsReplace("fetch($1).then(response => response.ok)", gurl);
  }

  std::unique_ptr<net::TCPSocket> socket_ =
      net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
  std::string port_;
};

// Baseline test; connection established, prompt shown, request denied.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPromptBrowserTest,
                       FetchPermissionPromptShown) {
  permissions::PermissionRequestObserver permission_request_observer(
      web_contents());

  // LNA fetch should succeed.
  ASSERT_EQ(true, content::EvalJs(
                      web_contents(),
                      FetchUrlJs(https_server().GetURL(kHostB, kLnaPath))));

  permission_request_observer.Wait();
  EXPECT_TRUE(permission_request_observer.request_shown());
}

// Connection failed, permission prompt not shown because host was not connected
// to and was not known from the hostname to be a local/loopback host.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPromptBrowserTest,
                       FetchPermissionPromptNotShown) {
  permissions::PermissionRequestObserver permission_request_observer(
      web_contents());

  // LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(web_contents(), FetchUrlJs(https_server().GetURL(
                                                  kHostC, kLnaPath))),
              content::EvalJsResult::IsError());

  // Permission prompt not shown, c.com doesn't resolve so no connection is
  // made, and c.com is not a host that is always local/loopback.
  EXPECT_FALSE(permission_request_observer.request_shown());
}

// Connection failed, permission prompt shown because host is a .local domain
// that resolves.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPromptBrowserTest,
                       FetchPermissionPromptShownLocal) {
  permissions::PermissionRequestObserver permission_request_observer(
      web_contents());

  // LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(web_contents(), FetchUrlJs(GetUnconnectedURL(
                                                  kHostLocal, kLnaPath))),
              content::EvalJsResult::IsError());

  permission_request_observer.Wait();
  EXPECT_TRUE(permission_request_observer.request_shown());
}

// Connection failed, permission prompt shown because host is localhost.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPromptBrowserTest,
                       FetchPermissionPromptShownLocalhost) {
  permissions::PermissionRequestObserver permission_request_observer(
      web_contents());

  // LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(web_contents(), FetchUrlJs(GetUnconnectedURL(
                                                  "localhost", kLnaPath))),
              content::EvalJsResult::IsError());

  permission_request_observer.Wait();
  EXPECT_TRUE(permission_request_observer.request_shown());
}

// Connection failed, permission prompt shown because host is an IP literal that
// maps to a loopback address.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPromptBrowserTest,
                       FetchPermissionPromptShownIpLiteral) {
  permissions::PermissionRequestObserver permission_request_observer(
      web_contents());

  // LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(web_contents(), FetchUrlJs(GetUnconnectedURL(
                                                  "127.0.0.1", kLnaPath))),
              content::EvalJsResult::IsError());

  permission_request_observer.Wait();
  EXPECT_TRUE(permission_request_observer.request_shown());
}

// Connection failed, permission prompt shown because host is an IPv6 literal
// that maps to a loopback address.
IN_PROC_BROWSER_TEST_F(LocalNetworkAccessPromptBrowserTest,
                       FetchPermissionPromptShownIpv6Literal) {
  permissions::PermissionRequestObserver permission_request_observer(
      web_contents());

  // LNA fetch should fail.
  EXPECT_THAT(content::EvalJs(web_contents(),
                              FetchUrlJs(GetUnconnectedURL("[::1]", kLnaPath))),
              content::EvalJsResult::IsError());

  permission_request_observer.Wait();
  EXPECT_TRUE(permission_request_observer.request_shown());
}

}  // namespace local_network_access
