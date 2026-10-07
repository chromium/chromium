// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_mojo_proxy_resolver_linux.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/proxy_resolver/public/mojom/system_proxy_resolver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using proxy_resolver::mojom::LinuxProxyStatus;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

constexpr char kProxyResolverInterface[] =
    "org.freedesktop.portal.ProxyResolver";

using ResultFuture = base::test::TestFuture<
    const net::ProxyList&,
    proxy_resolver::mojom::SystemProxyResolutionStatusPtr>;

class ChromeMojoProxyResolverLinuxTest : public testing::Test {
 public:
  ChromeMojoProxyResolverLinuxTest()
      : mock_bus_(base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options())),
        mock_portal_proxy_(base::MakeRefCounted<dbus::MockObjectProxy>(
            mock_bus_.get(),
            dbus_xdg::kPortalServiceName,
            dbus::ObjectPath(dbus_xdg::kPortalObjectPath))) {
    ON_CALL(*mock_bus_, GetObjectProxy(dbus_xdg::kPortalServiceName, _))
        .WillByDefault(Return(mock_portal_proxy_.get()));
    ON_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillByDefault(
            Invoke([this](dbus::MethodCall* call, int timeout_ms,
                          dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              EXPECT_EQ(call->GetInterface(), kProxyResolverInterface);
              EXPECT_EQ(call->GetMember(), "Lookup");
              dbus::MessageReader reader(call);
              std::string uri;
              EXPECT_TRUE(reader.PopString(&uri));
              requested_uris_.push_back(uri);
              pending_callbacks_.push_back(std::move(callback));
            }));
  }

  ~ChromeMojoProxyResolverLinuxTest() override {
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
  }

 protected:
  void SetPortalAvailable(bool available) {
    dbus_xdg::SetPortalStateForTesting(
        available ? dbus_xdg::PortalRegistrarState::kSuccess
                  : dbus_xdg::PortalRegistrarState::kFailed);
  }

  // Replies to the oldest pending Lookup call with `proxies`.
  void RespondWithProxies(const std::vector<std::string>& proxies) {
    ASSERT_FALSE(pending_callbacks_.empty());
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendArrayOfStrings(proxies);
    auto callback = std::move(pending_callbacks_.front());
    pending_callbacks_.erase(pending_callbacks_.begin());
    std::move(callback).Run(response.get(), nullptr);
  }

  // Replies to the oldest pending Lookup call with a D-Bus error.
  void RespondWithError() {
    ASSERT_FALSE(pending_callbacks_.empty());
    dbus::MethodCall method_call(kProxyResolverInterface, "Lookup");
    method_call.SetSerial(1);
    auto error = dbus::ErrorResponse::FromMethodCall(
        &method_call, "org.freedesktop.DBus.Error.Failed", "Lookup failed");
    auto callback = std::move(pending_callbacks_.front());
    pending_callbacks_.erase(pending_callbacks_.begin());
    std::move(callback).Run(nullptr, error.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_portal_proxy_;
  std::vector<std::string> requested_uris_;
  std::vector<dbus::ObjectProxy::ResponseOrErrorCallback> pending_callbacks_;
};

// A missing FileChooser interface shouldn't prevent using ProxyResolver.
TEST_F(ChromeMojoProxyResolverLinuxTest, FileChooserUnavailable) {
  SetPortalAvailable(true);
  dbus_xdg::SetPortalInterfaceVersionForTesting(
      dbus_xdg::kFileChooserInterfaceName, 0);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("https://example.test/"), future.GetCallback());
  ASSERT_EQ(requested_uris_.size(), 1u);
  RespondWithProxies({"direct://"});
  EXPECT_TRUE(future.Get<1>()->is_success);
}

TEST_F(ChromeMojoProxyResolverLinuxTest, ProxyResolverUnavailable) {
  SetPortalAvailable(true);
  dbus_xdg::SetPortalInterfaceVersionForTesting(kProxyResolverInterface, 0);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("https://example.test/"), future.GetCallback());
  auto [proxy_list, status] = future.Take();
  EXPECT_TRUE(proxy_list.IsEmpty());
  EXPECT_FALSE(status->is_success);
  EXPECT_EQ(status->linux_proxy_status, LinuxProxyStatus::kPortalUnavailable);

  // Subsequent requests fail immediately too.
  ResultFuture future2;
  resolver.GetProxyForUrl(GURL("https://example.test/"), future2.GetCallback());
  EXPECT_EQ(future2.Get<1>()->linux_proxy_status,
            LinuxProxyStatus::kPortalUnavailable);
  EXPECT_TRUE(requested_uris_.empty());
}

