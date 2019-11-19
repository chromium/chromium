// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/proxy_resolution_service_provider.h"

#include <memory>

#include "chromeos/dbus/services/service_provider_test_helper.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The parsed result from ProxyResolutionServiceProvider's D-Bus result (error
// is a string).
struct ResolveProxyResult {
  std::string error;
  std::string proxy_info;
};

// A mock result to configure what the NetworkContext should return (error is an
// integer).
struct LookupProxyForURLMockResult {
  net::Error error = net::ERR_UNEXPECTED;
  std::string proxy_info_pac_string;
};

// Mock NetworkContext that allows controlling the result of
// LookUpProxyForURL().
class MockNetworkContext : public network::TestNetworkContext {
 public:
  MockNetworkContext() {}
  ~MockNetworkContext() override {}

  // network::mojom::NetworkContext implementation:
  void LookUpProxyForURL(
      const GURL& url,
      mojo::PendingRemote<::network::mojom::ProxyLookupClient>
          proxy_lookup_client) override {
    mojo::Remote<::network::mojom::ProxyLookupClient>
        proxy_lookup_client_remote(std::move(proxy_lookup_client));
    if (lookup_proxy_result_.error == net::OK) {
      net::ProxyInfo proxy_info;
      proxy_info.UsePacString(lookup_proxy_result_.proxy_info_pac_string);
      proxy_lookup_client_remote->OnProxyLookupComplete(net::OK, proxy_info);
    } else {
      proxy_lookup_client_remote->OnProxyLookupComplete(
          lookup_proxy_result_.error, base::nullopt);
    }
  }

  void SetNextProxyResult(LookupProxyForURLMockResult mock_result) {
    lookup_proxy_result_ = mock_result;
  }

 private:
  LookupProxyForURLMockResult lookup_proxy_result_;

  DISALLOW_COPY_AND_ASSIGN(MockNetworkContext);
};

}  // namespace

class ProxyResolutionServiceProviderTest : public testing::Test {
 public:
  ProxyResolutionServiceProviderTest() {
    service_provider_ = std::make_unique<ProxyResolutionServiceProvider>();
    service_provider_->set_network_context_for_test(&mock_network_context_);

    test_helper_.SetUp(
        kNetworkProxyServiceName, dbus::ObjectPath(kNetworkProxyServicePath),
        kNetworkProxyServiceInterface, kNetworkProxyServiceResolveProxyMethod,
        service_provider_.get());
  }

  ~ProxyResolutionServiceProviderTest() override {
    test_helper_.TearDown();
  }

 protected:
  // Makes a D-Bus call to |service_provider_|'s ResolveProxy method and sets
  // the parsed response in |result|.
  void CallMethod(const std::string& source_url, ResolveProxyResult* result) {
    dbus::MethodCall method_call(kNetworkProxyServiceInterface,
                                 kNetworkProxyServiceResolveProxyMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(source_url);

    std::unique_ptr<dbus::Response> response =
        test_helper_.CallMethod(&method_call);

    // Parse the |dbus::Response|.
    ASSERT_TRUE(response);
    dbus::MessageReader reader(response.get());
    std::string proxy_info, error;
    EXPECT_TRUE(reader.PopString(&result->proxy_info));
    EXPECT_TRUE(reader.PopString(&result->error));
  }

  MockNetworkContext mock_network_context_;

  std::unique_ptr<ProxyResolutionServiceProvider> service_provider_;
  ServiceProviderTestHelper test_helper_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolutionServiceProviderTest);
};

// Tests the normal success case. The proxy resolver returns a single proxy.
TEST_F(ProxyResolutionServiceProviderTest, Success) {
  mock_network_context_.SetNextProxyResult({net::OK, "PROXY localhost:8080"});

  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result);

  // The response should contain the proxy info and an empty error.
  EXPECT_EQ("PROXY localhost:8080", result.proxy_info);
  EXPECT_EQ("", result.error);
}

// Tests the case where the proxy resolver fails with
// ERR_MANDATORY_PROXY_CONFIGURATION_FAILED.
TEST_F(ProxyResolutionServiceProviderTest, ResolverFailed) {
  mock_network_context_.SetNextProxyResult(
      {net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED, "PROXY localhost:8080"});

  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result);

  // The response should contain empty proxy info and a "mandatory proxy config
  // failed" error (which the error from the resolver will be mapped to).
  EXPECT_EQ("DIRECT", result.proxy_info);
  EXPECT_EQ(net::ErrorToString(net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED),
            result.error);
}

// Tests calling the proxy resolution provider with an invalid URL.
TEST_F(ProxyResolutionServiceProviderTest, BadURL) {
  ResolveProxyResult result;
  CallMethod(":bad-url", &result);

  // The response should contain empty proxy info and a "mandatory proxy config
  // failed" error (which the error from the resolver will be mapped to).
  EXPECT_EQ("DIRECT", result.proxy_info);
  EXPECT_EQ("Invalid URL", result.error);
}

// Tests the failure case where a NetworkContext cannot be retrieved. This could
// happen at certain points during startup/shutdown while the primary profile is
// null.
TEST_F(ProxyResolutionServiceProviderTest, NullNetworkContext) {
  service_provider_->set_network_context_for_test(nullptr);

  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result);

  // The response should contain a failure.
  EXPECT_EQ("DIRECT", result.proxy_info);
  EXPECT_EQ("No NetworkContext", result.error);
}

}  // namespace chromeos