TEST_F(ChromeMojoProxyResolverLinuxTest, PortalSetupFailed) {
  SetPortalAvailable(false);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("https://example.test/"), future.GetCallback());
  EXPECT_EQ(future.Get<1>()->linux_proxy_status,
            LinuxProxyStatus::kPortalUnavailable);
  EXPECT_TRUE(requested_uris_.empty());
}

// Availability is only checked once per resolver.
TEST_F(ChromeMojoProxyResolverLinuxTest, QueriesVersionOnce) {
  SetPortalAvailable(true);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future1;
  resolver.GetProxyForUrl(GURL("http://a.test/"), future1.GetCallback());

  // Changing the reported version after the first request has no effect.
  dbus_xdg::SetPortalInterfaceVersionForTesting(kProxyResolverInterface, 0);
  ResultFuture future2;
  resolver.GetProxyForUrl(GURL("http://b.test/"), future2.GetCallback());
  EXPECT_EQ(requested_uris_.size(), 2u);
}

TEST_F(ChromeMojoProxyResolverLinuxTest, ParsesProxies) {
  SetPortalAvailable(true);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("https://example.test/"), future.GetCallback());
  ASSERT_EQ(requested_uris_.size(), 1u);
  EXPECT_EQ(requested_uris_[0], "https://example.test/");

  RespondWithProxies({
      "http://proxy.test:8080",
      "https://secure.test:8443",
      "socks://socks.test:1080",
      "socks4://socks4.test:1081",
      "socks5://user:pass@socks5.test:1082",
      "direct://",
      "http://user@user-only.test:3129",
      "http://[::1]:3130",
      "socks5://user:@empty-password.test:1083",
  });

  auto [proxy_list, status] = future.Take();
  EXPECT_TRUE(status->is_success);
  EXPECT_EQ(status->os_error, 0);
  EXPECT_EQ(status->linux_proxy_status, LinuxProxyStatus::kOk);

  net::ProxyList expected;
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTP, "proxy.test", 8080));
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, "secure.test", 8443));
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_SOCKS5, "socks.test", 1080));
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_SOCKS4, "socks4.test", 1081));
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_SOCKS5, "socks5.test", 1082));
  expected.AddProxyChain(net::ProxyChain::Direct());
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTP, "user-only.test", 3129));
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTP, "[::1]", 3130));
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_SOCKS5, "empty-password.test", 1083));
  EXPECT_TRUE(expected.Equals(proxy_list))
      << "Expected: " << expected.ToDebugString()
      << " Actual: " << proxy_list.ToDebugString();
}

TEST_F(ChromeMojoProxyResolverLinuxTest, DirectOnly) {
  SetPortalAvailable(true);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("http://example.test/"), future.GetCallback());
  RespondWithProxies({"direct://"});

  auto [proxy_list, status] = future.Take();
  EXPECT_TRUE(status->is_success);
  EXPECT_EQ(proxy_list.size(), 1u);
  EXPECT_TRUE(proxy_list.First().is_direct());
}

TEST_F(ChromeMojoProxyResolverLinuxTest, IgnoresUnsupportedEntries) {
  SetPortalAvailable(true);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("http://example.test/"), future.GetCallback());
  RespondWithProxies(
      {"ftp://ftp.test:21", "garbage", "http://proxy.test:3128"});

  auto [proxy_list, status] = future.Take();
  EXPECT_TRUE(status->is_success);
  net::ProxyList expected;
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTP, "proxy.test", 3128));
  EXPECT_TRUE(expected.Equals(proxy_list));
}

// Only URIs of the form "protocol://[user[:password]@]host:port" and
// "direct://" are accepted.
TEST_F(ChromeMojoProxyResolverLinuxTest, RejectsMalformedUris) {
  SetPortalAvailable(true);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("http://example.test/"), future.GetCallback());
  RespondWithProxies({
      "http://proxy.test:8080/",
      "http://proxy.test:8080/path",
      "http://proxy.test:8080?query",
      "http://proxy.test:8080#fragment",
      "http://proxy.test:",
      "http://a@b@proxy.test:8080",
      "http://@proxy.test:8080",
      "http://:pass@proxy.test:8080",
      "http://user:pass:extra@proxy.test:8080",
      " http://proxy.test:8080",
      "http://proxy.test:8080 ",
      "http://proxy.test:8080\n",
      "://proxy.test:8080",
      "proxy.test:8080",
      "direct://proxy.test:8080",
      "DIRECT://",
      "quic://proxy.test:443",
      "socks5://valid.test:1080",
  });

  auto [proxy_list, status] = future.Take();
  EXPECT_TRUE(status->is_success);
  net::ProxyList expected;
  expected.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_SOCKS5, "valid.test", 1080));
  EXPECT_TRUE(expected.Equals(proxy_list))
      << "Expected: " << expected.ToDebugString()
      << " Actual: " << proxy_list.ToDebugString();
}

TEST_F(ChromeMojoProxyResolverLinuxTest, AllEntriesInvalid) {
  SetPortalAvailable(true);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("http://example.test/"), future.GetCallback());
  RespondWithProxies({"ftp://ftp.test:21", "garbage"});

  auto [proxy_list, status] = future.Take();
  EXPECT_FALSE(status->is_success);
  EXPECT_EQ(status->linux_proxy_status, LinuxProxyStatus::kInvalidResponse);
  EXPECT_TRUE(proxy_list.IsEmpty());
}

TEST_F(ChromeMojoProxyResolverLinuxTest, EmptyResponse) {
  SetPortalAvailable(true);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("http://example.test/"), future.GetCallback());
  RespondWithProxies({});

  auto [proxy_list, status] = future.Take();
  EXPECT_FALSE(status->is_success);
  EXPECT_EQ(status->linux_proxy_status, LinuxProxyStatus::kEmptyProxyList);
  EXPECT_TRUE(proxy_list.IsEmpty());
}

TEST_F(ChromeMojoProxyResolverLinuxTest, DBusError) {
  SetPortalAvailable(true);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future;
  resolver.GetProxyForUrl(GURL("http://example.test/"), future.GetCallback());
  RespondWithError();

  auto [proxy_list, status] = future.Take();
  EXPECT_FALSE(status->is_success);
  EXPECT_EQ(status->linux_proxy_status, LinuxProxyStatus::kDBusError);
  EXPECT_TRUE(proxy_list.IsEmpty());
}

TEST_F(ChromeMojoProxyResolverLinuxTest, ConcurrentRequests) {
  SetPortalAvailable(true);
  ChromeMojoProxyResolverLinux resolver(mock_bus_);

  ResultFuture future1;
  ResultFuture future2;
  resolver.GetProxyForUrl(GURL("http://a.test/"), future1.GetCallback());
  resolver.GetProxyForUrl(GURL("http://b.test/"), future2.GetCallback());
  ASSERT_EQ(requested_uris_.size(), 2u);
  EXPECT_EQ(requested_uris_[0], "http://a.test/");
  EXPECT_EQ(requested_uris_[1], "http://b.test/");

  RespondWithProxies({"http://proxy-a.test:80"});
  RespondWithProxies({"http://proxy-b.test:80"});

  EXPECT_EQ(future1.Get<0>().First().First().host_port_pair().host(),
            "proxy-a.test");
  EXPECT_EQ(future2.Get<0>().First().First().host_port_pair().host(),
            "proxy-b.test");
}

TEST_F(ChromeMojoProxyResolverLinuxTest, DestroyedWithPendingLookup) {
  SetPortalAvailable(true);
  auto resolver = std::make_unique<ChromeMojoProxyResolverLinux>(mock_bus_);

  bool called = false;
  resolver->GetProxyForUrl(
      GURL("http://example.test/"),
      base::BindOnce(
          [](bool* called, const net::ProxyList&,
             proxy_resolver::mojom::SystemProxyResolutionStatusPtr) {
            *called = true;
          },
          &called));
  resolver.reset();

  // The reply arriving after destruction must not crash or run the callback.
  RespondWithProxies({"http://proxy.test:8080"});
  EXPECT_FALSE(called);
}

}  // namespace
